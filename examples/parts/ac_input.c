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
#include "parts_logger.h"

#define USECS_PER_SECOND (1000 * 1000)
#define HZ (50)

static avr_cycle_count_t
switch_auto(
		avr_cycle_timer_pool_t * pool,
        avr_cycle_count_t when,
        void * param)
{
	ac_input_t * b = (ac_input_t *) param;
	b->value = !b->value;
	avr_raise_irq(b->irq + IRQ_AC_OUT, b->value);
	return when + avr_usec_to_cycles(pool->clock, USECS_PER_SECOND / HZ);
}

static const char * name = "8>ac.input.output";

void ac_input_init(avr_t *avr, ac_input_t *b) {
	ac_input_initialize(&(avr->irq_pool),&(avr->cycle_timers),b);
}
void ac_input_initialize(avr_irq_pool_t *irq_pool, avr_cycle_timer_pool_t * cycle_timers,ac_input_t *b)
{
	b->irq = avr_alloc_irq(irq_pool, 0, IRQ_AC_COUNT, &name);
	b->logger.level = LOG_ERROR;
	ac_reset(cycle_timers,b);
}
void ac_reset(avr_cycle_timer_pool_t * cycle_timers,ac_input_t * b) {
	b->value = 0;
	avr_cycle_timer_register_usec(cycle_timers, USECS_PER_SECOND / HZ, switch_auto, b);
	parts_global_logger(&(b->logger),LOG_OUTPUT,"ac_input_init period %duS or %d cycles\n",
			USECS_PER_SECOND / HZ,
			(int)avr_usec_to_cycles(cycle_timers->clock, USECS_PER_SECOND / HZ));
#if 0
	printf("ac_input_init period %duS or %d cycles\n",
			USECS_PER_SECOND / HZ,
			(int)avr_usec_to_cycles(cycle_timers->clock, USECS_PER_SECOND / HZ));
#endif
}
