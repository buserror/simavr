/*
	sim_cycle_timers.c

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sim_avr.h"
#include "sim_time.h"
#include "sim_cycle_timers.h"

#define QUEUE(__q, __e) { \
		(__e)->next = (__q); \
		(__q) = __e; \
	}
#define DETACH(__q, __l, __e) { \
		if (__l) \
			(__l)->next = (__e)->next; \
		else \
			(__q) = (__e)->next; \
	}
#define INSERT(__q, __l, __e) { \
		if (__l) { \
			(__e)->next = (__l)->next; \
			(__l)->next = (__e); \
		} else { \
			(__e)->next = (__q); \
			(__q) = (__e); \
		} \
	}

void
avr_cycle_timer_reset(
		struct avr_t * avr)
{
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;
	memset(pool, 0, sizeof(*pool));
	// queue all slots into the free queue
	for (int i = 0; i < MAX_CYCLE_TIMERS; i++) {
		avr_cycle_timer_slot_p t = &pool->timer_slots[i];
		QUEUE(pool->timer_free, t);
	}
}

// no sanity checks checking here, on purpose
static void
avr_cycle_timer_insert(
		avr_t * avr,
		avr_cycle_count_t when,
		avr_cycle_timer_t timer,
		void * param)
{
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;

	when += avr->cycle;

	avr_cycle_timer_slot_p t = pool->timer_free;

	if (!t) {
		AVR_LOG(avr, LOG_ERROR, "CYCLE: %s: ran out of timers (%d)!\n", __func__, MAX_CYCLE_TIMERS);
		return;
	}
	// detach head
	pool->timer_free = t->next;
	t->next = NULL;
	t->timer = timer;
	t->param = param;
	t->when = when;

	// find its place in the list
	avr_cycle_timer_slot_p loop = pool->timer, last = NULL;
	while (loop) {
		if (loop->when > when)
			break;
		last = loop;
		loop = loop->next;
	}
	INSERT(pool->timer, last, t);
}

void
avr_cycle_timer_register(
		avr_t * avr,
		avr_cycle_count_t when,
		avr_cycle_timer_t timer,
		void * param)
{
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;

	// remove it if it was already scheduled
	avr_cycle_timer_cancel(avr, timer, param);

	if (!pool->timer_free) {
		AVR_LOG(avr, LOG_ERROR, "CYCLE: %s: pool is full (%d)!\n", __func__, MAX_CYCLE_TIMERS);
		return;
	}
	avr_cycle_timer_insert(avr, when, timer, param);
}

void
avr_cycle_timer_register_usec(
		avr_t * avr,
		uint32_t when,
		avr_cycle_timer_t timer,
		void * param)
{
	avr_cycle_timer_register(avr, avr_usec_to_cycles(avr, when), timer, param);
}

void
avr_cycle_timer_cancel(
		avr_t * avr,
		avr_cycle_timer_t timer,
		void * param)
{
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;

	// find its place in the list
	avr_cycle_timer_slot_p t = pool->timer, last = NULL;
	while (t) {
		if (t->timer == timer && t->param == param) {
			DETACH(pool->timer, last, t);
			QUEUE(pool->timer_free, t);
			break;
		}
		last = t;
		t = t->next;
	}
}

/*
 * Check to see if a timer is present, if so, return the number (+1) of
 * cycles left for it to fire, and if not present, return zero
 */
avr_cycle_count_t
avr_cycle_timer_status(
		avr_t * avr,
		avr_cycle_timer_t timer,
		void * param)
{
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;

	// find its place in the list
	avr_cycle_timer_slot_p t = pool->timer;
	while (t) {
		if (t->timer == timer && t->param == param) {
			return 1 + (t->when - avr->cycle);
		}
		t = t->next;
	}
	return 0;
}

/*
 * run through all the timers, call the ones that needs it,
 * clear the ones that wants it, and calculate the next
 * potential cycle we could sleep for...
 */
avr_cycle_count_t
avr_cycle_timer_process(
		avr_t * avr)
{
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;

	if (!pool->timer)
		return (avr_cycle_count_t)1000;

	do {
		avr_cycle_timer_slot_p t = pool->timer;
		avr_cycle_count_t when = t->when;

		if (when > avr->cycle)
			return t->when - avr->cycle;

		// detach from active timers
		pool->timer = t->next;
		t->next = NULL;
		do {
			avr_cycle_count_t w = t->timer(avr, when, t->param);
			// make sure the return value is either zero, or greater
			// than the last one to prevent infinite loop here
			when = w > when ? w : 0;
		} while (when && when <= avr->cycle);
		
		if (when) // reschedule then
			avr_cycle_timer_insert(avr, when - avr->cycle, t->timer, t->param);
		
		// requeue this one into the free ones
		QUEUE(pool->timer_free, t);
	} while (pool->timer);

	return (avr_cycle_count_t)1000;
}
