/*
 atmega32_rotenc_test.c

 A simple example demonstrating a Pansonic EVEP rotary encoder
 scrolling an LED up and down an 8 segment LED bar.

 Copyright 2018 Doug Szumski <d.s.szumski@gmail.com>

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
#include <stdio.h>

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega32");

typedef enum {
	CLOCKWISE,
	COUNTERCLOCKWISE
} led_scroll_dir_t;

/* Initialise the inputs for signal A and signal B from the rotary encoder.
 * They are hooked up to INTO on PD2 and GPIO pin PA0. Pull-ups are on the
 * PCB. */
void
rotary_spin_int_init(void)
{
	// Set INT0 and GPIO pin as inputs
	DDRD &= ~(1 << PD2) & ~(1 << PA0);
	// Any logical change on INT0 generates an interrupt (see DS p.67)
	MCUCR |= (1 << ISC00);
	MCUCR &= ~(1 << ISC01);
	// Enable interrupt pin PD2
	GICR |= (1 << INT0);
}

/* Initialise inputs for signal C from the rotary encoder (button). This
 * is hooked up to INT2 on PB2. A pull-up is on the PCB. */
void
rotary_button_int_init(void)
{
	DDRB &= ~(1 << PB2);
	// Falling edge trigger (pin is pulled up) (see DS p.67)
	MCUCSR &= ~(1 << ISC2);
	// Enable interrupt pin PB2
	GICR |= (1 << INT2);
}

/* Configure 8 segment virtual 'LED bar' on port C */
void
led_bar_init(void)
{
	DDRC = 0xFF; // All outputs
	PORTC = 0b00010000; // Start with a light on in the middle
}

void
led_bar_scroll(led_scroll_dir_t dir)
{
	switch (dir) {
		case CLOCKWISE:
			if (PORTC < 0b10000000) PORTC <<= 1;
			break;
		case COUNTERCLOCKWISE:
			if (PORTC > 0b00000001) PORTC >>= 1;
			break;
		default:
			break;
	}
}

int
main()
{
	// Disable interrupts whilst configuring them to avoid false triggers
	cli();
	rotary_spin_int_init();
	rotary_button_int_init();
	sei();

	led_bar_init();

	while (1) {
		// Wait for some input
	}

	cli();
	sleep_mode();
}

ISR(INT0_vect)
{
	// The Panasonic EVEP rotary encoder this is written for moves two
	// phases for every 'click' it emits. The interrupt is configured
	// to fire on every edge change of B, which is once per 'click'.
	// Moving forwards in phase, after B has changed state we poll A.
	// We get the sequence (A=0,B=1), (A=1,B=0). Moving backwards in
	// phase, after B has changed state we get (A=1,B=1), (A=0,B=0).
	//
	// +-------+---+---+
	// | Phase | A | B |
	// +-------+---+---+
	// |   0   | 0 | 0 |
	// |   1   | 0 | 1 |
	// |   2   | 1 | 1 |
	// |   3   | 1 | 0 |
	// +-------+---+---+
	//
	// The twist direction is then obtained by taking the logical
	// XOR of outputs A and B after the interrupt has fired. Of
	// course with a 'real life' part you might be better off
	// using a state machine, or some extra hardware to filter out
	// contact bounces which aren't modelled in the virtual part.
	uint8_t ccw_twist = ((PIND & (1 << PD2)) >> 2) ^ (PINA & (1 << PA0));

	// Scroll the LED bar
	if (ccw_twist) {
		led_bar_scroll(COUNTERCLOCKWISE);
	} else {
		led_bar_scroll(CLOCKWISE);
	}
}

ISR(INT2_vect)
{
	// Fires when rotary encoder button was pressed and resets
	// the LED bar to the starting position
	PORTC = 0b00010000;
}
