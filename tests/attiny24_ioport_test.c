/*
	attiny24_ioport_test.c

	Copyright 2026 Giles Atkinson

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
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/cpufunc.h>
#include "avr_mcu_section.h"

/* This test is to verify that the SBI instruction correctly
 * changes single bits in the PORT and interrupt control registers.
 */

int main(void)
{
	volatile uint8_t a;

	DDRA = 0xff;	// Output
	PORTA = 0xff;   // Ones
	PINA |= 1;      // PORTA == 0xfe
	PINA |= 0x20;   // PORTA == 0xde

	// Clearing bits in PINA does nothing, but CBI should still work.

	PINA &= ~4;
	PORTA &= ~4;	// PORTA == 0xda;

	// But this is expected to clear PORTA!

	a = PINA;
	PINA = a | 2;

	// Repeat the exercise with the pin change interrupt flags,
	// reporting results in the low bits of PORTA.
	// This does not test SBI behaviour because GIFR is outside
	// the usable range, but still checks flage clearing.

	DDRB = 0xff;	// Output

	// Enable pin change interrupts on the high bits of both ports.

	GIMSK = _BV(PCIE1) | _BV(PCIE0);
	PCMSK0 = PCMSK1 = 0x80; // Pin PB7 does not actually exist!
	PORTA = 0x80;	// Raise interrupts.
	PORTB = 0x80;
	PORTA = GIFR;   // PORTA == 0x30
	GIFR = 0x10;
	PORTA = GIFR;   // PORTA == 0x20
	PORTA = 0x80;	// Re-raise interrupt.
	PORTA = GIFR;   // PORTA == 0x30
	GIFR = 0x20;
	PORTA = GIFR;   // PORTA == 0x10

    sleep_cpu();
}
