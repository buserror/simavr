/*
	sim_cycle_counters.c

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_avr.h"
#include "sim_time.h"
#include "sim_cycle_counters.h"

#define LOG_PREFIX		"CYCLE_COUNTERS: "

// Internal struct for the notify callbacks
typedef struct avr_cycle_counter_hook_t {
	struct avr_cycle_counter_hook_t * next;
	avr_cycle_counter_notify_t notify;
	void * param;
} avr_cycle_counter_hook_t;

void
avr_cycle_counter_init(
		avr_t * avr)
{
	memset(&avr->cycle_counters, 0, sizeof(avr->cycle_counters));
}

void
avr_cycle_counter_register(
		avr_t * avr,
		uint8_t id,
		const char * name)
{
	avr_cycle_counter_pool_t * pool = &avr->cycle_counters;
	avr_cycle_counter_t * counter;

	if(id >= MAX_CYCLE_COUNTERS) {
		AVR_LOG(avr, LOG_ERROR, LOG_PREFIX "%s: id 0x%02x outside permissible range (>0x%02x)\n", __FUNCTION__, id, MAX_CYCLE_COUNTERS - 1);
		return;
	}

	counter = &pool->counters[id];
	if(counter->registered) {
		AVR_LOG(avr, LOG_ERROR, LOG_PREFIX "%s: counter with ID %d ('%s') already registered\n", __FUNCTION__, id, counter->name);
		return;
	}

	AVR_LOG(avr, LOG_TRACE, LOG_PREFIX "%s: register cycle counter '%s' (%d)\n", __FUNCTION__, name, id);

	memset(counter, 0, sizeof(*counter));
	if(name) {
		strncpy(counter->name, name, sizeof(counter->name));
		counter->name[sizeof(counter->name) - 1] = 0;
	}

	counter->registered = 1;
}

void
avr_cycle_counter_unregister(
		avr_t * avr,
		uint8_t id)
{
	avr_cycle_counter_pool_t * pool = &avr->cycle_counters;
	avr_cycle_counter_t * counter;

	if(id >= MAX_CYCLE_COUNTERS) {
		AVR_LOG(avr, LOG_ERROR, LOG_PREFIX "%s: id 0x%02x outside permissible range (>0x%02x)\n", __FUNCTION__, id, MAX_CYCLE_COUNTERS - 1);
		return;
	}

	counter = &pool->counters[id];
	if(!counter->registered) {
		AVR_LOG(avr, LOG_ERROR, LOG_PREFIX "%s: no counter with ID %d registered\n", __FUNCTION__, id);
		return;
	}

	AVR_LOG(avr, LOG_TRACE, LOG_PREFIX "%s: unregister cycle counter '%s' (%d)\n", __FUNCTION__, counter->name, id);

	counter->registered = 0;
}

static void
_avr_cycle_counter_start(
		avr_t * avr,
		uint8_t id,
		avr_cycle_count_t overhead)
{
	avr_cycle_counter_pool_t * pool = &avr->cycle_counters;
	avr_cycle_counter_t * counter;

	if(id >= MAX_CYCLE_COUNTERS) {
		AVR_LOG(avr, LOG_ERROR, LOG_PREFIX "%s: id 0x%02x outside permissible range (>0x%02x)\n", __FUNCTION__, id, MAX_CYCLE_COUNTERS - 1);
		return;
	}

	counter = &pool->counters[id];
	if(!counter->registered) {
		AVR_LOG(avr, LOG_ERROR, LOG_PREFIX "%s: counter with id %d not registered\n", __FUNCTION__, id);
		return;
	}

	counter->start = avr->cycle;
	// The overhead for this counter only consist of 1 'out'
	counter->overhead = overhead ? 1 : 0;

	// Append the counter to the list of active counters
	counter->prev = pool->counter_tail;
	pool->counter_tail = counter;

	// Propagate the overhead through the list of active counters
	counter = counter->prev;
	while(overhead && counter) {
		counter->overhead += overhead;

		counter = counter->prev;
	}
}

void
avr_cycle_counter_start(
		avr_t * avr,
		uint8_t id)
{
	_avr_cycle_counter_start(avr, id, 0);
}

void
avr_cycle_counter_start_with_overhead(
		avr_t * avr,
		uint8_t id,
		avr_cycle_count_t overhead)
{
	_avr_cycle_counter_start(avr, id, overhead);
}

static void
_avr_cycle_counter_stop(
		avr_t * avr,
		avr_cycle_count_t overhead)
{
	avr_cycle_count_t cycles;
	avr_cycle_counter_pool_t * pool = &avr->cycle_counters;
	avr_cycle_counter_t * counter = pool->counter_tail;
	avr_cycle_counter_hook_t * hook = pool->hook;

	if(!counter) {
		AVR_LOG(avr, LOG_ERROR, LOG_PREFIX "%s: no counters are currently active\n", __FUNCTION__);
		return;
	}

	cycles = avr->cycle - counter->start - counter->overhead;

	AVR_LOG(avr, LOG_TRACE, LOG_PREFIX "%s: counter '%s' measured %" PRI_avr_cycle_count " cycle(s) (overhead %" PRI_avr_cycle_count " cycle(s))\n", __FUNCTION__, counter->name, cycles, counter->overhead);

	// Invoke callbacks
	while(hook) {
		if(hook->notify)
			hook->notify(avr, counter, cycles, hook->param);

		hook = hook->next;
	}

	// Remove the counter from the list of active counters
	pool->counter_tail = counter = counter->prev;

	// Propagate the overhead through the list of active counter
	while(overhead && counter) {
		counter->overhead += overhead;

		counter = counter->prev;
	}
}

void
avr_cycle_counter_stop(
		avr_t * avr)
{
	_avr_cycle_counter_stop(avr, 0);
}

void
avr_cycle_counter_stop_with_overhead(
		avr_t * avr,
		avr_cycle_count_t overhead)
{
	_avr_cycle_counter_stop(avr, overhead);
}

void
avr_cycle_counter_register_notify(
		avr_t * avr,
		avr_cycle_counter_notify_t notify,
		void * param)
{
	avr_cycle_counter_pool_t * pool = &avr->cycle_counters;
	avr_cycle_counter_hook_t * hook;

	if(!notify)
		return;

	hook = malloc(sizeof(*hook));

	hook->notify = notify;
	hook->param = param;

	hook->next = pool->hook;
	pool->hook = hook;
}

void
avr_cycle_counter_unregister_notify(
		avr_t * avr,
		avr_cycle_counter_notify_t notify,
		void * param)
{
	avr_cycle_counter_pool_t * pool = &avr->cycle_counters;
	avr_cycle_counter_hook_t * hook, * prev = NULL;

	if(!notify)
		return;

	hook = pool->hook;
	while(hook) {
		if(hook->notify == notify && hook->param == param) {
			if(prev)
				prev->next = hook->next;
			else
				pool->hook = hook->next;

			free(hook);

			return;
		}

		prev = hook;
		hook = hook->next;
	}

	AVR_LOG(avr, LOG_ERROR, LOG_PREFIX "%s: cannot unregister non-existent counter notify\n", __FUNCTION__);
}
