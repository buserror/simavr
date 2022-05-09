/*
 * avrtest.c
 *
 * Created on: 4 Feb 2011
 * Author: sliedes
 * This is a very slightly modified version of atmega48_disabled_timer.c
 * by jone.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega48");

volatile uint8_t count;

ISR(TIMER0_COMPA_vect)
{
    ++count;
}

int main(void)
{
	// Set up timer0 - do not start yet
	TCCR0A |= (1 << WGM01);                     // Configure timer 0 for CTC mode
	TIMSK0 |= (1 << OCIE0A);                    // Enable CTC interrupt
	OCR0A   = 0xAA;                             // CTC compare value

	TCCR0B |= (1 << CS00) | (1 << CS01);        // Start timer: clk/64

	while ((TIFR0 & (1 << OCF0A)) == 0)
		;

	// Now interrupt is pending.  Try and clear it.

	TIFR0 = 0;
	if (TIFR0 & (1 << OCF0A))
		++count;			    // Should not clear
	TIFR0 = (1 << OCF0A);
	if ((TIFR0 & (1 << OCF0A)) == 0)
		++count;			    // Should clear!

	sei();                                      // Enable global interrupts

	// Let it run to next interrupt.

	sleep_mode();
	TIMSK0 = 0;		                    // Disable CTC interrupt

	if (count == 3)				    // Expected
		cli();

	// Time out if interrupting or count wrong.

	for (;;)
		sleep_mode();
}
