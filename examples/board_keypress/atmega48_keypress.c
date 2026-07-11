/*
	atmega48_keypress.c
	
	Copyright 2017 Al Popov

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
#include <avr/interrupt.h>
#include <avr/sleep.h>

// for linker, emulator, and programmer's sake
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega48");

int main()
{	
	// Setup output pins for LEDs
	DDRB = _BV(PINB5);
	DDRD = _BV(PIND7);
	// Turn on internal pull-ups on input pins
	PORTD = _BV(PIND0) | _BV(PIND1);

	sei();

	for (;;) {
		if (PIND & _BV(PIND0)) {
			PORTB |= _BV(PINB5);
		} else {
			PORTB &= ~_BV(PINB5);
		}
		if (PIND & _BV(PIND1)) {
			PORTD |= _BV(PIND7);
		} else {
			PORTD &= ~_BV(PIND7);
		}
		//sleep_mode();
	}	
}

