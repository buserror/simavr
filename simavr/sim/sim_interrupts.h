/*
	sim_interrupts.h

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

#ifndef __SIM_INTERUPTS_H__
#define __SIM_INTERUPTS_H__

#include "sim_avr.h"

// interrupt vector for the IO modules
typedef struct avr_int_vector_t {
	uint8_t vector;		// vector number, zero (reset) is reserved

	avr_regbit_t enable;	// IO register index for the "interrupt enable" flag for this vector
	avr_regbit_t raised;	// IO register index for the register where the "raised" flag is (optional)
} avr_int_vector_t;


/*
 * Interrupt Helper Functions
 */
// register an interrupt vector. It's only needed if you want to use the "r_raised" flags
void avr_register_vector(avr_t *avr, avr_int_vector_t * vector);
// raise an interrupt (if enabled). The interrupt is latched and will be called later
// return non-zero if the interrupt was raised and is now pending
int avr_raise_interrupt(avr_t * avr, avr_int_vector_t * vector);
// return non-zero if the AVR core has any pending interrupts
int avr_has_pending_interrupts(avr_t * avr);
// return nonzero if a specific interrupt vector is pending
int avr_is_interrupt_pending(avr_t * avr, avr_int_vector_t * vector);
// clear the "pending" status of an interrupt
void avr_clear_interrupt(avr_t * avr, int v);
// called by the core at each cycle to check whether an interrupt is pending
void avr_service_interrupts(avr_t * avr);

#endif /* __SIM_INTERUPTS_H__ */
