/*
	avr_timer.c

	Handles the 8 bits and 16 bits AVR timer.
	Handles
	+ CDC
	+ Fast PWM

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
#include "avr_timer.h"

static avr_cycle_count_t avr_timer_compa(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_timer_t * p = (avr_timer_t *)param;
	avr_raise_interrupt(avr, &p->compa);
	return p->tov_cycles ? 0 : p->compa_cycles ? when + p->compa_cycles : 0;
}

static avr_cycle_count_t avr_timer_compb(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_timer_t * p = (avr_timer_t *)param;
	avr_raise_interrupt(avr, &p->compb);
	return p->tov_cycles ? 0 : p->compb_cycles ? when + p->compb_cycles : 0;
}

static avr_cycle_count_t avr_timer_tov(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_timer_t * p = (avr_timer_t *)param;
	avr_raise_interrupt(avr, &p->overflow);

	if (p->compa_cycles && p->tov_cycles > p->compa_cycles)
		avr_cycle_timer_register(avr, p->compa_cycles, avr_timer_compa, p);
	if (p->compb_cycles && p->tov_cycles > p->compb_cycles)
		avr_cycle_timer_register(avr, p->compb_cycles, avr_timer_compb, p);
	return when + p->tov_cycles;
}

static uint8_t avr_timer_tcnt_read(struct avr_t * avr, avr_io_addr_t addr, void * param)
{
	//avr_timer_t * p = (avr_timer_t *)param;
	// made to trigger potential watchpoints
	return avr_core_watch_read(avr, addr);
}

/*
 * The timers are /always/ 16 bits here, if the higher byte register
 * is specified it's just added.
 */
static uint16_t _timer_get_ocra(avr_timer_t * p)
{
	return p->io.avr->data[p->r_ocra] |
				(p->r_ocrah ? (p->io.avr->data[p->r_ocrah] << 8) : 0);
}
static uint16_t _timer_get_ocrb(avr_timer_t * p)
{
	return p->io.avr->data[p->r_ocrb] |
				(p->r_ocrbh ? (p->io.avr->data[p->r_ocrbh] << 8) : 0);
}

static void avr_timer_configure(avr_timer_t * p, uint32_t clock, uint32_t top)
{
	uint32_t ocra = _timer_get_ocra(p);
	uint32_t ocrb = _timer_get_ocrb(p);
	float fa = clock / (float)(ocra+1), fb = clock / (float)(ocrb+1);
	float t = clock / (float)(top+1);
	float frequency = p->io.avr->frequency;

	p->compa_cycles = p->compb_cycles = p->tov_cycles = 0;

	printf("%s-%c clock %d top %d a %d b %d\n", __FUNCTION__, p->name, clock, top, ocra, ocrb);
	if (top != ocra) {
		p->tov_cycles = frequency / t; // avr_hz_to_cycles(frequency, t);
		printf("%s-%c TOP %.2fHz = cycles = %d\n", __FUNCTION__, p->name, t, (int)p->tov_cycles);
	}
	if (ocra && ocra <= top) {
		p->compa_cycles = frequency / fa; // avr_hz_to_cycles(p->io.avr, fa);
		printf("%s-%c A %.2fHz = cycles = %d\n", __FUNCTION__, p->name, fa, (int)p->compa_cycles);
	}
	if (ocrb && ocrb <= top) {
		p->compb_cycles = frequency / fb; // avr_hz_to_cycles(p->io.avr, fb);
		printf("%s-%c B %.2fHz = cycles = %d\n", __FUNCTION__, p->name, fb, (int)p->compb_cycles);
	}

	if (p->tov_cycles > 1)
		avr_cycle_timer_register(p->io.avr, p->tov_cycles, avr_timer_tov, p);
	else {
		if (p->compa_cycles > 1)
			avr_cycle_timer_register(p->io.avr, p->compa_cycles, avr_timer_compa, p);
		if (p->compb_cycles > 1)
			avr_cycle_timer_register(p->io.avr, p->compb_cycles, avr_timer_compb, p);
	}
}

static void avr_timer_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_timer_t * p = (avr_timer_t *)param;

	p->compa_cycles = 0;
	p->compb_cycles = 0;

	avr_core_watch_write(avr, addr, v);
	long clock = avr->frequency;

	// only can exists on "asynchronous" 8 bits timers
	if (avr_regbit_get(avr, p->as2))
		clock = 32768;

	uint8_t cs = avr_regbit_get_array(avr, p->cs, ARRAY_SIZE(p->cs));
	if (cs == 0) {
		printf("%s-%c clock turned off\n", __FUNCTION__, p->name);		
		avr_cycle_timer_cancel(avr, avr_timer_tov, p);
		avr_cycle_timer_cancel(avr, avr_timer_compa, p);
		avr_cycle_timer_cancel(avr, avr_timer_compb, p);
		return;
	}

	uint8_t mode = avr_regbit_get_array(avr, p->wgm, ARRAY_SIZE(p->wgm));
	uint8_t cs_div = p->cs_div[cs];
	uint32_t f = clock >> cs_div;

	printf("%s-%c clock %d, div %d(/%d) = %d ; mode %d\n", __FUNCTION__, p->name, clock, cs, 1 << cs_div, f, mode);
	switch (p->wgm_op[mode].kind) {
		case avr_timer_wgm_normal:
			avr_timer_configure(p, f, (1 << p->wgm_op[mode].size) - 1);
			break;
		case avr_timer_wgm_ctc: {
			avr_timer_configure(p, f, _timer_get_ocra(p));
		}	break;
		case avr_timer_wgm_fast_pwm: {
			avr_raise_irq(p->io.irq + TIMER_IRQ_OUT_PWM0, _timer_get_ocra(p));
			avr_raise_irq(p->io.irq + TIMER_IRQ_OUT_PWM1, _timer_get_ocra(p));
		}	break;
		default:
			printf("%s-%c unsupported timer mode wgm=%d (%d)\n", __FUNCTION__, p->name, mode, p->wgm_op[mode].kind);
	}
}

static void avr_timer_reset(avr_io_t * port)
{
	avr_timer_t * p = (avr_timer_t *)port;
	avr_cycle_timer_cancel(p->io.avr, avr_timer_tov, p);
	avr_cycle_timer_cancel(p->io.avr, avr_timer_compa, p);
	avr_cycle_timer_cancel(p->io.avr, avr_timer_compb, p);
	p->compa_cycles = 0;
	p->compb_cycles = 0;
}

static	avr_io_t	_io = {
	.kind = "timer",
	.reset = avr_timer_reset,
};

void avr_timer_init(avr_t * avr, avr_timer_t * p)
{
	p->io = _io;
	printf("%s timer%c created OCRA %02x OCRAH %02x\n", __FUNCTION__, p->name, p->r_ocra,  p->r_ocrah);

	// allocate this module's IRQ
	p->io.irq_count = TIMER_IRQ_COUNT;
	p->io.irq = avr_alloc_irq(0, p->io.irq_count);
	p->io.irq_ioctl_get = AVR_IOCTL_TIMER_GETIRQ(p->name);
	p->io.irq[TIMER_IRQ_OUT_PWM0].flags |= IRQ_FLAG_FILTERED;
	p->io.irq[TIMER_IRQ_OUT_PWM1].flags |= IRQ_FLAG_FILTERED;

	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->compa);
	avr_register_vector(avr, &p->compb);

	avr_register_io_write(avr, p->cs[0].reg, avr_timer_write, p);

	/*
	 * Even if the timer is 16 bits, we don't care to have watches on the
	 * high bytes because the datasheet says that the low address is always
	 * the trigger.
	 */
	avr_register_io_write(avr, p->r_ocra, avr_timer_write, p);
	avr_register_io_write(avr, p->r_ocrb, avr_timer_write, p);

	avr_register_io_read(avr, p->r_tcnt, avr_timer_tcnt_read, p);
}
