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
static uint16_t _timer_get_tcnt(avr_timer_t * p)
{
	return p->io.avr->data[p->r_tcnt] |
				(p->r_tcnth ? (p->io.avr->data[p->r_tcnth] << 8) : 0);
}

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
	int start = p->tov_base == 0;
	
	if (!start)
		avr_raise_interrupt(avr, &p->overflow);
	p->tov_base = when;

	if (p->compa_cycles) {
		if (p->compa_cycles < p->tov_cycles)
			avr_cycle_timer_register(avr, 
				p->compa_cycles - (avr->cycle - p->tov_base), 
				avr_timer_compa, p);
		else if (p->tov_cycles == p->compa_cycles && !start)
			avr_timer_compa(avr, when, param);
	}
	
	if (p->compb_cycles) {
		if (p->compb_cycles < p->tov_cycles)
			avr_cycle_timer_register(avr, 
				p->compb_cycles - (avr->cycle - p->tov_base), 
				avr_timer_compb, p);
		else if (p->tov_cycles == p->compb_cycles && !start)
			avr_timer_compb(avr, when, param);
	}

	return when + p->tov_cycles;
}


static uint8_t avr_timer_tcnt_read(struct avr_t * avr, avr_io_addr_t addr, void * param)
{
	avr_timer_t * p = (avr_timer_t *)param;
	// made to trigger potential watchpoints

	uint64_t when = avr->cycle - p->tov_base;

	uint16_t tcnt = (when * p->tov_top) / p->tov_cycles;
//	printf("%s-%c when = %d tcnt = %d/%d\n", __FUNCTION__, p->name, (uint32_t)when, tcnt, p->tov_top);

	avr->data[p->r_tcnt] = tcnt;
	if (p->r_tcnth)
		avr->data[p->r_tcnth] = tcnt >> 8;
	
	return avr_core_watch_read(avr, addr);
}

static void avr_timer_tcnt_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_timer_t * p = (avr_timer_t *)param;
	avr_core_watch_write(avr, addr, v);
	uint16_t tcnt = _timer_get_tcnt(p);

	if (!p->tov_top)
		return;
		
	if (tcnt >= p->tov_top)
		tcnt = 0;
	
	// this involves some magicking
	// cancel the current timers, recalculate the "base" we should be at, reset the
	// timer base as it should, and re-shedule the timers using that base.
	
	avr_cycle_timer_cancel(avr, avr_timer_tov, p);
	avr_cycle_timer_cancel(avr, avr_timer_compa, p);
	avr_cycle_timer_cancel(avr, avr_timer_compb, p);

	uint64_t cycles = (tcnt * p->tov_cycles) / p->tov_top;

//	printf("%s-%c %d/%d -- cycles %d/%d\n", __FUNCTION__, p->name, tcnt, p->tov_top, (uint32_t)cycles, (uint32_t)p->tov_cycles);

	// this reset the timers bases to the new base
	p->tov_base = 0;
	avr_cycle_timer_register(avr, p->tov_cycles - cycles, avr_timer_tov, p);
	avr_timer_tov(avr, avr->cycle - cycles, p);

//	tcnt = ((avr->cycle - p->tov_base) * p->tov_top) / p->tov_cycles;
//	printf("%s-%c new tnt derive to %d\n", __FUNCTION__, p->name, tcnt);	
}

static void avr_timer_configure(avr_timer_t * p, uint32_t clock, uint32_t top)
{
	uint32_t ocra = _timer_get_ocra(p);
	uint32_t ocrb = _timer_get_ocrb(p);
	float fa = clock / (float)(ocra+1), fb = clock / (float)(ocrb+1);
	float t = clock / (float)(top+1);
	float frequency = p->io.avr->frequency;

	p->compa_cycles = p->compb_cycles = p->tov_cycles = 0;
	p->tov_top = top;

	printf("%s-%c clock %d top %d a %d b %d\n", __FUNCTION__, p->name, clock, top, ocra, ocrb);
	p->tov_cycles = frequency / t; // avr_hz_to_cycles(frequency, t);
	printf("%s-%c TOP %.2fHz = cycles = %d\n", __FUNCTION__, p->name, t, (int)p->tov_cycles);

	if (ocra && ocra <= top) {
		p->compa_cycles = frequency / fa; // avr_hz_to_cycles(p->io.avr, fa);
		printf("%s-%c A %.2fHz = cycles = %d\n", __FUNCTION__, p->name, fa, (int)p->compa_cycles);
	}
	if (ocrb && ocrb <= top) {
		p->compb_cycles = frequency / fb; // avr_hz_to_cycles(p->io.avr, fb);
		printf("%s-%c B %.2fHz = cycles = %d\n", __FUNCTION__, p->name, fb, (int)p->compb_cycles);
	}

	if (p->tov_cycles > 1) {
		avr_cycle_timer_register(p->io.avr, p->tov_cycles, avr_timer_tov, p);
		// calling it once, with when == 0 tells it to arm the A/B timers if needed
		p->tov_base = 0;
		avr_timer_tov(p->io.avr, p->io.avr->cycle, p);
	}
}

static void avr_timer_reconfigure(avr_timer_t * p)
{
	avr_t * avr = p->io.avr;

	// cancel everything
	p->compa_cycles = 0;
	p->compb_cycles = 0;
	p->tov_cycles = 0;
	
	avr_cycle_timer_cancel(avr, avr_timer_tov, p);
	avr_cycle_timer_cancel(avr, avr_timer_compa, p);
	avr_cycle_timer_cancel(avr, avr_timer_compb, p);

	long clock = avr->frequency;

	// only can exists on "asynchronous" 8 bits timers
	if (avr_regbit_get(avr, p->as2))
		clock = 32768;

	uint8_t cs = avr_regbit_get_array(avr, p->cs, ARRAY_SIZE(p->cs));
	if (cs == 0) {
		printf("%s-%c clock turned off\n", __FUNCTION__, p->name);		
		return;
	}

	uint8_t mode = avr_regbit_get_array(avr, p->wgm, ARRAY_SIZE(p->wgm));
	uint8_t cs_div = p->cs_div[cs];
	uint32_t f = clock >> cs_div;

	//printf("%s-%c clock %d, div %d(/%d) = %d ; mode %d\n", __FUNCTION__, p->name, clock, cs, 1 << cs_div, f, mode);
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

static void avr_timer_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_timer_t * p = (avr_timer_t *)param;
	avr_core_watch_write(avr, addr, v);
	avr_timer_reconfigure(p);
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
	avr_register_io_write(avr, p->r_tcnt, avr_timer_tcnt_write, p);

	avr_register_io_read(avr, p->r_tcnt, avr_timer_tcnt_read, p);
}
