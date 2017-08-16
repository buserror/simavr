/*
	sim_time.h

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


#ifndef __SIM_TIME_H___
#define __SIM_TIME_H___

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_avr.h"

// converts a number of usec to a number of machine cycles, at current speed
static inline avr_cycle_count_t
avr_usec_to_cycles(struct avr_clock_t * clock, uint32_t usec)
{
	return clock->frequency * (avr_cycle_count_t)usec / 1000000;
}

// converts back a number of cycles to usecs (for usleep)
static inline uint32_t
avr_cycles_to_usec(struct avr_clock_t * clock, avr_cycle_count_t cycles)
{
	return 1000000L * cycles / clock->frequency;
}

// converts back a number of cycles to nsecs
static inline uint64_t
avr_cycles_to_nsec(struct avr_clock_t * clock, avr_cycle_count_t cycles)
{
	return (uint64_t)1E6 * (uint64_t)cycles / (clock->frequency/1000);
}

// converts a number of hz (to megahertz etc) to a number of cycle
static inline avr_cycle_count_t
avr_hz_to_cycles(avr_clock_t * clock, uint32_t hz)
{
	return clock->frequency / hz;
}

#ifdef __cplusplus
};
#endif

#endif /* __SIM_TIME_H___ */
