/*
	sim_cycle_timers.h

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


#ifndef __SIM_CYCLE_TIMERS_H___
#define __SIM_CYCLE_TIMERS_H___

#include "sim_avr.h"

#ifdef __cplusplus
extern "C" {
#endif

// converts a number of usec to a number of machine cycles, at current speed
static inline avr_cycle_count_t avr_usec_to_cycles(avr_t * avr, uint32_t usec)
{
	return avr->frequency * (avr_cycle_count_t)usec / 1000000;
}

// converts back a number of cycles to usecs (for usleep)
static inline uint32_t avr_cycles_to_usec(avr_t * avr, avr_cycle_count_t cycles)
{
	return 1000000L * cycles / avr->frequency;
}

// converts back a number of cycles to nsecs
static inline uint64_t avr_cycles_to_nsec(avr_t * avr, avr_cycle_count_t cycles)
{
	return (uint64_t)1E6 * (uint64_t)cycles / (avr->frequency/1000);
}

// converts a number of hz (to megahertz etc) to a number of cycle
static inline avr_cycle_count_t avr_hz_to_cycles(avr_t * avr, uint32_t hz)
{
	return avr->frequency / hz;
}

// register for calling 'timer' in 'when' cycles
void avr_cycle_timer_register(avr_t * avr, avr_cycle_count_t when, avr_cycle_timer_t timer, void * param);
// register a timer to call in 'when' usec
void avr_cycle_timer_register_usec(avr_t * avr, uint32_t when, avr_cycle_timer_t timer, void * param);
// cancel a previously set timer
void avr_cycle_timer_cancel(avr_t * avr, avr_cycle_timer_t timer, void * param);
/*
 * Check to see if a timer is present, if so, return the number (+1) of
 * cycles left for it to fire, and if not present, return zero
 */
avr_cycle_count_t
avr_cycle_timer_status(avr_t * avr, avr_cycle_timer_t timer, void * param);


//
// Private, called from the core
//
avr_cycle_count_t avr_cycle_timer_process(avr_t * avr);

#ifdef __cplusplus
};
#endif

#endif /* __SIM_CYCLE_TIMERS_H___ */
