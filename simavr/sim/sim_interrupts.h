/*
	sim_interrupts.h

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

#ifndef __SIM_INTERRUPTS_H__
#define __SIM_INTERRUPTS_H__

#include "sim_avr_types.h"
#include "sim_irq.h"
#include "fifo_declare.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	AVR_INT_IRQ_PENDING = 0,
	AVR_INT_IRQ_RUNNING,
	AVR_INT_IRQ_COUNT,
	AVR_INT_ANY		= 0xff,	// for avr_get_interrupt_irq()
};
// interrupt vector for the IO modules
typedef struct avr_int_vector_t {
	uint8_t 		vector;			// vector number, zero (reset) is reserved
	avr_regbit_t 	enable;			// IO register index for the "interrupt enable" flag for this vector
	avr_regbit_t 	raised;			// IO register index for the register where the "raised" flag is (optional)

	// 'pending' IRQ, and 'running' status as signaled here
	avr_irq_t		irq[AVR_INT_IRQ_COUNT];
	uint8_t			pending : 1,	// 1 while scheduled in the fifo
					trace : 1,		// only for debug of a vector
					raise_sticky : 1;	// 1 if the interrupt flag (= the raised regbit) is not cleared
										// by the hardware when executing the interrupt routine (see TWINT)
} avr_int_vector_t, *avr_int_vector_p;

// Size needs to be >= max number of vectors, and a power of two
DECLARE_FIFO(avr_int_vector_p, avr_int_pending, 64);

// interrupt vectors, and their enable/clear registers
typedef struct  avr_int_table_t {
	avr_int_vector_t * vector[64];
	uint8_t			vector_count;
	avr_int_pending_t pending;
	uint8_t			running_ptr;
	avr_int_vector_t *running[64]; // stack of nested interrupts
	// global status for pending + running in interrupt context
	avr_irq_t		irq[AVR_INT_IRQ_COUNT];
} avr_int_table_t, *avr_int_table_p;

/*
 * Interrupt Helper Functions
 */
// register an interrupt vector. It's only needed if you want to use the "r_raised" flags
void
avr_register_vector(
		struct avr_t *avr,
		avr_int_vector_t * vector);
// raise an interrupt (if enabled). The interrupt is latched and will be called later
// return non-zero if the interrupt was raised and is now pending
int
avr_raise_interrupt(
		struct avr_t * avr,
		avr_int_vector_t * vector);
// return non-zero if the AVR core has any pending interrupts
int
avr_has_pending_interrupts(
		struct avr_t * avr);
// return nonzero if a specific interrupt vector is pending
int
avr_is_interrupt_pending(
		struct avr_t * avr,
		avr_int_vector_t * vector);
// clear the "pending" status of an interrupt
void
avr_clear_interrupt(
		struct avr_t * avr,
		avr_int_vector_t * vector);
// called by the core at each cycle to check whether an interrupt is pending
void
avr_service_interrupts(
		struct avr_t * avr);
// called by the core when RETI opcode is ran
void
avr_interrupt_reti(
		struct avr_t * avr);
// clear the interrupt (inc pending) if "raised" flag is 1
int
avr_clear_interrupt_if(
		struct avr_t * avr,
		avr_int_vector_t * vector,
		uint8_t old);

// return the IRQ that is raised when the vector is enabled and called/cleared
// this allows tracing of pending interrupts
avr_irq_t *
avr_get_interrupt_irq(
		struct avr_t * avr,
		uint8_t v);

// Initializes the interrupt table
void
avr_interrupt_init(
		struct avr_t * avr );

// reset the interrupt table and the fifo
void
avr_interrupt_reset(
		struct avr_t * avr );

#ifdef __cplusplus
};
#endif

#endif /* __SIM_INTERRUPTS_H__ */
