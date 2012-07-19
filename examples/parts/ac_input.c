/*
	ac_input.c

	Copyright Luki <humbell@ethz.ch>
	Copyright 2011 Michel Pollet <buserror@gmail.com>

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
#include "sim_avr.h"
#include "sim_time.h"
#include "ac_input.h"

#define USECS_PER_SECOND (1000 * 1000)
#define HZ (50)

static avr_cycle_count_t
switch_auto(
		struct avr_t * avr,
        avr_cycle_count_t when,
        void * param)
{
	ac_input_t * b = (ac_input_t *) param;
	b->value = !b->value;
	avr_raise_irq(b->irq + IRQ_AC_OUT, b->value);
	return when + avr_usec_to_cycles(avr, USECS_PER_SECOND / HZ);
}

static const char * name = ">ac_input";

void ac_input_init(avr_t *avr, ac_input_t *b)
{
	b->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_AC_COUNT, &name);
	b->avr = avr;
	b->value = 0;
	avr_cycle_timer_register_usec(avr, USECS_PER_SECOND / HZ, switch_auto, b);
	printf("ac_input_init period %duS or %d cycles\n",
			USECS_PER_SECOND / HZ,
			(int)avr_usec_to_cycles(avr, USECS_PER_SECOND / HZ));
}
