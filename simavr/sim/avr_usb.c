/* vim: set sts=4:sw=4:ts=4:noexpandtab
	avr_usb.c

	Copyright 2012 Torbjorn Tyridal <ttyridal@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * USB device controller of the AT90USB82/162, ATmega8U2/16U2/32U2 and
 * ATmega16U4/32U4. The register layout is common, the cores describe the
 * endpoint geometry and the optional VBUS pad (see avr_usb_t).
 *
 * Interrupts are level driven like on the real part: the USB_COM and
 * USB_GEN conditions are re-evaluated whenever a flag or an enable bit
 * changes, so enabling an interrupt whose flag is already set fires it.
 */

/* TODO suspend/resume, SOFs never stop while attached */
/* TODO otg support? */
/* TODO drop bitfields? */
/* TODO thread safe ioctls */
/* TODO dual-bank endpoint buffers, banks are allocated but one is used */
/* TODO data toggle sequencing, DTSEQ only follows the resets */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "avr_usb.h"
#include "sim_time.h"

enum usb_regs
{
	usbcon = 0,
	usbsta = 1,
	usbint = 2,
	udcon = 8,
	udint = 9,
	udien = 10,
	udaddr = 11,
	udfnuml = 12,
	udfnumh = 13,
	udmfn = 14,
//     _res=15,
	ueintx = 16,
	uenum = 17,
	uerst = 18,
	ueconx = 19,
	uecfg0x = 20,
	uecfg1x = 21,
	uesta0x = 22,
	uesta1x = 23,
	ueienx = 24,
	uedatx = 25,
	uebclx = 26,
	uebchx = 27,	// ATmega16U4/32U4 only, reserved (reads 0) elsewhere
	ueint = 28,
	otgtcon = 29,
};

// register bits
#define USBCON_USBE		(1 << 7)
#define USBCON_FRZCLK	(1 << 5)
#define UDCON_DETACH	(1 << 0)
#define UDADDR_ADDEN	(1 << 7)
#define UECONX_EPEN		(1 << 0)
#define UECONX_RSTDT	(1 << 3)
#define UECONX_STALLRQC	(1 << 4)
#define UECONX_STALLRQ	(1 << 5)

// UEINTX flags that can raise an interrupt (all but RWAL and FIFOCON)
#define UEINTX_INT_MASK	0x5f
// UESTA0X OVERFI/UNDERFI, enabled by UEIENX.FLERRE
#define UESTA0X_ERR_MASK	0x60
// UDINT flags, UDIEN has the same layout
#define UDINT_INT_MASK	0x7f

union _ueintx {
	struct {
		uint8_t txini :1;
		uint8_t stalledi :1;
		uint8_t rxouti :1;
		uint8_t rxstpi :1;
		uint8_t nakouti :1;
		uint8_t rwal :1;
		uint8_t nakini :1;
		uint8_t fifocon :1;
	};
	uint8_t v;
};

struct _epstate {
	union _ueintx ueintx;
	union {
		struct {
			uint8_t epen :1;
			uint8_t res :2;
			uint8_t rstdt :1;
			uint8_t stallrqc :1;
			uint8_t stallrq :1;
		};
		uint8_t v;
	} ueconx;
	union {
		struct {
			uint8_t epdir :1;
			uint8_t res :5;
			uint8_t eptype :2;
		};
		uint8_t v;
	} uecfg0x;
	union {
		struct {
			uint8_t res :1;
			uint8_t alloc :1;
			uint8_t epbk :2;
			uint8_t epsize :3;
			uint8_t res2 :1;
		};
		uint8_t v;
	} uecfg1x;
	union {
		struct {
			uint8_t nbusybk :2;
			uint8_t dtseq :2;
			uint8_t res :1;
			uint8_t underfi :1;
			uint8_t overfi :1;
			uint8_t cfgok :1;
		};
		uint8_t v;
	} uesta0x;
	union {
		struct {
			uint8_t curbk :2;
			uint8_t ctrldir :1;
			uint8_t res :5;
		};
		uint8_t v;
	} uesta1x;
	union {
		struct {
			uint8_t txine :1;
			uint8_t stallede :1;
			uint8_t rxoute :1;
			uint8_t rxstpe :1;
			uint8_t nakoute :1;
			uint8_t res :1;
			uint8_t nakine :1;
			uint8_t flerre :1;
		};
		uint8_t v;
	} ueienx;

	struct {
		uint8_t bytes[256];
		uint16_t tail;
	} bank[2];
	uint8_t current_bank;
};

struct usb_internal_state {
	struct _epstate ep_state[AVR_USB_MAX_ENDPOINTS];
	avr_int_vector_t com_vect;
	avr_int_vector_t gen_vect;
	uint8_t vbus_level;		// VBUS as driven by the host
	uint8_t attached;		// last value sent on USB_IRQ_ATTACH
	uint8_t bad_epnum_warned;
};

/*
 * Part description helpers, zero fields fall back to the AT90USB162
 * layout the module was written for.
 */
static uint8_t
ep_count(
		avr_usb_t * p)
{
	return p->ep_count ? p->ep_count : 5;
}

static uint16_t
ep_size_max(
		avr_usb_t * p,
		uint8_t ep)
{
	return p->ep_size_max[ep] ? p->ep_size_max[ep] : 64;
}

static uint8_t
ep_banks_max(
		avr_usb_t * p,
		uint8_t ep)
{
	if (p->ep_banks_max[ep])
		return p->ep_banks_max[ep];
	return ep ? 2 : 1;
}

static uint8_t
current_ep_to_cpu(
		avr_usb_t * p)
{
	return p->io.avr->data[p->r_usbcon + uenum] & (AVR_USB_MAX_ENDPOINTS - 1);
}

/*
 * EPNUM values past the last endpoint select a dummy state that is never
 * enabled, so a bad UENUM write does not stop the simulation.
 */
static struct _epstate *
get_epstate(
		avr_usb_t * p,
		uint8_t ep)
{
	assert(ep < AVR_USB_MAX_ENDPOINTS);
	return &p->state->ep_state[ep];
}

static uint8_t
ep_int_pending(
		struct _epstate * epstate)
{
	if (!epstate->ueconx.epen)
		return 0;
	if (epstate->ueintx.v & epstate->ueienx.v & UEINTX_INT_MASK)
		return 1;
	return epstate->ueienx.flerre && (epstate->uesta0x.v & UESTA0X_ERR_MASK);
}

// UEINT: one EPINTx bit per endpoint with an enabled, raised flag
static uint8_t
ueint_value(
		avr_usb_t * p)
{
	uint8_t v = 0;

	for (int i = 0; i < ep_count(p); i++)
		if (ep_int_pending(get_epstate(p, i)))
			v |= 1 << i;
	return v;
}

static void
update_com_irq(
		avr_usb_t * p)
{
	if (ueint_value(p))
		avr_raise_interrupt(p->io.avr, &p->state->com_vect);
}

static void
update_gen_irq(
		avr_usb_t * p)
{
	avr_t * avr = p->io.avr;
	uint8_t pending = avr->data[p->r_usbcon + udint] &
			avr->data[p->r_usbcon + udien] & UDINT_INT_MASK;

	if (p->vbusti.reg &&
			avr_regbit_get(avr, p->vbusti) && avr_regbit_get(avr, p->vbuste))
		pending = 1;
	if (pending)
		avr_raise_interrupt(avr, &p->state->gen_vect);
}

/*
 * USB_IRQ_ATTACH follows the D+ pull-up: USBE set, DETACH clear and, on
 * parts with a VBUS pad, VBUS detected.
 */
static void
update_attach(
		avr_usb_t * p)
{
	avr_t * avr = p->io.avr;
	uint8_t attached = (avr->data[p->r_usbcon + usbcon] & USBCON_USBE) &&
			!(avr->data[p->r_usbcon + udcon] & UDCON_DETACH);

	if (p->vbus.reg && !avr_regbit_get(avr, p->vbus))
		attached = 0;
	if (attached != p->state->attached) {
		p->state->attached = attached;
		AVR_LOG(avr, LOG_TRACE, "USB: %s\n", attached ? "attach" : "detach");
		avr_raise_irq(p->io.irq + USB_IRQ_ATTACH, attached);
	}
}

/*
 * USBSTA.VBUS tracks the host VBUS level while the pad is enabled
 * (OTGPADE), every change of it sets VBUSTI.
 */
static void
update_vbus(
		avr_usb_t * p)
{
	avr_t * avr = p->io.avr;

	if (!p->vbus.reg)
		return;

	uint8_t level = avr_regbit_get(avr, p->otgpade) && p->state->vbus_level;
	if (level != avr_regbit_get(avr, p->vbus)) {
		avr_regbit_setto(avr, p->vbus, level);
		avr_regbit_set(avr, p->vbusti);
		AVR_LOG(avr, LOG_TRACE, "USB: VBUS %d\n", level);
	}
}

enum epints {
	txini = 0,
	stalledi = 1,
	rxouti = 2,
	rxstpi = 3,
	nakouti = 4,
	nakini = 6,
	overfi = 10,
	underfi = 11,
};

static void
raise_ep_interrupt(
        struct avr_t * avr,
        avr_usb_t * p,
        uint8_t ep,
        enum epints irq)
{
	struct _epstate * epstate = get_epstate(p, ep);

	switch (irq) {
		case txini:
		case stalledi:
		case rxouti:
		case nakouti:
		case nakini:
		case rxstpi:
			epstate->ueintx.v |= 1 << irq;
			break;
		case overfi:
			epstate->uesta0x.overfi = 1;
			break;
		case underfi:
			epstate->uesta0x.underfi = 1;
			break;
		default:
			assert(0);
	}
	update_com_irq(p);
}

enum usbints {
	suspi = 0, msofi = 1, sofi = 2, eorsti = 3, wakeupi = 4, eorsmi = 5,
	uprsmi = 6
};

static void
raise_usb_interrupt(
		avr_usb_t * p,
		enum usbints irq)
{
	uint8_t * Rudint = &p->io.avr->data[p->r_usbcon + udint];

	switch (irq) {
		case uprsmi:
		case eorsmi:
		case wakeupi:
		case eorsti:
		case sofi:
		case msofi:
		case suspi:
			*Rudint |= 1 << irq;
			break;
		default:
			assert(0);
	}
	update_gen_irq(p);
}

static uint16_t
ep_fifo_size(
		struct _epstate * epstate)
{
	return 8 << epstate->uecfg1x.epsize;
}

static int
ep_fifo_empty(
		struct _epstate * epstate)
{
	return epstate->bank[epstate->current_bank].tail == 0;
}

static int
ep_fifo_full(
		struct _epstate * epstate)
{
	return epstate->bank[epstate->current_bank].tail >= ep_fifo_size(epstate);
}

static uint16_t
ep_fifo_count(
		struct _epstate * epstate)
{
	return epstate->bank[epstate->current_bank].tail;
}

/*
 * CFGOK check: size and bank count within what the endpoint supports and,
 * when the part declares its DPRAM size, the endpoints allocated so far
 * (EP0 up to this one, the allocation order) fitting in it.
 */
static int
ep_config_valid(
		avr_usb_t * p,
		uint8_t ep)
{
	struct _epstate * epstate = get_epstate(p, ep);
	uint32_t used = 0;

	if (ep >= ep_count(p))
		return 0;
	if (epstate->uecfg1x.epsize > 5 || epstate->uecfg1x.epbk > 1)
		return 0;
	if (ep_fifo_size(epstate) > ep_size_max(p, ep) ||
			epstate->uecfg1x.epbk + 1 > ep_banks_max(p, ep))
		return 0;
	if (!p->dpram_size)
		return 1;
	for (int i = 0; i <= ep; i++) {
		struct _epstate * e = get_epstate(p, i);
		if (e->uecfg1x.alloc && e->uecfg1x.epsize <= 5 && e->uecfg1x.epbk <= 1)
			used += ep_fifo_size(e) * (e->uecfg1x.epbk + 1);
	}
	return used <= p->dpram_size;
}

// UEINTX state of an empty, configured endpoint
static void
ep_init_flags(
		struct _epstate * epstate)
{
	if (epstate->uecfg0x.eptype == 0)
		epstate->ueintx.txini = 1;
	else if (epstate->uecfg0x.epdir) {
		epstate->ueintx.txini = 1;
		epstate->ueintx.rwal = 1;
		epstate->ueintx.fifocon = 1;
	} else
		epstate->ueintx.rxouti = 0;
}

/*
 * Endpoint reset (UERST, EPEN cleared, USB reset of EP0): banks emptied,
 * UEINTX/UESTA0X/UESTA1X back to their reset values. The configuration
 * (and so CFGOK) and the data toggle are kept.
 */
static void
ep_reset(
		struct _epstate * epstate)
{
	uint8_t keep = epstate->uesta0x.v & 0x8c;	// CFGOK, DTSEQ

	epstate->bank[0].tail = 0;
	epstate->bank[1].tail = 0;
	epstate->current_bank = 0;
	epstate->ueintx.v = 0;
	epstate->uesta0x.v = keep;
	epstate->uesta1x.v = 0;
	if (epstate->ueconx.epen && epstate->uecfg1x.alloc)
		ep_init_flags(epstate);
}

static int
ep_fifo_cpu_readbyte(
		avr_usb_t * p,
		struct _epstate * epstate)
{
	uint16_t i, j;
	uint8_t v = epstate->bank[epstate->current_bank].bytes[0];

	if (!epstate->ueconx.epen) {
		AVR_LOG(p->io.avr, LOG_WARNING,
				"USB: Reading bytes from non configured endpoint\n");
		return -1;
	}

	if (ep_fifo_empty(epstate))
		return -2;

	for (i = 0, j = ep_fifo_count(epstate) - 1; i < j; i++)
		epstate->bank[epstate->current_bank].bytes[i] =
		        epstate->bank[epstate->current_bank].bytes[i + 1];
	epstate->bank[epstate->current_bank].tail--;
	return v;
}

static int
ep_fifo_cpu_writebyte(
		avr_usb_t * p,
		struct _epstate * epstate,
		uint8_t v)
{
	if (!epstate->ueconx.epen) {
		AVR_LOG(p->io.avr, LOG_WARNING,
				"USB: Adding bytes to non configured endpoint\n");
		return -1;
	}
	if (ep_fifo_full(epstate))
		return -2;

	epstate->bank[epstate->current_bank].bytes[epstate->bank[epstate->current_bank].tail++] = v;
	return 0;
}

static int
ep_fifo_usb_read(
		avr_usb_t * p,
		struct _epstate * epstate,
		uint8_t * buf,
		uint32_t max)
{
	if (epstate->ueintx.txini) {
		return AVR_IOCTL_USB_NAK;
	}
	if (epstate->ueintx.fifocon && epstate->uecfg0x.eptype != 0) {
		return AVR_IOCTL_USB_NAK;
	}

	int ret = epstate->bank[epstate->current_bank].tail;
	if ((uint32_t)ret > max) {
		AVR_LOG(p->io.avr, LOG_WARNING,
				"USB: IN packet of %d bytes, host buffer is %u, truncated\n",
				ret, max);
		ret = max;
	}
	memcpy(buf, epstate->bank[epstate->current_bank].bytes, ret);
	epstate->bank[epstate->current_bank].tail = 0;
	return ret;
}

static int
ep_fifo_usb_write(
		avr_usb_t * p,
		uint8_t ep,
		uint8_t * buf,
		uint32_t len)
{
	struct _epstate * epstate = get_epstate(p, ep);

	if (epstate->ueintx.rxouti) {
		return AVR_IOCTL_USB_NAK;
	}
	if (epstate->ueintx.fifocon && epstate->uecfg0x.eptype != 0) {
		return AVR_IOCTL_USB_NAK;
	}

	if (len > ep_fifo_size(epstate)) {
		AVR_LOG(p->io.avr, LOG_WARNING,
				"USB: EP%d OUT packet of %u bytes, endpoint size is %d\n",
				ep, len, ep_fifo_size(epstate));
		len = ep_fifo_size(epstate);
		raise_ep_interrupt(p->io.avr, p, ep, overfi);
	}
	memcpy(epstate->bank[epstate->current_bank].bytes, buf, len);
	epstate->bank[epstate->current_bank].tail = len;

	return 0;
}

static uint8_t
avr_usb_ep_read_bytecount(
        struct avr_t * avr,
        avr_io_addr_t addr,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	uint16_t count = ep_fifo_count(get_epstate(p, current_ep_to_cpu(p)));

	return addr == p->r_usbcon + uebclx ? count & 0xff : count >> 8;
}

static uint8_t
avr_usb_ueint_read(
        struct avr_t * avr,
        avr_io_addr_t addr,
        void * param)
{
	return ueint_value((avr_usb_t *) param);
}

static void
avr_usb_usbcon_write(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param);

static void
avr_usb_udaddr_write(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	if (v & UDADDR_ADDEN)
		AVR_LOG(avr, LOG_TRACE, "USB: Activate address %d\n", v & 0x7f);
	avr_core_watch_write(avr, addr, v);
}

static void
avr_usb_udcon_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_usb_t * p = (avr_usb_t *)param;

	avr_core_watch_write(avr, addr, v);
	update_attach(p);
}

// UDINT flags are cleared by writing 0, writing 1 has no effect
static void
avr_usb_udint_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_usb_t * p = (avr_usb_t *)param;

	avr_core_watch_write(avr, addr, avr->data[addr] & v);
	update_gen_irq(p);
}

static void
avr_usb_udien_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_usb_t * p = (avr_usb_t *)param;

	avr_core_watch_write(avr, addr, v);
	update_gen_irq(p);
}

// USBSTA is read only
static void
avr_usb_usbsta_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
}

// USBINT.VBUSTI is cleared by writing 0
static void
avr_usb_usbint_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_usb_t * p = (avr_usb_t *)param;

	avr_core_watch_write(avr, addr, avr->data[addr] & v);
	update_gen_irq(p);
}

static void
avr_usb_uenum_write(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;

	v &= AVR_USB_MAX_ENDPOINTS - 1;
	if (v >= ep_count(p) && !p->state->bad_epnum_warned) {
		AVR_LOG(avr, LOG_WARNING,
				"USB: UENUM=%d, this part has endpoints 0..%d\n",
				v, ep_count(p) - 1);
		p->state->bad_epnum_warned = 1;
	}
	avr_core_watch_write(avr, addr, v);
}

static void
avr_usb_uerst_write(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;

	v &= (1 << ep_count(p)) - 1;
	for (int i = 0; i < ep_count(p); i++)
		if (v & (1 << i))
			ep_reset(get_epstate(p, i));
	avr_core_watch_write(avr, addr, v);
}

static uint8_t
avr_usb_ep_read_ueintx(
        struct avr_t * avr,
        avr_io_addr_t addr,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	struct _epstate * epstate = get_epstate(p, current_ep_to_cpu(p));

	if (epstate->uecfg0x.epdir)
		epstate->ueintx.rwal = !ep_fifo_full(epstate);
	else
		epstate->ueintx.rwal = !ep_fifo_empty(epstate);

	return epstate->ueintx.v;
}

static void
avr_usb_ep_write_ueintx(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	uint8_t ep = current_ep_to_cpu(p);

	union _ueintx * newstate = (union _ueintx*) &v;
	union _ueintx * curstate = &get_epstate(p, ep)->ueintx;

	if (curstate->rxouti & !newstate->rxouti)
		curstate->rxouti = 0;
	if (curstate->txini & !newstate->txini)
		curstate->txini = 0;
	if (curstate->rxstpi & !newstate->rxstpi) {
		curstate->txini = 1;
		curstate->rxouti = 0;
		curstate->rxstpi = 0;
	}
	if (curstate->fifocon & !newstate->fifocon)
		curstate->fifocon = 0;
	if (curstate->nakini & !newstate->nakini)
		curstate->nakini = 0;
	if (curstate->nakouti & !newstate->nakouti)
		curstate->nakouti = 0;
	if (curstate->stalledi & !newstate->stalledi)
		curstate->stalledi = 0;
	if (curstate->rwal & !newstate->rwal)
		AVR_LOG(avr, LOG_WARNING, "USB: Pointless change of ueintx.rwal\n");

	update_com_irq(p);
}

static uint8_t
avr_usb_ep_read(
        struct avr_t * avr,
        avr_io_addr_t addr,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	uint8_t laddr = addr - p->r_usbcon;
	uint8_t v = 0;
	struct _epstate * epstate = get_epstate(p, current_ep_to_cpu(p));

	switch(laddr) {
		case ueconx:  v = epstate->ueconx.v; break;
		case uecfg0x: v = epstate->uecfg0x.v; break;
		case uecfg1x: v = epstate->uecfg1x.v; break;
		case uesta0x: v = epstate->uesta0x.v; break;
		case uesta1x: v = epstate->uesta1x.v; break;
		case ueienx:  v = epstate->ueienx.v; break;
		default:assert(0);
	}
	return v;
}

static void
avr_usb_ep_write(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	uint8_t ep = current_ep_to_cpu(p);
	struct _epstate * epstate = get_epstate(p, ep);
	uint8_t laddr = addr - p->r_usbcon;

	if (ep >= ep_count(p))
		return;

	switch (laddr) {
		case ueconx: {
			uint8_t was_enabled = epstate->ueconx.epen;
			// STALLRQ and STALLRQC are strobes, a read-modify-write
			// "UECONX |= STALLRQC" carries STALLRQ too, so clear wins.
			if (v & UECONX_STALLRQ)
				epstate->ueconx.stallrq = 1;
			if (v & UECONX_STALLRQC)
				epstate->ueconx.stallrq = 0;
			if (v & UECONX_RSTDT)
				epstate->uesta0x.dtseq = 0;
			epstate->ueconx.epen = (v & UECONX_EPEN) != 0;
			if (was_enabled && !epstate->ueconx.epen) {
				ep_reset(epstate);
				epstate->uesta0x.dtseq = 0;
			}
		}	break;
		case uecfg0x:
			epstate->uecfg0x.v = v;
			epstate->uesta0x.cfgok = 0;
			break;
		case uecfg1x:
			epstate->uecfg1x.v = v;
			epstate->uesta0x.cfgok = epstate->uecfg1x.alloc &&
					ep_config_valid(p, ep);
			if (epstate->uecfg1x.alloc && !epstate->uesta0x.cfgok)
				AVR_LOG(avr, LOG_WARNING,
						"USB: EP%d configuration rejected (UECFG1X=%02x)\n",
						ep, v);
			ep_init_flags(epstate);
			break;
		case uesta0x:
			// only OVERFI and UNDERFI can be cleared
			v = (epstate->uesta0x.v & 0x9f) + (v & (0x60 & epstate->uesta0x.v));
			epstate->uesta0x.v = v;
			break;
		case uesta1x:
			// read only
			break;
		case ueienx:
			epstate->ueienx.v = v;
			break;
		default:
			assert(0);
	}
	update_com_irq(p);
}

static uint8_t
avr_usb_ep_read_data(
        struct avr_t * avr,
        avr_io_addr_t addr,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	uint8_t ep = current_ep_to_cpu(p);
	int ret = ep_fifo_cpu_readbyte(p, get_epstate(p, ep));

	if (ret < 0) {
		if (ret == -2)
			raise_ep_interrupt(avr, p, ep, underfi);
		return 0;
	} else
		return (uint8_t) ret;
}

static void
avr_usb_ep_write_data(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	uint8_t ep = current_ep_to_cpu(p);
	int ret = ep_fifo_cpu_writebyte(p, get_epstate(p, ep), v);
	if (ret == 0)
		return;

	if (ret == -2)
		raise_ep_interrupt(avr, p, ep, overfi);
}

// PLOCK (bit 0) follows PLLE (bit 1), the PLL locks instantly
static void
avr_usb_pll_write(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	v = (v & ~1) | ((v >> 1) & 1);
	avr_core_watch_write(avr, addr, v);
}


/*
 * Full speed Start Of Frame, every 1ms from the first bus reset until the
 * device detaches. Advances the 11 bits frame number in UDFNUMH:UDFNUML.
 */
static avr_cycle_count_t
sof_generator(
        struct avr_t * avr,
        avr_cycle_count_t when,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;
	uint16_t fnum;

	if (!p->state->attached)
		return 0;
	fnum = (avr->data[p->r_usbcon + udfnuml] |
			(avr->data[p->r_usbcon + udfnumh] << 8)) + 1;
	avr->data[p->r_usbcon + udfnuml] = fnum;
	avr->data[p->r_usbcon + udfnumh] = (fnum >> 8) & 0x07;
	raise_usb_interrupt(p, sofi);
	return when + avr_usec_to_cycles(avr, 1000);
}

/*
 * Device side reset, on CPU reset and when USBE is cleared: all the
 * device and endpoint registers back to their reset values, DETACH set.
 */
static void
reset_device(
		avr_usb_t * p)
{
	avr_t * avr = p->io.avr;

	memset(p->state->ep_state, 0, sizeof p->state->ep_state);
	for (int i = udcon; i < otgtcon; i++)
		avr->data[p->r_usbcon + i] = 0;
	avr->data[p->r_usbcon + udcon] = UDCON_DETACH;
}

static void
avr_usb_usbcon_write(
        struct avr_t * avr,
        avr_io_addr_t addr,
        uint8_t v,
        void * param)
{
	avr_usb_t * p = (avr_usb_t *) param;

	if ((avr->data[addr] & USBCON_USBE) && !(v & USBCON_USBE)) {
		AVR_LOG(avr, LOG_TRACE, "USB: controller reset (USBE cleared)\n");
		reset_device(p);
		v |= USBCON_FRZCLK;
	}
	avr_core_watch_write(avr, addr, v);
	update_vbus(p);
	update_attach(p);
	update_gen_irq(p);
	update_com_irq(p);
}

static int
avr_usb_ioctl(
		struct avr_io_t * io,
		uint32_t ctl,
		void * io_param)
{
	avr_usb_t * p = (avr_usb_t *) io;
	struct avr_io_usb * d = (struct avr_io_usb*) io_param;
	struct _epstate * epstate = 0;
	int ret;
	uint8_t ep;

	switch (ctl) {
		case AVR_IOCTL_USB_READ:
		case AVR_IOCTL_USB_WRITE:
		case AVR_IOCTL_USB_SETUP:
			ep = d->pipe & 0x7f;
			if (ep >= ep_count(p)) {
				AVR_LOG(io->avr, LOG_WARNING,
						"USB: host access to EP%d, part has %d endpoints\n",
						ep, ep_count(p));
				return -1;
			}
			// no pull-up, or endpoint not usable: no handshake
			if (!p->state->attached)
				return AVR_IOCTL_USB_NAK;
			epstate = get_epstate(p, ep);
			if (!epstate->ueconx.epen || !epstate->uesta0x.cfgok)
				return AVR_IOCTL_USB_NAK;
			break;
		case AVR_IOCTL_USB_RESET:
			if (!p->state->attached) {
				AVR_LOG(io->avr, LOG_WARNING, "USB: bus reset while detached\n");
				return -1;
			}
			break;
	}

	switch (ctl) {
		case AVR_IOCTL_USB_READ:
			if (epstate->ueconx.stallrq) {
				raise_ep_interrupt(io->avr, p, ep, stalledi);
				return AVR_IOCTL_USB_STALL;
			}
			if (ep && !epstate->uecfg0x.epdir)
				AVR_LOG(io->avr, LOG_WARNING, "USB: Reading from IN endpoint from host??\n");

			ret = ep_fifo_usb_read(p, epstate, d->buf, d->sz);
			if (ret < 0) {
				raise_ep_interrupt(io->avr, p, ep, nakini);
				// is this correct? It makes the cdc example work.
				// Linux stops polling the data ep if we send naks,but
				// according to usb spec nak'ing should be ok.
				if (epstate->uecfg0x.eptype == 2) {
					d->sz = 0;
					return 0;
				} else
					return ret;
			}
			d->sz = ret;
			epstate->ueintx.fifocon = 1;
			raise_ep_interrupt(io->avr, p, ep, txini);
			return 0;
		case AVR_IOCTL_USB_WRITE:
			if (ep && epstate->uecfg0x.epdir)
				AVR_LOG(io->avr, LOG_WARNING, "USB: Writing to IN endpoint from host??\n");

			if (epstate->ueconx.stallrq) {
				raise_ep_interrupt(io->avr, p, ep, stalledi);
				return AVR_IOCTL_USB_STALL;
			}

			ret = ep_fifo_usb_write(p, ep, d->buf, d->sz);
			if (ret < 0) {
				if (ret == AVR_IOCTL_USB_NAK)
					raise_ep_interrupt(io->avr, p, ep, nakouti);
				return ret;
			}

			epstate->ueintx.fifocon = 1;
			raise_ep_interrupt(io->avr, p, ep, rxouti);
			return 0;
		case AVR_IOCTL_USB_SETUP:
			epstate->ueconx.stallrq = 0;
			// teensy actually depends on this (fails to ack rxouti on usb
			// control read status stage) even if the datasheet clearly states
			// that one should do so.
			epstate->ueintx.rxouti = 0;

			ret = ep_fifo_usb_write(p, ep, d->buf, d->sz);
			if (ret < 0)
				return ret;
			raise_ep_interrupt(io->avr, p, ep, rxstpi);

			return 0;
		case AVR_IOCTL_USB_RESET:
			AVR_LOG(io->avr, LOG_TRACE, "USB: __USB_RESET__\n");
			// all endpoints disabled, EP0 stays configured, address 0
			memset(&p->state->ep_state[1], 0,
					sizeof p->state->ep_state - sizeof p->state->ep_state[0]);
			ep_reset(get_epstate(p, 0));
			io->avr->data[p->r_usbcon + udaddr] = 0;
			raise_usb_interrupt(p, eorsti);
			update_com_irq(p);
			avr_cycle_timer_register_usec(io->avr, 1000, sof_generator, p);
			return 0;
		case AVR_IOCTL_USB_VBUS:
			p->state->vbus_level = io_param != NULL;
			update_vbus(p);
			update_attach(p);
			update_gen_irq(p);
			return 0;
		default:
			return -1;
	}
}

void
avr_usb_reset(
		struct avr_io_t *io)
{
	avr_usb_t * p = (avr_usb_t *) io;
	avr_t * avr = io->avr;

	reset_device(p);
	avr->data[p->r_usbcon + usbcon] = USBCON_FRZCLK;
	avr->data[p->r_usbcon + usbsta] = 0;
	avr->data[p->r_usbcon + usbint] = 0;
	for (int i = usbint + 1; i < udcon; i++)
		avr->data[p->r_usbcon + i] = 0;
	if (p->id.reg)
		avr_regbit_set(avr, p->id);
	update_attach(p);

	AVR_LOG(io->avr, LOG_TRACE, "USB: %s\n", __FUNCTION__);
}

static const char * irq_names[USB_IRQ_COUNT] = {
	[USB_IRQ_ATTACH] = ">attach",
};

static void
avr_usb_dealloc(
		struct avr_io_t * port)
{
	avr_usb_t * p = (avr_usb_t *) port;
	free(p->state);
}

static	avr_io_t	_io = {
	.kind = "usb",
	.reset = avr_usb_reset,
	.irq_names = irq_names,
	.ioctl = avr_usb_ioctl,
	.dealloc = avr_usb_dealloc,
};

static void
register_io_ep_readwrite(
		avr_t * avr,
		avr_usb_t * p,
		uint8_t laddr)
{
	avr_register_io_write(avr, p->r_usbcon + laddr, avr_usb_ep_write, p);
	avr_register_io_read(avr, p->r_usbcon + laddr, avr_usb_ep_read, p);
}

static void
register_vectors(
		avr_t * avr,
		avr_usb_t * p)
{
	// usb interrupts are multiplexed into just two vectors.
	// we therefore need fake bits for enable & raise

	// use usbe as fake enable bit
	p->state->com_vect.enable = (avr_regbit_t)AVR_IO_REGBIT(p->r_usbcon, 7);
	p->state->gen_vect.enable = (avr_regbit_t)AVR_IO_REGBIT(p->r_usbcon, 7);

//	// use reserved/unused bits in usbsta as fake raised bits
//	p->state->com_vect.raised = (avr_regbit_t)AVR_IO_REGBIT(p->r_usbcon+1,7);
//	p->state->gen_vect.raised = (avr_regbit_t)AVR_IO_REGBIT(p->r_usbcon+1,6);

	p->state->com_vect.vector = p->usb_com_vect;
	p->state->gen_vect.vector = p->usb_gen_vect;

	avr_register_vector(avr, &p->state->com_vect);
	avr_register_vector(avr, &p->state->gen_vect);
}

void avr_usb_init(avr_t * avr, avr_usb_t * p)
{
	p->io = _io;

	assert(ep_count(p) <= AVR_USB_MAX_ENDPOINTS - 1);
	p->state = calloc(1, sizeof *p->state);

	avr_register_io(avr, &p->io);
	register_vectors(avr, p);
	// allocate this module's IRQ
	avr_io_setirqs(&p->io, AVR_IOCTL_USB_GETIRQ(), USB_IRQ_COUNT, NULL);

	avr_register_io_write(avr, p->r_usbcon + usbcon, avr_usb_usbcon_write, p);
	if (p->vbus.reg) {
		avr_register_io_write(avr, p->r_usbcon + usbsta, avr_usb_usbsta_write, p);
		avr_register_io_write(avr, p->r_usbcon + usbint, avr_usb_usbint_write, p);
	}

	avr_register_io_write(avr, p->r_usbcon + udaddr, avr_usb_udaddr_write, p);
	avr_register_io_write(avr, p->r_usbcon + udcon, avr_usb_udcon_write, p);
	avr_register_io_write(avr, p->r_usbcon + udint, avr_usb_udint_write, p);
	avr_register_io_write(avr, p->r_usbcon + udien, avr_usb_udien_write, p);
	avr_register_io_write(avr, p->r_usbcon + uenum, avr_usb_uenum_write, p);
	avr_register_io_write(avr, p->r_usbcon + uerst, avr_usb_uerst_write, p);

	avr_register_io_read(avr, p->r_usbcon + uedatx, avr_usb_ep_read_data, p);
	avr_register_io_write(avr, p->r_usbcon + uedatx, avr_usb_ep_write_data, p);
	avr_register_io_read(avr, p->r_usbcon + uebclx, avr_usb_ep_read_bytecount, p); //ro
	avr_register_io_read(avr, p->r_usbcon + uebchx, avr_usb_ep_read_bytecount, p); //ro
	avr_register_io_read(avr, p->r_usbcon + ueint, avr_usb_ueint_read, p); //ro

	avr_register_io_read(avr, p->r_usbcon + ueintx, avr_usb_ep_read_ueintx, p);
	avr_register_io_write(avr, p->r_usbcon + ueintx, avr_usb_ep_write_ueintx, p);

	register_io_ep_readwrite(avr, p, ueconx);
	register_io_ep_readwrite(avr, p, uecfg0x);
	register_io_ep_readwrite(avr, p, uecfg1x);
	register_io_ep_readwrite(avr, p, uesta0x);
	register_io_ep_readwrite(avr, p, uesta1x);
	register_io_ep_readwrite(avr, p, ueienx);

	avr_register_io_write(avr, p->r_pllcsr, avr_usb_pll_write, p);
}
