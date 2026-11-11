/*
	usb_test_fw.h

	USB device firmware shared by atmega32u2_usb.c and atmega32u4_usb.c,
	driven by test_usb_host.h.

	- Probes endpoint configurations before attaching and reports the CFGOK
	  results through a vendor request.
	- Attaches when VBUS is detected (parts with a VBUS pad) or right away.
	  VBUSTE is enabled after VBUSTI is already set, so the USB_GEN
	  interrupt has to fire on the level, not on the edge.
	- Answers GET_DESCRIPTOR(device), SET_ADDRESS, SET_CONFIGURATION and
	  vendor request 1 on EP0, stalls everything else.
	- Echoes EP1 OUT packets, XORed with 0x5a, on EP2 IN.

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

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#ifdef UEBCHX
#define EP1_SIZE	256
#define EP1_SIZE_BITS	(5 << EPSIZE0)
#define EP2_SIZE	64
#define EP2_SIZE_BITS	(3 << EPSIZE0)
#else
// 176 bytes DPRAM: EP0 64 + EP1 64 + EP2 32
#define EP1_SIZE	64
#define EP1_SIZE_BITS	(3 << EPSIZE0)
#define EP2_SIZE	32
#define EP2_SIZE_BITS	(2 << EPSIZE0)
#endif

// Probe result bits, see probe_endpoints()
#define PROBE_EP1_256		(1 << 0)
#define PROBE_EP2_128		(1 << 1)
#define PROBE_EP1_DUAL		(1 << 2)
#define PROBE_EP3_DUAL		(1 << 3)
#define PROBE_DPRAM_192		(1 << 4)
#define PROBE_EP5			(1 << 5)
#define PROBE_STALL_RMW		(1 << 6)
#define PROBE_UERST			(1 << 7)

static const uint8_t device_desc[18] = {
	18, 1, 0x00, 0x02, 0xff, 0x00, 0x00, 64,
	0xeb, 0x03, 0x34, 0x12, 0x00, 0x01, 0, 0, 0, 1
};

static volatile uint8_t probe;
static volatile uint8_t configured;
static uint8_t buf[EP1_SIZE];

static void
ep_free(uint8_t ep)
{
	UENUM = ep;
	UECFG1X = 0;
	UECONX = 0;
}

// Configures an endpoint, returns CFGOK
static uint8_t
ep_config(uint8_t ep, uint8_t cfg0, uint8_t cfg1)
{
	UENUM = ep;
	UECONX = (1 << EPEN);
	UECFG0X = cfg0;
	UECFG1X = cfg1 | (1 << ALLOC);
	return (UESTA0X & (1 << CFGOK)) != 0;
}

static void
probe_endpoints(void)
{
	const uint8_t bulk_out = (2 << EPTYPE0);
	const uint8_t bulk_in = (2 << EPTYPE0) | (1 << EPDIR);
	uint8_t r = 0;

	if (ep_config(1, bulk_out, 5 << EPSIZE0))
		r |= PROBE_EP1_256;
	ep_free(1);
	if (ep_config(2, bulk_out, 4 << EPSIZE0))
		r |= PROBE_EP2_128;
	ep_free(2);
	if (ep_config(1, bulk_out, (3 << EPSIZE0) | (1 << EPBK0)))
		r |= PROBE_EP1_DUAL;
	ep_free(1);
	if (ep_config(3, bulk_out, (3 << EPSIZE0) | (1 << EPBK0)))
		r |= PROBE_EP3_DUAL;
	ep_free(3);

	// 3 x 64 bytes, more than the 176 bytes of the xxU2 DPRAM
	ep_config(0, 0, 3 << EPSIZE0);
	ep_config(1, bulk_out, 3 << EPSIZE0);
	if (ep_config(2, bulk_out, 3 << EPSIZE0))
		r |= PROBE_DPRAM_192;
	ep_free(2);
	ep_free(1);
	ep_free(0);

	if (ep_config(5, bulk_out, 3 << EPSIZE0))
		r |= PROBE_EP5;
	ep_free(5);

	// read-modify-write STALLRQC must clear a pending STALLRQ
	ep_config(3, bulk_in, 3 << EPSIZE0);
	UECONX |= (1 << STALLRQ);
	if (UECONX & (1 << STALLRQ)) {
		UECONX |= (1 << STALLRQC);
		if (!(UECONX & (1 << STALLRQ)))
			r |= PROBE_STALL_RMW;
	}

	// endpoint reset empties the bank, keeps the configuration
	UEDATX = 1; UEDATX = 2; UEDATX = 3;
	if (UEBCLX == 3) {
		UERST = (1 << 3);
		UERST = 0;
		UENUM = 3;
		if (UEBCLX == 0 && (UESTA0X & (1 << CFGOK)) && (UECONX & (1 << EPEN)))
			r |= PROBE_UERST;
	}
	ep_free(3);

	probe = r;
}

ISR(USB_GEN_vect)
{
#ifdef VBUSTI
	if (USBINT & (1 << VBUSTI)) {
		USBINT &= ~(1 << VBUSTI);
		if (USBSTA & (1 << VBUS))
			UDCON &= ~(1 << DETACH);
		else
			UDCON |= (1 << DETACH);
	}
#endif
	if (UDINT & (1 << EORSTI)) {
		UDINT &= ~(1 << EORSTI);
		configured = 0;
		ep_config(0, 0, 3 << EPSIZE0);
		UEIENX = (1 << RXSTPE);
	}
}

static void
ep0_send(const uint8_t * data, uint8_t len)
{
	while (len--)
		UEDATX = *data++;
	UEINTX &= ~(1 << TXINI);
	while (!(UEINTX & (1 << RXOUTI)))
		;
	UEINTX &= ~(1 << RXOUTI);
}

static void
ep0_status_in(void)
{
	UEINTX &= ~(1 << TXINI);
	while (!(UEINTX & (1 << TXINI)))
		;
}

ISR(USB_COM_vect)
{
	uint8_t setup[8];

	UENUM = 0;
	if (!(UEINTX & (1 << RXSTPI)))
		return;
	for (uint8_t i = 0; i < 8; i++)
		setup[i] = UEDATX;
	UEINTX &= ~(1 << RXSTPI);

	uint16_t wLength = setup[6] | (setup[7] << 8);
	switch (setup[1] | (setup[0] << 8)) {
		case 0x8006:	// GET_DESCRIPTOR
			if (setup[3] != 1)
				goto stall;
			ep0_send(device_desc, wLength < 18 ? wLength : 18);
			break;
		case 0x0005:	// SET_ADDRESS
			UDADDR = setup[2] & 0x7f;
			ep0_status_in();
			UDADDR |= (1 << ADDEN);
			break;
		case 0x0009:	// SET_CONFIGURATION
			ep_config(1, (2 << EPTYPE0), EP1_SIZE_BITS);
			ep_config(2, (2 << EPTYPE0) | (1 << EPDIR), EP2_SIZE_BITS);
			UENUM = 0;
			ep0_status_in();
			configured = 1;
			break;
		case 0xc001: {	// vendor: probe results
			uint8_t v = probe;
			ep0_send(&v, 1);
		}	break;
		default:
		stall:
			UECONX |= (1 << STALLRQ);
			break;
	}
}

static void
echo(void)
{
	uint16_t n, i, j;

	UENUM = 1;
	if (!(UEINTX & (1 << RXOUTI)))
		return;
#ifdef UEBCHX
	n = UEBCLX | (UEBCHX << 8);
#else
	n = UEBCLX;
#endif
	for (i = 0; i < n; i++)
		buf[i] = UEDATX ^ 0x5a;
	UEINTX &= ~((1 << RXOUTI) | (1 << FIFOCON));

	UENUM = 2;
	for (i = 0; i < n; i += EP2_SIZE) {
		while (!(UEINTX & (1 << TXINI)))
			;
		UEINTX &= ~(1 << TXINI);
		for (j = i; j < n && j < i + EP2_SIZE; j++)
			UEDATX = buf[j];
		UEINTX &= ~(1 << FIFOCON);
	}
}

int
main(void)
{
	PLLCSR = (1 << PLLE);
	while (!(PLLCSR & (1 << PLOCK)))
		;
#ifdef UHWCON
	UHWCON = (1 << UVREGE);
#endif
	USBCON = (1 << USBE);	// clears FRZCLK
	probe_endpoints();

	UDIEN = (1 << EORSTE);
#ifdef VBUSTI
	USBCON |= (1 << OTGPADE);
	while (!(USBINT & (1 << VBUSTI)))
		;
	sei();
	USBCON |= (1 << VBUSTE);	// flag already set, must interrupt now
#else
	UDCON &= ~(1 << DETACH);
	sei();
#endif
	for (;;) {
		cli();
		if (configured)
			echo();
		sei();
	}
}
