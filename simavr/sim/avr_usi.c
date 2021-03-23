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

static uint8_t _usi_get_counter(struct avr_t * avr, avr_usi_t * p)
{
	return avr->data[p->r_usisr] & 0xF;
}

static void _usi_set_counter(struct avr_t * avr, avr_usi_t * p, uint8_t new_val)
{
	avr->data[p->r_usisr] = (_usi_get_counter(avr, p) & ~0xF) | (new_val & 0xF);
}

static void _avr_usi_clock_counter(struct avr_t * avr, avr_usi_t * p)
{
	uint8_t counter_val = _usi_get_counter(avr, p);
	counter_val++;
	_usi_set_counter(avr, p, counter_val);

	DBG(printf("USI ------------------- 	4bC new value %d\n", counter_val));

	if(counter_val > AVR_USI_COUNTER_MAX) {
		avr_core_watch_write(avr, p->r_usibr, avr->data[p->r_usidr]);
		DBG(printf("USI ------------------- 	OVERFLOW! usibr = 0x%02X\n", avr->data[p->r_usibr]));
		avr_raise_interrupt(avr, &p->usi_ovf);
	}
}

static void _avr_usi_set_usidr(struct avr_t * avr, avr_usi_t * p, uint8_t new_val)
{
	DBG(printf("USI ------------------- 	USIDR new value 0x%02X\n", new_val));
	bool top_set = avr->data[p->r_usidr] & 0x80;
	avr_core_watch_write(avr, p->r_usidr, new_val);

	switch(avr_regbit_get(avr, p->usiwm)) {
		case USI_WM_THREEWIRE:
			avr_raise_irq(&p->io.irq[USI_IRQ_DO], AVR_IOPORT_OUTPUT | (top_set ? 1 : 0));
			break;
		case USI_WM_TWOWIRE:
		case USI_WM_TWOWIRE_HOLD:
			// TODO: this
			break;
		default:
			break;
	}
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
			// TODO: this
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
			// TODO: this
			break;

		default:
			break;
	}
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

	uint8_t old_usisif = avr_regbit_get(avr, p->usi_start.raised);
	uint8_t old_usioif = avr_regbit_get(avr, p->usi_ovf.raised);

	avr_core_watch_write(avr, addr, v);

	if (avr_clear_interrupt_if(avr, &p->usi_start, old_usisif)) {
		DBG(printf("USI ------------------- 	acknowledging USISIF\n"));
		// TODO: release the hold on USCL
	}

	if (avr_clear_interrupt_if(avr, &p->usi_ovf, old_usioif)) {
		DBG(printf("USI ------------------- 	acknowledging USIOIF\n"));
		// TODO: release the hold on USCL
	}
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

	// check if wire mode changed
	if (new_wm != old_wm) {
		DBG(printf("USI -------------------     wm = %d\n", new_wm));
		_avr_usi_disconnect_irqs(avr, p, old_wm);
		_avr_usi_connect_irqs(avr, p, new_wm);
	}

	// check if clock select changed
	if (new_cs != old_cs) {
		DBG(printf("USI -------------------     cs = %d\n", new_cs));
		// TODO: hook things up differently
	}

	// check if we're strobing the USI clock
	if (avr_regbit_get(avr, p->usiclk)) {
		DBG(printf("USI -------------------     strobe USI clock\n"));
		avr_regbit_clear(avr, p->usiclk);
		// TODO: this
	}

	// check if we're toggling the clock output
	if (avr_regbit_get(avr, p->usitc)) {
		DBG(printf("USI -------------------     toggle USCL\n"));
		avr_regbit_clear(avr, p->usitc);
		// TODO: this
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
	// avr_t * avr = p->io.avr;
	DBG(printf("USI ------------------- DI changed to %d at cycle %llu\n",
		value,
		p->io.avr->cycle));
	p->in_bit0 = value;
}

static void _avr_usi_usck_changed(struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_usi_t * p = (avr_usi_t *)param;
	avr_t * avr = p->io.avr;

	int up = !irq->value && value;
	int down = irq->value && !value;

	DBG(printf("USI ------------------- USCK had a %s edge at cycle %llu\n",
		up ? "rising" : down ? "falling" : "?",
		avr->cycle));

	uint8_t wm = avr_regbit_get(avr, p->usiwm);

	if(wm == USI_WM_TWOWIRE || wm == USI_WM_TWOWIRE_HOLD) {
		// TODO: tell the TWI CCU that something happened
	}

	uint8_t cs = avr_regbit_get(avr, p->usics);

	// clock USIDR first, so if the counter overflows, the interrupt will see it
	if(cs == USI_CS_EXT_POS) {
		if(up)
			_avr_usi_clock_usidr(avr, p);

		_avr_usi_clock_counter(avr, p);
	} else if(cs == USI_CS_EXT_NEG) {
		if(down)
			_avr_usi_clock_usidr(avr, p);

		_avr_usi_clock_counter(avr, p);
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
	avr_irq_register_notify(&p->io.irq[USI_IRQ_DI],        _avr_usi_di_changed,   p);
	avr_irq_register_notify(&p->io.irq[USI_IRQ_USCK],      _avr_usi_usck_changed, p);
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
	avr_register_io_write(avr, p->r_usibr, avr_usi_write_usibr, p);
	avr_register_io_write(avr, p->r_usidr, avr_usi_write_usidr, p);
}