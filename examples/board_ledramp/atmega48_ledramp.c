/*
	atmega48_ledramp.c
	
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
#include <avr/interrupt.h>
#include <avr/sleep.h>

// for linker, emulator, and programmer's sake
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega48");

#include <stdio.h>
/* ------------------------------------------------------------------------- */
static int uart_putchar(char c, FILE *stream) {
  if (c == '\n')
    uart_putchar('\r', stream);
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);

#define	TICK_HZ					64

volatile uint32_t tickCount;

ISR(TIMER2_COMPA_vect)		// handler for Output Compare 1 overflow interrupt
{
	sei();
	tickCount++;
}

void tick_init()
{
	/*
		Timer 2 as RTC
	 */
	// needs to do that before changing the timer registers
	// ASYNC timer using a 32k crystal
	ASSR |= (1 << AS2);
	TCCR2A = (1 << WGM21);
    // use CLK/8 prescale value, clear timer/counter on compareA match
    TCCR2B = (2 << CS20);
 /*   -- MathPad
		clock=32768
		prescaler=8
		hz=64
		(clock/prescaler/hz)-1:63 -- */
    OCR2A = 63;
    TIMSK2  |= (1 << OCIE2A);
}

volatile uint8_t pressed = 0;

ISR(PCINT1_vect)
{
	pressed = (PINC & (1 << PC0)) ? 0 : 1;
	// wouldn't do that on real hardware, but it's a demo...
	printf("PCINT1_vect %02x\n", PINC);
}

int main()
{	
	DDRB=0xff;	// all PORT B output
	DDRC = 0;	// make PORT C input
	// enable pin change interrupt for PORT C pin 0
	PCMSK1 |= (1 << PCINT8);	// C0
	PCICR |= (1 << PCIE1);

	stdout = &mystdout;
	
	tick_init();
	sei();

	uint8_t mask = 0;
	for (;;) {
		mask <<= 1;
		if (!mask)
			mask = 1;
		if (pressed)
			PORTB = 0xff;
		else
			PORTB = mask;
		sleep_mode();
	}	
}

