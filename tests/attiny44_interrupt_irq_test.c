/*
	attiny44_interrupt_irq_test.c

	Copyright 2022 Giles Atkinson

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
#include <util/delay.h>
#include <avr/cpufunc.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny44");
AVR_MCU_VOLTAGES(5000, 5000, 3000) // VCC, AVCC, VREF - millivolts.

static volatile int nest;

ISR(INT0_vect)
{
    GPIOR1 = 1; // Signal to test harness
    if (nest)
        sei();
}

ISR(PCINT1_vect)
{
    GPIOR1 = 2;
    if (nest)
        sei();
}

ISR(ADC_vect)
{
    GPIOR1 = 3;
    if (nest)
        sei();
}

void go(void)
{

    /* Turn on the ADC. */

    ADCSRA = _BV(ADEN) + _BV(ADSC) + _BV(ADIE); // Enable, start, clk scale = 2

    /* Wait for ADC. */

    while ((ADCSRA & (1 << ADIF)) == 0)
        ;

    sei();                          // Three interrupts.

    /* Wait for ADC interrupt. */

    while (ADCSRA & _BV(ADIF))
        ;
    cli();
}

int main(void)
{
    /* Cause "external" and pin-change interrupts. */

    GIMSK = _BV(INT0) + _BV(PCIE1); // Enable INTO and PORTB pin change.
    MCUCR = 1;                      // Interrupt on either edge of PB2.
    PCMSK1 = _BV(PCINT10);          // Pin change interrupt for PB2.
    PORTB = _BV(2);                 // Two interrupts if pull-ups configured.
    DDRB = _BV(2);                  // Make sure of it.

    go();

    /* Do it again with interrupt nesting. */

    nest = 1;
    PORTB = 0;                      // Two interrupts.
    go();

    /* Final check: writing to GPIOR1 should have had not effect as the
     * handler did not writr thr passed value.
     */

    if (GPIOR1)
        GPIOR1 = 0xff;

    /* Stop test by sleeping with interrupts off. */

    sleep_cpu();
}
