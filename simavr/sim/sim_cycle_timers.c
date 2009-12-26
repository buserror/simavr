/*
	sim_cycle_timers.c

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

#include <stdlib.h>
#include <stdio.h>
#include "sim_cycle_timers.h"

void avr_cycle_timer_register(avr_t * avr, avr_cycle_count_t when, avr_cycle_timer_t timer, void * param)
{
	avr_cycle_timer_cancel(avr, timer, param);

	if (avr->cycle_timer_map == 0xffffffff) {
		fprintf(stderr, "avr_cycle_timer_register is full!\n");
		return;
	}
	when += avr->cycle;
	for (int i = 0; i < 32; i++)
		if (!(avr->cycle_timer_map & (1 << i))) {
			avr->cycle_timer[i].timer = timer;
			avr->cycle_timer[i].param = param;
			avr->cycle_timer[i].when = when;
			avr->cycle_timer_map |= (1 << i);
			return;
		}
}

void avr_cycle_timer_register_usec(avr_t * avr, uint32_t when, avr_cycle_timer_t timer, void * param)
{
	avr_cycle_timer_register(avr, avr_usec_to_cycles(avr, when), timer, param);
}

void avr_cycle_timer_cancel(avr_t * avr, avr_cycle_timer_t timer, void * param)
{
	if (!avr->cycle_timer_map)
		return;
	for (int i = 0; i < 32; i++)
		if ((avr->cycle_timer_map & (1 << i)) &&
				avr->cycle_timer[i].timer == timer &&
				avr->cycle_timer[i].param == param) {
			avr->cycle_timer[i].timer = NULL;
			avr->cycle_timer[i].param = NULL;
			avr->cycle_timer[i].when = 0;
			avr->cycle_timer_map &= ~(1 << i);
			return;
		}
}

/*
 * run thru all the timers, call the ones that needs it,
 * clear the ones that wants it, and calculate the next
 * potential cycle we could sleep for...
 */
avr_cycle_count_t avr_cycle_timer_process(avr_t * avr)
{
	if (!avr->cycle_timer_map)
		return (avr_cycle_count_t)-1;

	avr_cycle_count_t min = (avr_cycle_count_t)-1;

	for (int i = 0; i < 32; i++) {
		if (!(avr->cycle_timer_map & (1 << i)))
			continue;
		// do it several times, in case we're late
		while (avr->cycle_timer[i].when && avr->cycle_timer[i].when <= avr->cycle) {
			// call it
			avr->cycle_timer[i].when =
					avr->cycle_timer[i].timer(avr,
							avr->cycle_timer[i].when,
							avr->cycle_timer[i].param);
			if (avr->cycle_timer[i].when == 0) {
				// clear it
				avr->cycle_timer[i].timer = NULL;
				avr->cycle_timer[i].param = NULL;
				avr->cycle_timer[i].when = 0;
				avr->cycle_timer_map &= ~(1 << i);
				break;
			}
		}
		if (avr->cycle_timer[i].when && avr->cycle_timer[i].when < min)
			min = avr->cycle_timer[i].when;
	}
	return min - avr->cycle;
}
