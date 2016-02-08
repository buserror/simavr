/*
 atmega32_ds1338.c

 Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>

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

#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include <stdio.h>

#undef F_CPU
#define F_CPU 7380000

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega32");

#include "ds1338.h"
#include "i2cmaster.h"

volatile uint8_t update_flag;

/*
 * This demo consists of a DS1338 RTC connected via the TWI bus
 * of an atmega32. The square wave output of the DS1338 is
 * enabled, with the tick-rate set to 1HZ. This is then fed
 * to pin D3 on the atmega32 which is configured to generate
 * an interrupt on a rising edge. When the interrupt fires the
 * time is read from the DS1338.
 */

int
main()
{
	i2c_init();

	/*
	 * Demonstrate the virtual part functionality.
	 */
	ds1338_init();
	ds1338_time_t time = {
		.date = 31,
		.day = 6,
		.hours = 23,
		.minutes = 59,
		.month = 12,
		.seconds = 56,
		.year = 14,
	};
	ds1338_set_time(&time);

	// Setup pin change interrupt for the square wave output
	cli();
	DDRD &= ~(1 << PD3);
	// Fire INT1 on the rising edge
	MCUCR |= (1 << ISC11) | (1 << ISC10);
	GICR |= (1 << INT1);
	sei();

	while(time.seconds != 2)
	{
		if (update_flag) {
			ds1338_get_time(&time);
			update_flag = 0;
		}
	}

	cli();
	sleep_mode();

}

ISR (INT1_vect)
{
	update_flag = 1;
}
