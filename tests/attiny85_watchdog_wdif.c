/*
	attiny85_watchdog_wdif.c

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
 * The watchdog in interrupt mode, polled with interrupts off: WDIF is set
 * when it times out, and writing a 1 to it clears it until the next time.
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

#define WDIF_SET()	(WDTCR & (1 << WDIF))

int main(void)
{
	uint16_t n = 0;

	cli();
	WDTCR = (1 << WDIE);			/* 16 ms */
	while (!WDIF_SET())
		;
	say("set, ");
	WDTCR = (1 << WDIF) | (1 << WDIE);	/* a 1 clears it */
	say(WDIF_SET() ? "still set, " : "cleared, ");
	while (!WDIF_SET())
		n++;
	say(n > 1000 ? "set again later" : "set again at once");
	sleep_cpu();
}
