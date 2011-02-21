/*
	atmega48_charlcd.c

	Copyright Luki <humbell@ethz.ch>
	Copyright 2011 Michel Pollet <buserror@gmail.com>

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

#undef F_CPU
#define F_CPU 10000000

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/sleep.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//#include "avr_defines.h"

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega48");

static uint8_t subsecct = 0;
static uint8_t hour = 0;
static uint8_t minute = 0;
static uint8_t second = 0;
static volatile uint8_t update_needed = 0;

#include "avr_hd44780.c"

ISR( INT0_vect )
{
	/* External interrupt on pin D2 */
	subsecct++;
	if (subsecct == 50) {
		second++;
		subsecct = 0;
		update_needed = 1;
		if (second == 60) {
			minute++;
			second = 0;
			if (minute == 60) {
				minute = 0;
				hour++;
				if (hour == 24)
					hour = 0;
			}
		}
	}
}

int main()
{
	hd44780_init();
	/*
	 * Clear the display.
	 */
	hd44780_outcmd(HD44780_CLR);
	hd44780_wait_ready(1); // long wait

	/*
	 * Entry mode: auto-increment address counter, no display shift in
	 * effect.
	 */
	hd44780_outcmd(HD44780_ENTMODE(1, 0));
	hd44780_wait_ready(0);

	/*
	 * Enable display, activate non-blinking cursor.
	 */
	hd44780_outcmd(HD44780_DISPCTL(1, 1, 0));
	hd44780_wait_ready(0);

	EICRA = (1 << ISC00);
	EIMSK = (1 << INT0);

	sei();

	while (1) {
		while (!update_needed)
			sleep_mode();
		update_needed = 0;
		char buffer[16];

		hd44780_outcmd(HD44780_CLR);
		hd44780_wait_ready(1); // long wait
		hd44780_outcmd(HD44780_DDADDR(4));
		hd44780_wait_ready(0);
		sprintf(buffer, "%2d:%02d:%02d", hour, minute, second);

		char *s = buffer;
		while (*s) {
			hd44780_outdata(*s++);
			hd44780_wait_ready(0);
		}
	}

}
