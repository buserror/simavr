/*
	ac_input.h

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

/*
 * Simulates a 50hz changing signal
 */

#ifndef __AC_INPUT_H__
#define __AC_INPUT_H__

#include "sim_irq.h"
#include "parts_logger.h"


enum {
    IRQ_AC_OUT = 0,
    IRQ_AC_COUNT
};

typedef struct ac_input_t {
    avr_irq_t * irq;
    uint8_t value;
	parts_logger_t logger;
} ac_input_t;

void
ac_input_init(
			struct avr_t * avr,
			ac_input_t * b);

void
ac_input_initialize(
			avr_irq_pool_t * irq_pool,
			avr_cycle_timer_pool_t * cycle_timers,
			ac_input_t * b);

void ac_reset(avr_cycle_timer_pool_t * cycle_timers,ac_input_t * b);
#endif
