/*
	avr_acomp.c

	Copyright 2017 Konstantin Begun

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

#include <stdlib.h>
#include "avr_acomp.h"
#include "avr_timer.h"

static uint8_t
avr_acomp_get_state(
		struct avr_t * avr,
		avr_acomp_t *ac)
{
	if (avr_regbit_get(avr, ac->disabled))
		return 0;

	// get positive voltage
	uint16_t positive_v;

	if (avr_regbit_get(avr, ac->acbg)) {		// if bandgap
		positive_v = ACOMP_BANDGAP;
	} else {
		positive_v = ac->ain_values[0];	// AIN0
	}

	// get negative voltage
	uint16_t negative_v = 0;

	// multiplexer is enabled if acme is set and adc is off
	if (avr_regbit_get(avr, ac->acme) && !avr_regbit_get(avr, ac->aden)) {
		if (!avr_regbit_get(avr, ac->pradc)) {
			uint8_t adc_i = avr_regbit_get_array(avr, ac->mux, ARRAY_SIZE(ac->mux));
			if (adc_i < ac->mux_inputs && adc_i < ARRAY_SIZE(ac->adc_values)) {
				negative_v = ac->adc_values[adc_i];
			}
		}

	} else {
		negative_v = ac->ain_values[1];	// AIN1
	}

	return positive_v > negative_v;
}

static avr_cycle_count_t
avr_acomp_sync_state(
	struct avr_t * avr,
	avr_cycle_count_t when,
	void * param)
{
	avr_acomp_t * p = (avr_acomp_t *)param;
	if (!avr_regbit_get(avr, p->disabled)) {

		uint8_t cur_state = avr_regbit_get(avr, p->aco);
		uint8_t new_state = avr_acomp_get_state(avr, p);

		if (new_state != cur_state) {
			avr_regbit_setto(avr, p->aco, new_state);		// set ACO

			uint8_t acis0 = avr_regbit_get(avr, p->acis[0]);
			uint8_t acis1 = avr_regbit_get(avr, p->acis[1]);

			if ((acis0 == 0 && acis1 == 0) || (acis1 == 1 && acis0 == new_state)) {
				avr_raise_interrupt(avr, &p->ac);
			}

			avr_raise_irq(p->io.irq + ACOMP_IRQ_OUT, new_state);
		}

	}

	return 0;
}

static inline void
avr_schedule_sync_state(
	struct avr_t * avr,
	void *param)
{
	avr_cycle_timer_register(avr, 1, avr_acomp_sync_state, param);
}

static void
avr_acomp_write_acsr(
	struct avr_t * avr,
	avr_io_addr_t addr,
	uint8_t v,
	void * param)
{
	avr_acomp_t * p = (avr_acomp_t *)param;

	avr_core_watch_write(avr, addr, v);

	if (avr_regbit_get(avr, p->acic) != (p->timer_irq ? 1:0)) {
		if (p->timer_irq) {
			avr_unconnect_irq(p->io.irq + ACOMP_IRQ_OUT, p->timer_irq);
			p->timer_irq = NULL;
		}
		else {
			avr_irq_t *irq = avr_io_getirq(avr, AVR_IOCTL_TIMER_GETIRQ(p->timer_name), TIMER_IRQ_IN_ICP);
			if (irq) {
				avr_connect_irq(p->io.irq + ACOMP_IRQ_OUT, irq);
				p->timer_irq = irq;
			}
		}
	}

	avr_schedule_sync_state(avr, param);
}

static void
avr_acomp_dependencies_changed(
	struct avr_irq_t * irq,
	uint32_t value,
	void * param)
{
	avr_acomp_t * p = (avr_acomp_t *)param;
	avr_schedule_sync_state(p->io.avr, param);
}

static void
avr_acomp_irq_notify(
	struct avr_irq_t * irq,
	uint32_t value,
	void * param)
{
	avr_acomp_t * p = (avr_acomp_t *)param;

	switch (irq->irq) {
		case ACOMP_IRQ_AIN0 ... ACOMP_IRQ_AIN1: {
				p->ain_values[irq->irq - ACOMP_IRQ_AIN0] = value;
				avr_schedule_sync_state(p->io.avr, param);
			} 	break;
		case ACOMP_IRQ_ADC0 ... ACOMP_IRQ_ADC15: {
				p->adc_values[irq->irq - ACOMP_IRQ_ADC0] = value;
				avr_schedule_sync_state(p->io.avr, param);
			} 	break;
	}
}

static void
avr_acomp_register_dependencies(
	avr_acomp_t *p,
	avr_regbit_t rb)
{
	if (rb.reg) {
		avr_irq_register_notify(
					avr_iomem_getirq(p->io.avr, rb.reg, NULL, rb.bit),
					avr_acomp_dependencies_changed,
					p);
	}
}

static void
avr_acomp_reset(avr_io_t * port)
{
	avr_acomp_t * p = (avr_acomp_t *)port;

	for (int i = 0; i < ACOMP_IRQ_COUNT; i++)
		avr_irq_register_notify(p->io.irq + i, avr_acomp_irq_notify, p);

	// register notification for changes of registers comparator does not own
	// avr_register_io_write is tempting instead, but it requires that the handler
	// updates the actual memory too. Given this is for the registers this module
	// does not own, it is tricky to know whether it should write to the actual memory.
	// E.g., if there is already a native handler for it then it will do the writing
	// (possibly even omitting some bits etc). IInterefering would probably be wrong.
	// On the  other hand if there isn't a handler already, then this hadnler would have to,
	// as otherwise nobody will.
	// This write notification mechanism should probably need reviewing and fixing
	// For now using IRQ mechanism, as it is not intrusive

	avr_acomp_register_dependencies(p, p->pradc);
	avr_acomp_register_dependencies(p, p->aden);
	avr_acomp_register_dependencies(p, p->acme);

	// mux
	for (int i = 0; i < ARRAY_SIZE(p->mux); ++i) {
		avr_acomp_register_dependencies(p, p->mux[i]);
	}
}

static const char * irq_names[ACOMP_IRQ_COUNT] = {
	[ACOMP_IRQ_AIN0] = "16<ain0",
	[ACOMP_IRQ_AIN1] = "16<ain1",
	[ACOMP_IRQ_ADC0] = "16<adc0",
	[ACOMP_IRQ_ADC1] = "16<adc1",
	[ACOMP_IRQ_ADC2] = "16<adc2",
	[ACOMP_IRQ_ADC3] = "16<adc3",
	[ACOMP_IRQ_ADC4] = "16<adc4",
	[ACOMP_IRQ_ADC5] = "16<adc5",
	[ACOMP_IRQ_ADC6] = "16<adc6",
	[ACOMP_IRQ_ADC7] = "16<adc7",
	[ACOMP_IRQ_ADC8] = "16<adc0",
	[ACOMP_IRQ_ADC9] = "16<adc9",
	[ACOMP_IRQ_ADC10] = "16<adc10",
	[ACOMP_IRQ_ADC11] = "16<adc11",
	[ACOMP_IRQ_ADC12] = "16<adc12",
	[ACOMP_IRQ_ADC13] = "16<adc13",
	[ACOMP_IRQ_ADC14] = "16<adc14",
	[ACOMP_IRQ_ADC15] = "16<adc15",
	[ACOMP_IRQ_OUT] = ">out"
};

static avr_io_t _io = {
	.kind = "ac",
	.reset = avr_acomp_reset,
	.irq_names = irq_names,
};

void
avr_acomp_init(
	avr_t * avr,
	avr_acomp_t * p)
{
	p->io = _io;

	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->ac);
	// allocate this module's IRQ
	avr_io_setirqs(&p->io, AVR_IOCTL_ACOMP_GETIRQ, ACOMP_IRQ_COUNT, NULL);

	avr_register_io_write(avr, p->r_acsr, avr_acomp_write_acsr, p);
}
