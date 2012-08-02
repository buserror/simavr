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

#if 0
#define DEBUG(__w) __w
#define DUMP(_pool,_w) { \
	printf("%s:%d %s ",__func__,__LINE__, _w);\
	for (int _i=0;_i<_pool->count;_i++) \
		printf("[%2d:%7d] ",_i,(int)_pool->timer[_i].when);\
	printf("\n");\
}
#else
#define DEBUG(__w)
#define DUMP(_pool,_w)
#endif

void
avr_cycle_timer_reset(
		struct avr_t * avr)
{
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;
	memset(pool, 0, sizeof(*pool));
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

	// find its place in the list
	int inserti = 0;
	while (inserti < pool->count && pool->timer[inserti].when > when)
		inserti++;
	// make a hole
	int cnt = pool->count - inserti;
	if (cnt)
		memmove(&pool->timer[inserti + 1], &pool->timer[inserti],
				cnt * sizeof(avr_cycle_timer_slot_t));

	pool->timer[inserti].timer = timer;
	pool->timer[inserti].param = param;
	pool->timer[inserti].when = when;
	pool->count++;
	DEBUG(printf("%s %2d/%2d when %7d %p/%p\n", __func__, inserti, pool->count, (int)(when - avr->cycle), timer, param);)
	DUMP(pool, "after");
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

	if (pool->count == MAX_CYCLE_TIMERS) {
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

	for (int i = 0; i < pool->count; i++)
		if (pool->timer[i].timer == timer && pool->timer[i].param == param) {
			int cnt = pool->count - i - 1;
			DEBUG(printf("%s %2d when %7d %p/%p\n", __func__, i, (int)(pool->timer[i].when - avr->cycle), timer, param);)
			if (cnt)
				memmove(&pool->timer[i], &pool->timer[i+1],
						cnt * sizeof(avr_cycle_timer_slot_t));
			pool->count--;
			DUMP(pool, "after");
			return;
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

	for (int i = 0; i < pool->count; i++)
		if (pool->timer[i].timer == timer && pool->timer[i].param == param) {
			return 1 + (pool->timer[i].when - avr->cycle);
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

	if (!pool->count)
		return (avr_cycle_count_t)1000;

	do {
		// copy it, since the array is volatile
		avr_cycle_timer_slot_t  timer = pool->timer[pool->count-1];
		avr_cycle_count_t when = timer.when;
		if (when > avr->cycle)
			return when - avr->cycle;
		pool->count--; // remove the top element now
		do {
			DEBUG(printf("%s %2d when %7d %p/%p\n", __func__, pool->count, (int)(when), timer.timer, timer.param););
			when = timer.timer(avr, when, timer.param);
		} while (when && when <= avr->cycle);
		if (when) {
			DEBUG(printf("%s %2d reschedule when %7d %p/%p\n", __func__, pool->count, (int)(when), timer.timer, timer.param);)
			avr_cycle_timer_insert(avr, when - avr->cycle, timer.timer, timer.param);
		}
	} while (pool->count);

	return (avr_cycle_count_t)1000;
}
