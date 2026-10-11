/*
	attiny85_timer1_ctc1.c

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
 * Timer 1 with CTC1: cleared on a match with OCR1C, so it counts 0..OCR1C
 * and never overflows.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");

/* No UART in the tiny85: the test reads what is written to USIDR. */
static void say(const char *s)
{
	while (*s)
		USIDR = *s++;
}

int main(void)
{
	uint8_t high = 0, last = 0, wraps = 0;

	OCR1C = 9;
	TCCR1 = (1 << CTC1) | (1 << CS10);
	/* sampled at whatever phase the loop gives: the 9 itself lasts a
	   single cycle, so it may not be seen */
	for (int i = 0; i < 200; i++) {
		uint8_t t = TCNT1;
		if (t > high)
			high = t;
		if (t < last)
			wraps++;
		last = t;
	}
	say(high <= 9 ? "within OCR1C, " : "past OCR1C, ");
	say(wraps ? "wraps, " : "never wraps, ");
	say(TIFR & (1 << TOV1) ? "TOV1 set" : "no TOV1");
	cli();
	sleep_cpu();
}
