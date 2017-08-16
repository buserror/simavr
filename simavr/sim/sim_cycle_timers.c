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

#define DEFAULT_SLEEP_CYCLES 1000

void
avr_cycle_timer_reset(
		struct avr_cycle_timer_pool_t * pool)
{
#if 0
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;
#endif

	avr_clock_t * clock = pool->clock;

	memset(pool, 0, sizeof(*pool));
	// queue all slots into the free queue
	for (int i = 0; i < MAX_CYCLE_TIMERS; i++) {
		avr_cycle_timer_slot_p t = &pool->timer_slots[i];
		QUEUE(pool->timer_free, t);
	}
#if 0
	avr->pool.run_cycle_count = 1;
	avr->pool.run_cycle_limit = 1;
#endif
	pool->clock = clock;
	pool->clock->run_cycle_count = 1;
	pool->clock->run_cycle_limit = 1;
}

static avr_cycle_count_t
avr_cycle_timer_return_sleep_run_cycles_limited(
	avr_cycle_timer_pool_t *pool,
	avr_cycle_count_t sleep_cycle_count)
{
	// run_cycle_count is bound to run_cycle_limit but NOT less than 1 cycle...
	//	this is not an error!..  unless you like deadlock.
#if 0
	avr_cycle_count_t run_cycle_count = ((avr->pool.run_cycle_limit >= sleep_cycle_count) ?
		sleep_cycle_count : avr->pool.run_cycle_limit);
#endif
	avr_cycle_count_t run_cycle_count = ((pool->clock->run_cycle_limit >= sleep_cycle_count) ?
		sleep_cycle_count : pool->clock->run_cycle_limit);
#if 0
	avr->pool.run_cycle_count = run_cycle_count ? run_cycle_count : 1;
#endif
	pool->clock->run_cycle_count = run_cycle_count ? run_cycle_count : 1;

	// sleep cycles are returned unbounded thus preserving original behavior.
	return(sleep_cycle_count);
}

static void
avr_cycle_timer_reset_sleep_run_cycles_limited(
	avr_cycle_timer_pool_t *pool)
{
#if 0
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;
#endif
	avr_cycle_count_t sleep_cycle_count = DEFAULT_SLEEP_CYCLES;

	if(pool->timer) {
#if 0
		if(pool->timer->when > avr->pool.cycle) {
#endif
		if(pool->timer->when > pool->clock->cycle) {
#if 0
			sleep_cycle_count = pool->timer->when - avr->pool.cycle;
#endif
			sleep_cycle_count = pool->timer->when - pool->clock->cycle;
		} else {
			sleep_cycle_count = 0;
		}
	}

	avr_cycle_timer_return_sleep_run_cycles_limited(pool, sleep_cycle_count);
}

// no sanity checks checking here, on purpose
#if 0
static void
#endif
static int
avr_cycle_timer_insert(
		avr_cycle_timer_pool_t * pool,
		avr_cycle_count_t when,
		avr_cycle_timer_t timer,
		void * param)
{
#if 0
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;
#endif

#if 0
	when += avr->pool.cycle;
#endif
	when += pool->clock->cycle;

	avr_cycle_timer_slot_p t = pool->timer_free;

#if 0
	if (!t) {
		AVR_LOG(avr, LOG_ERROR, "CYCLE: %s: ran out of timers (%d)!\n", __func__, MAX_CYCLE_TIMERS);
		return;
	}
#endif
	if (!t) {
		return -1;
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
	return 0;
}

#if 0
void
#endif
int
avr_cycle_timer_register(
		avr_cycle_timer_pool_t * pool,
		avr_cycle_count_t when,
		avr_cycle_timer_t timer,
		void * param)
{
#if 0
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;
#endif

	// remove it if it was already scheduled
#if 0
	avr_cycle_timer_cancel(avr, timer, param);
#endif
	avr_cycle_timer_cancel(pool, timer, param);

#if 0
	if (!pool->timer_free) {
		AVR_LOG(avr, LOG_ERROR, "CYCLE: %s: pool is full (%d)!\n", __func__, MAX_CYCLE_TIMERS);
		return;
	}
#endif
	if ( !pool->timer_free ) {
		return -1;
	}
#if 0
	avr_cycle_timer_insert(avr, when, timer, param);
	avr_cycle_timer_reset_sleep_run_cycles_limited(avr);
#endif
	if ( avr_cycle_timer_insert(pool, when, timer, param) < 0 ) {
		return -1;
	}
	avr_cycle_timer_reset_sleep_run_cycles_limited(pool);
	return 0;
}


#if 0
void
#endif
int
avr_cycle_timer_register_usec(
		avr_cycle_timer_pool_t * pool,
		uint32_t when,
		avr_cycle_timer_t timer,
		void * param)
{
#if 0
	avr_cycle_timer_register(avr, avr_usec_to_cycles(avr, when), timer, param);
#endif
	return avr_cycle_timer_register(pool, avr_usec_to_cycles(pool->clock, when), timer, param);
}

void
avr_cycle_timer_cancel(
		avr_cycle_timer_pool_t * pool,
		avr_cycle_timer_t timer,
		void * param)
{
#if 0
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;
#endif

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
#if 0
	avr_cycle_timer_reset_sleep_run_cycles_limited(avr);
#endif
	avr_cycle_timer_reset_sleep_run_cycles_limited(pool);
}

/*
 * Check to see if a timer is present, if so, return the number (+1) of
 * cycles left for it to fire, and if not present, return zero
 */
avr_cycle_count_t
avr_cycle_timer_status(
		avr_cycle_timer_pool_t * pool,
		avr_cycle_timer_t timer,
		void * param)
{
#if 0 
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;
#endif

	// find its place in the list
	avr_cycle_timer_slot_p t = pool->timer;
	while (t) {
		if (t->timer == timer && t->param == param) {
#if 0
			return 1 + (t->when - avr->pool.cycle);
#endif
			return 1 + (t->when - pool->clock->cycle);
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
		avr_cycle_timer_pool_t * pool)
{
#if 0
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;
#endif

	if (pool->timer) do {
		avr_cycle_timer_slot_p t = pool->timer;
		avr_cycle_count_t when = t->when;

#if 0
		if (when > avr->pool.cycle)
#endif
		if (when > pool->clock->cycle)
#if 0
			return avr_cycle_timer_return_sleep_run_cycles_limited(avr, when - avr->pool.cycle);
#endif
			return avr_cycle_timer_return_sleep_run_cycles_limited(pool, when - pool->clock->cycle);

		// detach from active timers
		pool->timer = t->next;
		t->next = NULL;
#if 0
		while ( when <= pool->clock->cycle ) {

			avr_cycle_count_t w = t->timer(avr,when,t->param);

			// make sure the return value is either zero, or greater
			// than the last one to prevent infinite loop here
			if ( w > when ) {
				when = w;
				continue;
			}
			goto DEQUE_TIMER;
		}
		avr_cycle_timer_insert(avr,when - pool->clock->cycle,t->timer,t->param);

DEQUE_TIMER:;

#else

		do {
#if 0
			avr_cycle_count_t w = t->timer(avr, when, t->param);
#endif
			avr_cycle_count_t w = t->timer(pool, when, t->param);
			// make sure the return value is either zero, or greater
			// than the last one to prevent infinite loop here
			when = w > when ? w : 0;
#if 0
		} while (when && when <= avr->pool.cycle);
#endif
		} while (when && when <= pool->clock->cycle);
		
		if (when) // reschedule then
#if 0
			avr_cycle_timer_insert(avr, when - avr->pool.cycle, t->timer, t->param);
#endif
			avr_cycle_timer_insert(pool, when - pool->clock->cycle, t->timer, t->param);
		
		// requeue this one into the free ones
#endif 
		QUEUE(pool->timer_free, t);
	} while (pool->timer);

	// original behavior was to return 1000 cycles when no timers were present...
	// run_cycles are bound to at least one cycle but no more than requested limit...
	//	value passed here is returned unbounded, thus preserving original behavior.
#if 0
	return avr_cycle_timer_return_sleep_run_cycles_limited(avr, DEFAULT_SLEEP_CYCLES);
#endif
	return avr_cycle_timer_return_sleep_run_cycles_limited(pool, DEFAULT_SLEEP_CYCLES);
}
