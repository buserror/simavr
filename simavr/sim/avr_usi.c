/*
	avr_usi.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

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

#include <stdio.h>
#include <stdbool.h>
#include "avr_ioport.h"
#include "avr_usi.h"
#include "avr_timer.h"

#define _BV(r) ((r).mask << (r).bit)

// TODO: power reduction register

#if 0
#define DBG(x) x
#else
#define DBG(x)
#endif

static void _avr_usi_clock_counter(struct avr_t * avr, avr_usi_t * p)
{
	uint8_t counter_val = avr->data[p->r_usisr] & 0xF;
	uint8_t usisr = avr->data[p->r_usisr] & 0xf0;

	counter_val++;
	usisr |= (counter_val & 0x0f);
	avr_core_watch_write(avr, p->r_usisr, usisr);

	DBG(printf("USI ------------------- 	new count value %d\n", counter_val));

	if(counter_val > AVR_USI_COUNTER_MAX) {
		if (p->r_usibr)
			avr_core_watch_write(avr, p->r_usibr, avr->data[p->r_usidr]);
		DBG(printf("USI ------------------- 	OVERFLOW! usidr = 0x%02X\n", avr->data[p->r_usidr]));
		avr_raise_interrupt(avr, &p->usi_ovf);
	}
}

/* Force out current high bit. */

static void _avr_usi_push_high_bit(avr_usi_t * p)
{
	avr_raise_irq(&p->io.irq[USI_IRQ_DO],
				  ((p->io.avr->data[p->r_usidr] & 0x80) ? 1 : 0) |
				      AVR_IOPORT_OUTPUT);
}

static void _avr_usi_set_usidr(struct avr_t * avr, avr_usi_t * p, uint8_t new_val)
{
	DBG(printf("USI ------------------- 	USIDR new value 0x%02X\n", new_val));

	avr_core_watch_write(avr, p->r_usidr, new_val);

	if (!p->clock_high)
		_avr_usi_push_high_bit(p);
}

static void _avr_usi_clock_usidr(struct avr_t * avr, avr_usi_t * p)
{
	// in_bit0 is shifted into low bit
	_avr_usi_set_usidr(avr, p, ((avr->data[p->r_usidr] & ~0x80) << 1) | (p->in_bit0 ? 1 : 0));
}

static void _avr_usi_disconnect_irqs(struct avr_t * avr, avr_usi_t * p, uint8_t old_wm)
{
	switch(old_wm) {
		case USI_WM_OFF:
			// nothing hooked up...
			break;
		case USI_WM_THREEWIRE: {
			avr_ioport_getirq_t req = {
				.bit = p->pin_do
			};
			if (avr_ioctl(avr, AVR_IOCTL_IOPORT_GETIRQ_REGBIT, &req) > 0) {
				avr_unconnect_irq(&p->io.irq[USI_IRQ_DO], req.irq[0]);
			}
			break;
		}
		case USI_WM_TWOWIRE:
		case USI_WM_TWOWIRE_HOLD:
			avr_ioport_getirq_t req_di = { .bit = p->pin_di };
			if (avr_ioctl(avr, AVR_IOCTL_IOPORT_GETIRQ_REGBIT, &req_di) > 0) {
				avr_unconnect_irq(&p->io.irq[USI_IRQ_DO], req_di.irq[0]);
			}
			break;

		default:
			break;
	}
}

static void _avr_usi_connect_irqs(struct avr_t * avr, avr_usi_t * p, uint8_t new_wm)
{
	switch(new_wm) {
		case USI_WM_OFF:
			// nothing to hook up...
			break;
		case USI_WM_THREEWIRE: {
			avr_ioport_getirq_t req = {
				.bit = p->pin_do
			};
			if (avr_ioctl(avr, AVR_IOCTL_IOPORT_GETIRQ_REGBIT, &req) > 0) {
				avr_connect_irq(&p->io.irq[USI_IRQ_DO], req.irq[0]);
			}
			break;
		}
		case USI_WM_TWOWIRE:
		case USI_WM_TWOWIRE_HOLD:
			avr_ioport_getirq_t req_di = { .bit = p->pin_di };
			if (avr_ioctl(avr, AVR_IOCTL_IOPORT_GETIRQ_REGBIT, &req_di) > 0) {
				avr_connect_irq(&p->io.irq[USI_IRQ_DO], req_di.irq[0]);
			}
			break;

		default:
			break;
	}
}

static void _avr_usi_set_scl_hold(struct avr_t * avr, avr_usi_t * p, bool enable)
{
	avr_ioport_getirq_t req_ck = { .bit = p->pin_usck };
	if (avr_ioctl(avr, AVR_IOCTL_IOPORT_GETIRQ_REGBIT, &req_ck) > 0) {
		if (enable)
			req_ck.irq[0]->flags |= IRQ_FLAG_STRONG;
		else
			req_ck.irq[0]->flags &= ~IRQ_FLAG_STRONG;
	}
	if (enable)
		p->io.irq[USI_IRQ_USCK].flags |= IRQ_FLAG_STRONG;
	else
		p->io.irq[USI_IRQ_USCK].flags &= ~IRQ_FLAG_STRONG;
}

static avr_cycle_count_t _avr_usi_start_det_di_dly(struct avr_t * avr,	avr_cycle_count_t when,	void * param)
{
	avr_usi_t * p = (avr_usi_t *)param;

	if (!p->io.irq[USI_IRQ_USCK].value)
		return 0; // SCL must be high at SDA change

	DBG(printf("USI ------------------- DI start condition detected\n"));
	
	avr_raise_interrupt(avr, &p->usi_start);
	return 0;
}

static avr_cycle_count_t _avr_usi_stop_det_di_dly(struct avr_t * avr,	avr_cycle_count_t when,	void * param)
{
	avr_usi_t * p = (avr_usi_t *)param;

	if (!p->io.irq[USI_IRQ_USCK].value)
		return 0; // SCL must be high at SDA change

	DBG(printf("USI ------------------- DI stop condition detected\n"));
	
	avr_core_watch_write(avr, p->r_usisr, avr->data[p->r_usisr] | (1 << p->usipf.bit));
	return 0;
}

// -------------------------------------------------------------------------------------------------
// USISR - status register
// -------------------------------------------------------------------------------------------------

static uint8_t avr_usi_read_usisr(struct avr_t * avr, avr_io_addr_t addr, void * param)
{
	avr_usi_t * p = (avr_usi_t *)param;
	// high 3 bits and counter are stored normally
	uint8_t v = avr_core_watch_read(avr, p->r_usisr);

	// bit 4 (USIDC) is computed from register and pin
	// TODO: that^

	DBG(printf("USI ------------------- avr_usi_read_usisr = %02x\n", v));
	return v;
}

static void avr_usi_write_usisr(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_usi_t * p = (avr_usi_t *)param;
	DBG(printf("USI ------------------- avr_usi_write_usisr = %02x\n", v));

	if (v & (1 << p->usi_start.raised.bit)) {
		DBG(printf("USI ------------------- 	clearing USISIF\n"));
		v &= ~(1 << p->usi_start.raised.bit);
		avr_clear_interrupt(avr, &p->usi_start);
		if (avr_regbit_get(avr, p->usi_start.raised)) {
			_avr_usi_set_scl_hold(avr, p, false);
		}
	}

	if (v & (1 << p->usi_ovf.raised.bit)) {
		DBG(printf("USI ------------------- 	clearing USIOIF\n"));
		v &= ~(1 << p->usi_ovf.raised.bit);
		avr_clear_interrupt(avr, &p->usi_ovf);
		if (avr_regbit_get(avr, p->usi_ovf.raised)) {
			_avr_usi_set_scl_hold(avr, p, false);
		}
	}

	if (v & (1 << p->usipf.bit)) {
		DBG(printf("USI ------------------- 	clearing USIPF\n"));
		v &= ~(1 << p->usipf.bit);
	} else {
		v |= (avr->data[addr] & (1 << p->usipf.bit));
	}

	if (v & (1 << p->usidc.bit)) {
		DBG(printf("USI ------------------- 	ignoring USIDC\n"));
		v &= ~(1 << p->usidc.bit);
		v |= (avr->data[addr] & (1 << p->usidc.bit));
	}

	avr_core_watch_write(avr, addr, v);
}

// -------------------------------------------------------------------------------------------------
// USICR - control register
// -------------------------------------------------------------------------------------------------

#define USICR_CLK_MASK (_BV(p->usiclk) | _BV(p->usitc))

static uint8_t avr_usi_read_usicr(struct avr_t * avr, avr_io_addr_t addr, void * param)
{
	avr_usi_t * p = (avr_usi_t *)param;
	// high 6 bits are R/W
	uint8_t v = avr_core_watch_read(avr, p->r_usicr);
	// low 2 bits are W-only and read as 0
	v &= ~USICR_CLK_MASK;
	DBG(printf("USI ------------------- avr_usi_read_usicr = %02x\n", v));
	return v;
}

static void avr_usi_write_usicr(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_usi_t * p = (avr_usi_t *)param;
	DBG(printf("USI ------------------- avr_usi_write_usicr = %02x\n", v));

	// get old wire mode and clock select to detect changes
	uint8_t old_wm     = avr_regbit_get(avr, p->usiwm);
	uint8_t old_cs     = avr_regbit_get(avr, p->usics);

	avr_core_watch_write(avr, addr, v);

	uint8_t new_wm = avr_regbit_get(avr, p->usiwm);
	uint8_t new_cs = avr_regbit_get(avr, p->usics);

	// check if clock select changed

	if (new_cs != old_cs) {
		uint8_t old_clock_high = p->clock_high;

		DBG(printf("USI -------------------     cs = %d\n", new_cs));
		if (new_cs < USI_CS_EXT_POS) {
			p->clock_high = 0;
		} else {
			avr_ioport_getirq_t req_ck = { .bit = p->pin_usck };

			if (avr_ioctl(avr, AVR_IOCTL_IOPORT_GETIRQ_REGBIT, &req_ck) > 0)
				p->clock_high = ((req_ck.irq[0]->value & 0xff) != 0);
			if (new_cs == USI_CS_EXT_NEG)
				p->clock_high ^= 1;
		}

		if (old_clock_high && !p->clock_high)
			_avr_usi_push_high_bit(p);

		// TODO: hook things up differently
	}

	// check if wire mode changed

	if (new_wm != old_wm) {
		DBG(printf("USI -------------------     wm = %d\n", new_wm));
		_avr_usi_disconnect_irqs(avr, p, old_wm);
		_avr_usi_connect_irqs(avr, p, new_wm);

		if (new_wm && !old_wm && !p->clock_high)
			_avr_usi_push_high_bit(p);
	}

	// check if we're strobing the USI clock
	if (avr_regbit_get(avr, p->usiclk)) {
		DBG(printf("USI -------------------     strobe USI clock\n"));
		if (new_cs == 0) {
			/* Strobe - a fake clock bit. */

			_avr_usi_clock_usidr(avr, p);
			_avr_usi_clock_counter(avr, p);
		}
		avr_regbit_clear(avr, p->usiclk);
	}

	// check if we're toggling the clock output

	if (avr_regbit_get(avr, p->usitc)) {
		uint8_t out;

		/* The feature of directly clocking the counter is not implemented
		 * as it would cause double clocking without increasing speed.
		 */

		DBG(printf("USI -------------------     toggle USCL\n"));

		out = !(p->toggle_irq->value & 0xff);
		avr_raise_irq(p->toggle_irq, out | AVR_IOPORT_OUTPUT);
		avr_regbit_clear(avr, p->usitc);
	}
}

// -------------------------------------------------------------------------------------------------
// USIBR - buffered data register
// -------------------------------------------------------------------------------------------------

static void avr_usi_write_usibr(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	// it's read-only, so into the memory hole it goes.
}

// -------------------------------------------------------------------------------------------------
// USIDR - data register
// -------------------------------------------------------------------------------------------------

static void avr_usi_write_usidr(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	_avr_usi_set_usidr(avr, (avr_usi_t *)param, v);
}

// -------------------------------------------------------------------------------------------------
// Setup
// -------------------------------------------------------------------------------------------------

static void _avr_usi_di_changed(struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_usi_t * p = (avr_usi_t *)param;
	avr_t     * avr = p->io.avr;
	int         up, down;

	DBG(printf("USI ------------------- DI changed to %d at cycle %llu\n",
		value,
		avr->cycle));
	p->in_bit0 = value;

	if (avr_regbit_get(avr, p->usiwm) >= USI_WM_TWOWIRE) {
		//Start & Stop detection for two wire mode

		value &= 0xff;	// Ignore output flag
		up = !(irq->value & 0xff) && value;
		down = (irq->value & 0xff) && !value;
		
		if (down)
			avr_cycle_timer_register(avr, 1UL, _avr_usi_start_det_di_dly, p);
		else if (up)
			avr_cycle_timer_register(avr, 1UL, _avr_usi_stop_det_di_dly, p);
	}
}

static void _avr_usi_usck_changed(struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_usi_t * p = (avr_usi_t *)param;
	avr_t     * avr = p->io.avr;
	int         up, down;
	uint8_t     wm, cs;

	cs = avr_regbit_get(avr, p->usics);
	if (cs < USI_CS_EXT_POS)
		return;	// Clocked elsewhere.

	wm = avr_regbit_get(avr, p->usiwm);

	value &= 0xff;	// Ignore output flag
	if (cs == USI_CS_EXT_POS) {
		up = !(irq->value & 0xff) && value;
		down = (irq->value & 0xff) && !value;
	} else {
		down = !(irq->value & 0xff) && value;
		up = (irq->value & 0xff) && !value;
	}

	DBG(printf("USI ------------------- USCK had a logically %s edge at cycle %llu\n",
		up ? "rising" : down ? "falling" : "?",
		avr->cycle));

	if (wm >= USI_WM_TWOWIRE) {
		/* Clock Stretching after Start Condition: Hold SCL low after
		 *  master pulls it low (till interrupt flag is cleared)
		 */
		if (down && avr_regbit_get(avr, p->usi_start.raised)) {
			_avr_usi_set_scl_hold(avr, p, true);
		}
	}

	/* External input clocks the counter on both edges.  Clock USIDR first,
	 * so if the counter overflows, the interrupt will see it.  Latch output
	 * on "falling" edge.
	 */

	if (up || down) {
		if (up) {
			p->clock_high = 1;
			_avr_usi_clock_usidr(avr, p);
		} else {
			p->clock_high = 0;
			_avr_usi_push_high_bit(p);
		}
		_avr_usi_clock_counter(avr, p);
	}

	if (wm == USI_WM_TWOWIRE_HOLD) {
		/* Clock Stretching after Overflow (start of ACK): Hold SCL low
		 *  after master pulls it low (till interrupt flag is cleared)
		 */
		if (down && avr_regbit_get(avr, p->usi_ovf.raised)) {
			_avr_usi_set_scl_hold(avr, p, true);
		}
	}
}

static void _avr_usi_tim0_comp(struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_usi_t * p = (avr_usi_t *)param;
	avr_t * avr = p->io.avr;

	DBG(printf("USI ------------------- TIM0 compare happened...\n"));

	if(avr_regbit_get(avr, p->usics) == USI_CS_TIM0) {
		DBG(printf("USI ------------------- 	...and yeah, we're clocking\n"));
		_avr_usi_clock_usidr(avr, p);
		_avr_usi_clock_counter(avr, p);
	}
}

void avr_usi_reset(struct avr_io_t *io)
{
	avr_usi_t * p = (avr_usi_t *)io;
	struct avr_t * avr = p->io.avr;

	// set up input IRQs
	avr_irq_register_notify(&p->io.irq[USI_IRQ_DI], _avr_usi_di_changed, p);
	avr_irq_register_notify(&p->io.irq[USI_IRQ_USCK], _avr_usi_usck_changed, p);
	avr_irq_register_notify(&p->io.irq[USI_IRQ_TIM0_COMP], _avr_usi_tim0_comp,    p);

	// connect DI
	avr_ioport_getirq_t req_di = { .bit = p->pin_di };
	if (avr_ioctl(avr, AVR_IOCTL_IOPORT_GETIRQ_REGBIT, &req_di) > 0) {
		avr_connect_irq(req_di.irq[0], &p->io.irq[USI_IRQ_DI]);
	}

	// connect USCK/SCL
	avr_ioport_getirq_t req_ck = { .bit = p->pin_usck };
	if (avr_ioctl(avr, AVR_IOCTL_IOPORT_GETIRQ_REGBIT, &req_ck) > 0) {
		avr_connect_irq(req_ck.irq[0], &p->io.irq[USI_IRQ_USCK]);
		p->toggle_irq = req_ck.irq[0];
	}

	// connect TIM0
	avr_irq_t * tim0_comp = avr_io_getirq(avr, AVR_IOCTL_TIMER_GETIRQ('0'), TIMER_IRQ_OUT_COMP);
	avr_connect_irq(tim0_comp, &p->io.irq[USI_IRQ_TIM0_COMP]);
}

static const char * irq_names[USI_IRQ_COUNT] = {
	[USI_IRQ_DO]        = ">do",
	[USI_IRQ_DI]        = "=di_sda",
	[USI_IRQ_USCK]      = "=usck_scl",
	[USI_IRQ_TIM0_COMP] = "<tim0_comp",
};

static avr_io_t _io = {
	.kind = "usi",
	.reset = avr_usi_reset,
	.irq_names = irq_names,
};

void avr_usi_init(avr_t * avr, avr_usi_t * p)
{
	p->io = _io;
	avr_register_io(avr, &p->io);

	avr_register_vector(avr, &p->usi_start);
	avr_register_vector(avr, &p->usi_ovf);

	avr_io_setirqs(&p->io, AVR_IOCTL_USI_GETIRQ(), USI_IRQ_COUNT, NULL);

	avr_register_io_read (avr, p->r_usisr, avr_usi_read_usisr,  p);
	avr_register_io_write(avr, p->r_usisr, avr_usi_write_usisr, p);
	avr_register_io_read (avr, p->r_usicr, avr_usi_read_usicr,  p);
	avr_register_io_write(avr, p->r_usicr, avr_usi_write_usicr, p);
	if (p->r_usibr)
		avr_register_io_write(avr, p->r_usibr, avr_usi_write_usibr, p);
	avr_register_io_write(avr, p->r_usidr, avr_usi_write_usidr, p);
}
