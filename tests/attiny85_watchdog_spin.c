/*
	attiny85_watchdog_spin.c

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
 * The usual way to reset from firmware: interrupts off, the watchdog armed,
 * and wait -- in a loop, then (after the first reset) asleep. The phase
 * survives the resets in EEPROM.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
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
	uint8_t phase = eeprom_read_byte((uint8_t *)0);

	MCUSR = 0;
	wdt_disable();
	if (phase == 0xFF) {
		eeprom_write_byte((uint8_t *)0, 1);
		say("spin, ");
		cli();
		wdt_enable(WDTO_15MS);
		for (;;)
			;
	}
	if (phase == 1) {
		eeprom_write_byte((uint8_t *)0, 2);
		say("sleep, ");
		cli();
		wdt_enable(WDTO_15MS);
		sleep_cpu();
		for (;;)
			;
	}
	say("done");
	cli();
	sleep_cpu();
}
