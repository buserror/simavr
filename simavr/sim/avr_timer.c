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
#include "avr_ioport.h"

/*
 * The timers are /always/ 16 bits here, if the higher byte register
 * is specified it's just added.
 */
static uint16_t _timer_get_ocr(avr_timer_t * p, int compi)
{
	return p->io.avr->data[p->comp[compi].r_ocr] |
		      (p->comp[compi].r_ocrh ? (p->io.avr->data[p->comp[compi].r_ocrh] << 8) : 0);
}
static uint16_t _timer_get_tcnt(avr_timer_t * p)
{
	return p->io.avr->data[p->r_tcnt] |
				(p->r_tcnth ? (p->io.avr->data[p->r_tcnth] << 8) : 0);
}
static uint16_t _timer_get_icr(avr_timer_t * p)
{
	return p->io.avr->data[p->r_icr] |
				(p->r_tcnth ? (p->io.avr->data[p->r_icrh] << 8) : 0);
}
static avr_cycle_count_t avr_timer_comp(avr_timer_t *p, avr_cycle_count_t when, uint8_t comp)
{
	avr_t * avr = p->io.avr;
	avr_raise_interrupt(avr, &p->comp[comp].interrupt);

	// check output compare mode and set/clear pins
	uint8_t mode = avr_regbit_get(avr, p->comp[comp].com);
	avr_irq_t * irq = &p->io.irq[TIMER_IRQ_OUT_COMP + comp];

	switch (mode) {
		case avr_timer_com_normal: // Normal mode OCnA disconnected
			break;
		case avr_timer_com_toggle: // Toggle OCnA on compare match
			if (p->comp[comp].com_pin.reg)	// we got a physical pin
				avr_raise_irq(irq,
						AVR_IOPORT_OUTPUT | (avr_regbit_get(avr, p->comp[comp].com_pin) ? 0 : 1));
			else // no pin, toggle the IRQ anyway
				avr_raise_irq(irq,
						p->io.irq[TIMER_IRQ_OUT_COMP + comp].value ? 0 : 1);
			break;
		case avr_timer_com_clear:
			avr_raise_irq(irq, 0);
			break;
		case avr_timer_com_set:
			avr_raise_irq(irq, 1);
			break;
	}

	return p->tov_cycles ? 0 : p->comp[comp].comp_cycles ? when
	        + p->comp[comp].comp_cycles : 0;
}

static avr_cycle_count_t avr_timer_compa(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	return avr_timer_comp((avr_timer_t*)param, when, AVR_TIMER_COMPA);
}

static avr_cycle_count_t avr_timer_compb(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	return avr_timer_comp((avr_timer_t*)param, when, AVR_TIMER_COMPB);
}

static avr_cycle_count_t avr_timer_compc(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	return avr_timer_comp((avr_timer_t*)param, when, AVR_TIMER_COMPC);
}

// timer overflow
static avr_cycle_count_t avr_timer_tov(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_timer_t * p = (avr_timer_t *)param;
	int start = p->tov_base == 0;

	if (!start)
		avr_raise_interrupt(avr, &p->overflow);
	p->tov_base = when;

	static const avr_cycle_timer_t dispatch[AVR_TIMER_COMP_COUNT] =
		{ avr_timer_compa, avr_timer_compb, avr_timer_compc };

	for (int compi = 0; compi < AVR_TIMER_COMP_COUNT; compi++) {
		if (p->comp[compi].comp_cycles) {
			if (p->comp[compi].comp_cycles < p->tov_cycles)
				avr_cycle_timer_register(avr,
					p->comp[compi].comp_cycles,
					dispatch[compi], p);
			else if (p->tov_cycles == p->comp[compi].comp_cycles && !start)
				dispatch[compi](avr, when, param);
		}
	}

	return when + p->tov_cycles;
}

static uint16_t _avr_timer_get_current_tcnt(avr_timer_t * p)
{
	avr_t * avr = p->io.avr;
	if (p->tov_cycles) {
		uint64_t when = avr->cycle - p->tov_base;

		return (when * (((uint32_t)p->tov_top)+1)) / p->tov_cycles;
	}
	return 0;
}

static uint8_t avr_timer_tcnt_read(struct avr_t * avr, avr_io_addr_t addr, void * param)
{
	avr_timer_t * p = (avr_timer_t *)param;
	// made to trigger potential watchpoints

	uint16_t tcnt = _avr_timer_get_current_tcnt(p);

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
	avr_cycle_timer_cancel(avr, avr_timer_compc, p);

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
	float t = clock / (float)(top+1);
	float frequency = p->io.avr->frequency;

	p->tov_cycles = 0;
	p->tov_top = top;

	p->tov_cycles = frequency / t; // avr_hz_to_cycles(frequency, t);
	printf("%s-%c TOP %.2fHz = %d cycles\n", __FUNCTION__, p->name, t, (int)p->tov_cycles);

	for (int compi = 0; compi < AVR_TIMER_COMP_COUNT; compi++) {
		uint32_t ocr = _timer_get_ocr(p, compi);
		float fc = clock / (float)(ocr+1);

		p->comp[compi].comp_cycles = 0;
//		printf("%s-%c clock %d top %d OCR%c %d\n", __FUNCTION__, p->name, clock, top, 'A'+compi, ocr);

		if (ocr && ocr <= top) {
			p->comp[compi].comp_cycles = frequency / fc; // avr_hz_to_cycles(p->io.avr, fa);
			printf("%s-%c %c %.2fHz = %d cycles\n", __FUNCTION__, p->name,
					'A'+compi, fc, (int)p->comp[compi].comp_cycles);
		}
	}

	if (p->tov_cycles > 1) {
		avr_cycle_timer_register(p->io.avr, p->tov_cycles, avr_timer_tov, p);
		// calling it once, with when == 0 tells it to arm the A/B/C timers if needed
		p->tov_base = 0;
		avr_timer_tov(p->io.avr, p->io.avr->cycle, p);
	}
}

static void avr_timer_reconfigure(avr_timer_t * p)
{
	avr_t * avr = p->io.avr;

	avr_timer_wgm_t zero={0};
	p->mode = zero;
	// cancel everything
	p->comp[AVR_TIMER_COMPA].comp_cycles = 0;
	p->comp[AVR_TIMER_COMPB].comp_cycles = 0;
	p->comp[AVR_TIMER_COMPC].comp_cycles = 0;
	p->tov_cycles = 0;
	
	avr_cycle_timer_cancel(avr, avr_timer_tov, p);
	avr_cycle_timer_cancel(avr, avr_timer_compa, p);
	avr_cycle_timer_cancel(avr, avr_timer_compb, p);
	avr_cycle_timer_cancel(avr, avr_timer_compc, p);

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

	p->mode = p->wgm_op[mode];
	//printf("%s-%c clock %d, div %d(/%d) = %d ; mode %d\n", __FUNCTION__, p->name, clock, cs, 1 << cs_div, f, mode);
	switch (p->mode.kind) {
		case avr_timer_wgm_normal:
			avr_timer_configure(p, f, (1 << p->mode.size) - 1);
			break;
		case avr_timer_wgm_ctc: {
			avr_timer_configure(p, f, _timer_get_ocr(p, AVR_TIMER_COMPA));
		}	break;
		case avr_timer_wgm_pwm: {
			uint16_t top = p->mode.top == avr_timer_wgm_reg_ocra ? _timer_get_ocr(p, AVR_TIMER_COMPA) : _timer_get_icr(p);
			avr_timer_configure(p, f, top);
		}	break;
		case avr_timer_wgm_fast_pwm:
			avr_timer_configure(p, f, (1 << p->mode.size) - 1);
			break;
		default:
			printf("%s-%c unsupported timer mode wgm=%d (%d)\n", __FUNCTION__, p->name, mode, p->mode.kind);
	}	
}

static void avr_timer_write_ocr(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_timer_t * p = (avr_timer_t *)param;
	avr_core_watch_write(avr, addr, v);

	switch (p->mode.kind) {
		case avr_timer_wgm_normal:
			avr_timer_reconfigure(p);
			break;
		case avr_timer_wgm_pwm:
			if (p->mode.top != avr_timer_wgm_reg_ocra) {
				avr_raise_irq(p->io.irq + TIMER_IRQ_OUT_PWM0, _timer_get_ocr(p, AVR_TIMER_COMPA));
				avr_raise_irq(p->io.irq + TIMER_IRQ_OUT_PWM1, _timer_get_ocr(p, AVR_TIMER_COMPB));
			}
			break;
		case avr_timer_wgm_fast_pwm:
			avr_raise_irq(p->io.irq + TIMER_IRQ_OUT_PWM0, _timer_get_ocr(p, AVR_TIMER_COMPA));
			avr_raise_irq(p->io.irq + TIMER_IRQ_OUT_PWM1, _timer_get_ocr(p, AVR_TIMER_COMPB));
			break;
		default:
			printf("%s-%c mode %d UNSUPPORTED\n", __FUNCTION__, p->name, p->mode.kind);
			avr_timer_reconfigure(p);
			break;
	}
}

static void avr_timer_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_timer_t * p = (avr_timer_t *)param;

	uint8_t as2 = avr_regbit_get(avr, p->as2);
	uint8_t cs = avr_regbit_get_array(avr, p->cs, ARRAY_SIZE(p->cs));
	uint8_t mode = avr_regbit_get_array(avr, p->wgm, ARRAY_SIZE(p->wgm));

	avr_core_watch_write(avr, addr, v);

	// only reconfigure the timer if "relevant" bits have changed
	// this prevent the timer reset when changing the edge detector
	// or other minor bits
	if (avr_regbit_get_array(avr, p->cs, ARRAY_SIZE(p->cs)) != cs ||
			avr_regbit_get_array(avr, p->wgm, ARRAY_SIZE(p->wgm)) != mode ||
					avr_regbit_get(avr, p->as2) != as2) {
		avr_timer_reconfigure(p);
	}
}

/*
 * write to the TIFR register. Watch for code that writes "1" to clear
 * pending interrupts.
 */
static void avr_timer_write_pending(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_timer_t * p = (avr_timer_t *)param;
	// save old bits values
	uint8_t ov = avr_regbit_get(avr, p->overflow.raised);
	uint8_t ic = avr_regbit_get(avr, p->icr.raised);
	uint8_t cp[AVR_TIMER_COMP_COUNT];

	for (int compi = 0; compi < AVR_TIMER_COMP_COUNT; compi++)
		cp[compi] = avr_regbit_get(avr, p->comp[compi].interrupt.raised);

	// write the value
	avr_core_watch_write(avr, addr, v);

	// clear any interrupts & flags
	avr_clear_interrupt_if(avr, &p->overflow, ov);
	avr_clear_interrupt_if(avr, &p->icr, ic);

	for (int compi = 0; compi < AVR_TIMER_COMP_COUNT; compi++)
		avr_clear_interrupt_if(avr, &p->comp[compi].interrupt, cp[compi]);
}

static void avr_timer_irq_icp(struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_timer_t * p = (avr_timer_t *)param;
	avr_t * avr = p->io.avr;

	// input capture disabled when ICR is used as top
	if (p->mode.top == avr_timer_wgm_reg_icr)
		return;
	int bing = 0;
	if (avr_regbit_get(avr, p->ices)) { // rising edge
		if (!irq->value && value)
			bing++;
	} else {	// default, falling edge
		if (irq->value && !value)
			bing++;
	}
	if (!bing)
		return;
	// get current TCNT, copy it to ICR, and raise interrupt
	uint16_t tcnt = _avr_timer_get_current_tcnt(p);
	avr->data[p->r_icr] = tcnt;
	if (p->r_icrh)
		avr->data[p->r_icrh] = tcnt >> 8;
	avr_raise_interrupt(avr, &p->icr);
}

static void avr_timer_reset(avr_io_t * port)
{
	avr_timer_t * p = (avr_timer_t *)port;
	avr_cycle_timer_cancel(p->io.avr, avr_timer_tov, p);
	avr_cycle_timer_cancel(p->io.avr, avr_timer_compa, p);
	avr_cycle_timer_cancel(p->io.avr, avr_timer_compb, p);
	avr_cycle_timer_cancel(p->io.avr, avr_timer_compc, p);

	// check to see if the comparators have a pin output. If they do,
	// (try) to get the ioport corresponding IRQ and connect them
	// they will automagically be triggered when the comparator raises
	// it's own IRQ
	for (int compi = 0; compi < AVR_TIMER_COMP_COUNT; compi++) {
		p->comp[compi].comp_cycles = 0;

		avr_ioport_getirq_t req = {
			.bit = p->comp[compi].com_pin
		};
		if (avr_ioctl(port->avr, AVR_IOCTL_IOPORT_GETIRQ_REGBIT, &req) > 0) {
			// cool, got an IRQ
//			printf("%s-%c COMP%c Connecting PIN IRQ %d\n", __FUNCTION__, p->name, 'A'+compi, req.irq[0]->irq);
			avr_connect_irq(&port->irq[TIMER_IRQ_OUT_COMP + compi], req.irq[0]);
		}
	}
	avr_ioport_getirq_t req = {
		.bit = p->icp
	};
	if (avr_ioctl(port->avr, AVR_IOCTL_IOPORT_GETIRQ_REGBIT, &req) > 0) {
		// cool, got an IRQ for the input capture pin
//		printf("%s-%c ICP Connecting PIN IRQ %d\n", __FUNCTION__, p->name, req.irq[0]->irq);
		avr_irq_register_notify(req.irq[0], avr_timer_irq_icp, p);
	}

}

static const char * irq_names[TIMER_IRQ_COUNT] = {
	[TIMER_IRQ_OUT_PWM0] = "8>pwm0",
	[TIMER_IRQ_OUT_PWM1] = "8>pwm1",
	[TIMER_IRQ_OUT_COMP + 0] = ">compa",
	[TIMER_IRQ_OUT_COMP + 1] = ">compb",
	[TIMER_IRQ_OUT_COMP + 2] = ">compc",
};

static	avr_io_t	_io = {
	.kind = "timer",
	.reset = avr_timer_reset,
	.irq_names = irq_names,
};

void avr_timer_init(avr_t * avr, avr_timer_t * p)
{
	p->io = _io;

	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->overflow);
	avr_register_vector(avr, &p->icr);

	// allocate this module's IRQ
	avr_io_setirqs(&p->io, AVR_IOCTL_TIMER_GETIRQ(p->name), TIMER_IRQ_COUNT, NULL);

	// marking IRQs as "filtered" means they don't propagate if the
	// new value raised is the same as the last one.. in the case of the
	// pwm value it makes sense not to bother.
	p->io.irq[TIMER_IRQ_OUT_PWM0].flags |= IRQ_FLAG_FILTERED;
	p->io.irq[TIMER_IRQ_OUT_PWM1].flags |= IRQ_FLAG_FILTERED;

	if (p->wgm[0].reg) // these are not present on older AVRs
		avr_register_io_write(avr, p->wgm[0].reg, avr_timer_write, p);
	avr_register_io_write(avr, p->cs[0].reg, avr_timer_write, p);

	// this assumes all the "pending" interrupt bits are in the same
	// register. Might not be true on all devices ?
	avr_register_io_write(avr, p->overflow.raised.reg, avr_timer_write_pending, p);

	/*
	 * Even if the timer is 16 bits, we don't care to have watches on the
	 * high bytes because the datasheet says that the low address is always
	 * the trigger.
	 */
	for (int compi = 0; compi < AVR_TIMER_COMP_COUNT; compi++) {
		avr_register_vector(avr, &p->comp[compi].interrupt);

		if (p->comp[compi].r_ocr) // not all timers have all comparators
			avr_register_io_write(avr, p->comp[compi].r_ocr, avr_timer_write_ocr, p);
	}
	avr_register_io_write(avr, p->r_tcnt, avr_timer_tcnt_write, p);
	avr_register_io_read(avr, p->r_tcnt, avr_timer_tcnt_read, p);
}
