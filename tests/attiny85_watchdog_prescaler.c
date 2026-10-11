/*
	attiny85_watchdog_prescaler.c

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
 * A watchdog already running with a long timeout, then given a short one:
 * the reset must come after the short one.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
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
	if (MCUSR & (1 << WDRF)) {
		MCUSR = 0;
		wdt_disable();
		say("reset");
		cli();
		sleep_cpu();
	}
	wdt_enable(WDTO_8S);
	say("8 s, then 15 ms: ");
	wdt_enable(WDTO_15MS);
	sei();
	for (;;)
		;
}
