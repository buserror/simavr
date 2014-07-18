/*
	sim_cycle_counters.h

	Copyright 2014 Florian Albrechtskirchinger <falbrechtskirchinger@gmail.com>

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
 * Cycle counters allow measuring the execution time of code blocks.
 * This is particularly convenient when used with MCU section macros.
 * The counters attempt to account for overhead incurred by the instructions
 * used to communicate with simavr. This even works when nesting counters.
 * The method employed is quite limited though and not accurate in all cases.
 *
 * Another shortcoming of the current implementation is that any time spent
 * servicing IRQs is included in the measurement.
 */

#pragma once

#include "sim_avr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_CYCLE_COUNTERS	32

typedef struct avr_cycle_counter_t {
	struct avr_cycle_counter_t * prev;
	char name[32];
	avr_cycle_count_t start;
	avr_cycle_count_t overhead;
	uint8_t registered:1;
} avr_cycle_counter_t;

typedef void (*avr_cycle_counter_notify_t)(
	struct avr_t * avr,
	avr_cycle_counter_t * counter,
	avr_cycle_count_t cycles,
	void * param);

typedef struct avr_cycle_counter_pool_t {
	avr_cycle_counter_t counters[MAX_CYCLE_COUNTERS];
	avr_cycle_counter_t * counter_tail;		// Tail pointer to the list of active counters
	struct avr_cycle_counter_hook_t * hook;
} avr_cycle_counter_pool_t;

/*
 * Define a new cycle counter.
 * 'id' is used as numerical handle to subsequently refer to the counter.
 * 'name' can optionally be used to associate a human-readable identifier with
 * the counter.
 */
void
avr_cycle_counter_register(
		struct avr_t * avr,
		uint8_t id,
		const char * name);

void
avr_cycle_counter_unregister(
		struct avr_t * avr,
		uint8_t id);

void
avr_cycle_counter_start(
		struct avr_t * avr,
		uint8_t id);

void
avr_cycle_counter_stop(
		struct avr_t * avr);

/*
 * Register a callback to be invoked when the counter is stopped
 * and a measurement has been obtained.
 */
void
avr_cycle_counter_register_notify(
		struct avr_t * avr,
		avr_cycle_counter_notify_t notify,
		void * param);

void
avr_cycle_counter_unregister_notify(
		struct avr_t * avr,
		avr_cycle_counter_notify_t notify,
		void * param);

// Private functions

void
avr_cycle_counter_init(
		struct avr_t * avr);

// Called from the respective builtin commands
void
avr_cycle_counter_start_with_overhead(
		struct avr_t * avr,
		uint8_t id,
		avr_cycle_count_t overhead);

void
avr_cycle_counter_stop_with_overhead(
		struct avr_t * avr,
		avr_cycle_count_t overhead);

#ifdef __cplusplus
};
#endif
