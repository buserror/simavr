/*
	button.c

	This defines a sample for a very simple "peripheral"
	that can talk to an AVR core.
	It is in fact a bit more involved than strictly necessary,
	but is made to demonstrante a few useful features that are
	easy to use.

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
#include "sim_avr.h"
#include "button.h"

static avr_cycle_count_t
button_auto_release(
		avr_t * avr,
		avr_cycle_count_t when,
		void * param)
{
	button_t * b = (button_t *)param;
	avr_raise_irq(b->irq + IRQ_BUTTON_OUT, 1);
	printf("button_auto_release\n");
	return 0;
}

/*
 * button release. set the "pin" to one
 */
void
button_release(
		button_t * b)
{
	avr_cycle_timer_cancel(b->avr, button_auto_release, b);
	avr_raise_irq(b->irq + IRQ_BUTTON_OUT, 1); // release
}

/*
 * button press. set the "pin" to zero and optionally register a timer
 * that will reset it in a few usecs
 */
void
button_press(
		button_t * b,
		uint32_t duration_usec)
{
	avr_cycle_timer_cancel(b->avr, button_auto_release, b);
	avr_raise_irq(b->irq + IRQ_BUTTON_OUT, 0); // press
	if (duration_usec) {
		// register the auto-release
		avr_cycle_timer_register_usec(b->avr, duration_usec, button_auto_release, b);
	}
}

void
button_init(
		avr_t *avr,
		button_t * b,
		const char * name)
{
	b->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_BUTTON_COUNT, &name);
	b->avr = avr;
}
