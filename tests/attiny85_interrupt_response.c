/*
	attiny85_interrupt_response.c

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
 * A timer interrupt taken twice: once while the core runs, once while it
 * sleeps. test_attiny85_interrupt_response.c times the response.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");

static volatile uint8_t taken;

ISR(TIMER0_COMPA_vect)
{
	taken++;
}

int main(void)
{
	OCR0A = 99;
	TCCR0A = (1 << WGM01);		/* CTC */
	TIMSK = (1 << OCIE0A);
	TCCR0B = (1 << CS00);
	sei();

	while (taken < 1)
		;			/* running */

	set_sleep_mode(SLEEP_MODE_IDLE);
	sleep_enable();
	while (taken < 2)
		sleep_cpu();		/* asleep */

	cli();
	sleep_cpu();
}
