/*
 * avrtest.c
 *
 *  Created on: 1 Dec 2009
 *      Author: jone
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega48");

ISR(TIMER0_COMPA_vect)
{
	TCCR0B = 0;
	TCNT0 = 0;
}

int main(void)
{
	// Set up timer0 - do not start yet
	TCCR0A |= (1 << WGM01);                     // Configure timer 0 for CTC mode
	TIMSK0 |= (1 << OCIE0A);                    // Enable CTC interrupt
	OCR0A   = 0xAA;                             // CTC compare value

	sei();                                      // Enable global interrupts

	// here the interupts are enabled, but the interupt
	// vector should not be called
	while(1)
		;
}
