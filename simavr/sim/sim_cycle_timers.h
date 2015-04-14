/*
	sim_cycle_timers.h

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

/*
 * cycle timers are callbacks that will be called when "when" cycle is reached
 * these timers are one shots, then get cleared if the timer function returns zero,
 * they get reset if the callback function returns a new cycle number
 *
 * the implementation maintains a list of 'pending' timers, sorted by when they
 * should run, it allows very quick comparison with the next timer to run, and
 * quick removal of then from the pile once dispatched.
 */
#ifndef __SIM_CYCLE_TIMERS_H___
#define __SIM_CYCLE_TIMERS_H___

#include "sim_avr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CYCLE_TIMERS	64

typedef avr_cycle_count_t (*avr_cycle_timer_t)(
		struct avr_t * avr,
		avr_cycle_count_t when,
		void * param);

/*
 * Each timer instance contains the absolute cycle number they
 * are hoping to run at, a function pointer to call and a parameter
 * 
 * it will NEVER be the exact cycle specified, as each instruction is
 * not divisible and might take 2 or more cycles anyway.
 * 
 * However if there was a LOT of cycle lag, the timer migth be called
 * repeteadly until it 'caches up'.
 */
typedef struct avr_cycle_timer_slot_t {
	struct avr_cycle_timer_slot_t *next;
	avr_cycle_count_t	when;
	avr_cycle_timer_t	timer;
	void * param;
} avr_cycle_timer_slot_t, *avr_cycle_timer_slot_p;

/*
 * Timer pool contains a pool of timer slots available, they all
 * start queued into the 'free' qeueue, are migrated to the
 * 'active' queue when needed and are re-queued to the free one
 * when done
 */
typedef struct avr_cycle_timer_pool_t {
	avr_cycle_timer_slot_t timer_slots[MAX_CYCLE_TIMERS];
	avr_cycle_timer_slot_p timer_free;
	avr_cycle_timer_slot_p timer;
} avr_cycle_timer_pool_t, *avr_cycle_timer_pool_p;


// register for calling 'timer' in 'when' cycles
void
avr_cycle_timer_register(
		struct avr_t * avr,
		avr_cycle_count_t when,
		avr_cycle_timer_t timer,
		void * param);
// register a timer to call in 'when' usec
void
avr_cycle_timer_register_usec(
		struct avr_t * avr,
		uint32_t when,
		avr_cycle_timer_t timer,
		void * param);
// cancel a previously set timer
void
avr_cycle_timer_cancel(
		struct avr_t * avr,
		avr_cycle_timer_t timer,
		void * param);
/*
 * Check to see if a timer is present, if so, return the number (+1) of
 * cycles left for it to fire, and if not present, return zero
 */
avr_cycle_count_t
avr_cycle_timer_status(
		struct avr_t * avr,
		avr_cycle_timer_t timer,
		void * param);

//
// Private, called from the core
//
avr_cycle_count_t
avr_cycle_timer_process(
		struct avr_t * avr);
void
avr_cycle_timer_reset(
		struct avr_t * avr);

#ifdef __cplusplus
};
#endif

#endif /* __SIM_CYCLE_TIMERS_H___ */
