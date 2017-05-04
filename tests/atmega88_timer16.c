/*
	atmega88_timer16.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

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

/*
 * This demonstrates how to use the avr_mcu_section.h file.
 * The macro adds a section to the ELF file with useful
 * information for the simulator.
 */
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega88");

/*
 * This small section tells simavr to generate a VCD trace dump with changes to these
 * registers.
 * Opening it with gtkwave will show you the data being read & written to these
 * It also demonstrate how you can use unused pins to generate your own traces, with
 * your own events to be displayed.
 *
 * Here the port B first 2 bits are used to display when a tick occurs, and when a
 * TCNT reset occurs.
 */
const struct avr_mmcu_vcd_trace_t _mytrace[]  _MMCU_ = {
	{ AVR_MCU_VCD_SYMBOL("TCNT1L"), .what = (void*)&TCNT1L, },
	{ AVR_MCU_VCD_SYMBOL("TCNT1H"), .what = (void*)&TCNT1H, },
};
AVR_MCU_VCD_PORT_PIN('B', 3, "OC2A");
AVR_MCU_VCD_PORT_PIN('B', 1, "reset_timer");
AVR_MCU_VCD_PORT_PIN('B', 0, "tick");

volatile uint16_t tcnt;

ISR(TIMER2_COMPA_vect)		// handler for Output Compare 2 overflow interrupt
{
	// this really doesn't no anything but proves a way to wake the main()
	// from sleep at regular intervals
	PORTB ^= 1;
}

int main()
{
	//
	// start the 16 bits timer, with default "normal" waveform
	// and no interupt enabled. This just increments TCNT1
	// at a regular rate
	//
	// timer prescaler to 64
	TCCR1B |= (0<<CS12 | 1<<CS11 | 1<<CS10);

	DDRB = 0x0B;

	//
	// now enable a tick counter
	// using an asynchronous mode
	//
	ASSR |= (1 << AS2);		// use "external" 32.7k crystal source
	// use CLK/8 prescale value, clear timer/counter on compareA match
	// toggle OC2A pin too
	TCCR2A = (1 << WGM21) | (1 << COM2A0);
	TCCR2B = (2 << CS20); // prescaler
	OCR2A = 63;	// 64 hz
	TIMSK2  |= (1 << OCIE2A);

	sei();

	int count = 0;
	while (count++ < 100) {
		// we read TCNT1, which should contain some sort of incrementing value
		tcnt = TCNT1;		// read it
		if (tcnt > 10000) {
			TCNT1 = 500;	// reset it arbitrarily
			PORTB ^= 2;		// mark it in the waveform file
		}
		sleep_cpu();    	// this will sleep until a new timer2 tick interrupt occurs
	}
	// sleeping with interrupt off is interpreted by simavr as "exit please"
	cli();
	sleep_cpu();
}
