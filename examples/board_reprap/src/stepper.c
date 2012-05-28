/*
	stepper.c

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

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
#include <string.h>
#include <stdio.h>

#include "sim_avr.h"
#include "sim_time.h"
#include "stepper.h"

static avr_cycle_count_t
stepper_update_timer(
		struct avr_t * avr,
        avr_cycle_count_t when,
        void * param)
{
	stepper_p p = (stepper_p)param;
	union {
		float f;
		uint32_t i;
	} m = { .f = p->position / p->steps_per_mm };
//	printf("%s (%s) %3.4f\n", __func__, p->name, m.f);
	avr_raise_irq(p->irq + IRQ_STEPPER_POSITION_OUT, m.i);
	avr_raise_irq(p->irq + IRQ_STEPPER_ENDSTOP_OUT, p->position == p->endstop);
	return when + p->timer_period;
}

static void
stepper_dir_hook(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param )
{
	stepper_p p = (stepper_p)param;
	printf("%s (%s) %d\n", __func__, p->name, value);
	p->dir = !!value;
}

static void
stepper_enable_hook(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param )
{
	stepper_p p = (stepper_p)param;
	p->enable = !!value;
	printf("%s (%s) %d pos %.4f\n", __func__, p->name,
			p->enable != 0, p->position / p->steps_per_mm);
	avr_raise_irq(p->irq + IRQ_STEPPER_ENDSTOP_OUT, p->position == p->endstop);
}

static void
stepper_step_hook(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param )
{
	stepper_p p = (stepper_p)param;
	if (!p->enable)
		return;
	if (value)
		return;
	p->position += !p->dir && p->position == 0 ? 0 : p->dir ? 1 : -1;
	if (p->endstop && p->position < p->endstop)
		p->position = p->endstop;
	if (p->max_position > 0 && p->position > p->max_position)
		p->position = p->max_position;
}

static const char * irq_names[IRQ_STEPPER_COUNT] = {
	[IRQ_STEPPER_DIR_IN] = "1<stepper.direction",
	[IRQ_STEPPER_STEP_IN] = "1>stepper.step",
	[IRQ_STEPPER_ENABLE_IN] = "1<stepper.enable",
	[IRQ_STEPPER_POSITION_OUT] = "32<stepper.position",
	[IRQ_STEPPER_ENDSTOP_OUT] = "1<stepper.endstop",
};

void
stepper_init(
		struct avr_t * avr,
		stepper_p p,
		char * name,
		float steps_per_mm,
		float start_position,
		float max_position,
		float endstop_position)
{
	p->avr = avr;
	strcpy(p->name, name);
	p->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_STEPPER_COUNT, irq_names);
	avr_irq_register_notify(p->irq + IRQ_STEPPER_DIR_IN, stepper_dir_hook, p);
	avr_irq_register_notify(p->irq + IRQ_STEPPER_STEP_IN, stepper_step_hook, p);
	avr_irq_register_notify(p->irq + IRQ_STEPPER_ENABLE_IN, stepper_enable_hook, p);

	p->steps_per_mm = steps_per_mm;
	p->position = start_position * p->steps_per_mm;
	p->max_position = max_position * p->steps_per_mm;
	p->endstop = endstop_position >= 0 ? endstop_position * p->steps_per_mm : 0;
}

void
stepper_connect(
		stepper_p p,
		avr_irq_t *	step,
		avr_irq_t *	dir,
		avr_irq_t *	enable,
		avr_irq_t *	endstop,
		uint16_t flags)
{
	avr_connect_irq(step, p->irq + IRQ_STEPPER_STEP_IN);
	avr_connect_irq(dir, p->irq + IRQ_STEPPER_DIR_IN);
	avr_connect_irq(enable, p->irq + IRQ_STEPPER_ENABLE_IN);
	p->irq[IRQ_STEPPER_ENDSTOP_OUT].flags |= IRQ_STEPPER_POSITION_OUT;
	p->irq[IRQ_STEPPER_ENDSTOP_OUT].flags |= IRQ_FLAG_FILTERED;
	if (endstop) {
		avr_connect_irq(p->irq + IRQ_STEPPER_ENDSTOP_OUT, endstop);
		if (flags & stepper_endstop_inverted)
			p->irq[IRQ_STEPPER_ENDSTOP_OUT].flags |= IRQ_FLAG_NOT;
	}
	p->timer_period = avr_usec_to_cycles(p->avr, 100000 / 1000); // 1ms
	avr_cycle_timer_register(p->avr, p->timer_period, stepper_update_timer, p);
}

float
stepper_get_position_mm(
		stepper_p p)
{
	return p->position / p->steps_per_mm;
}

