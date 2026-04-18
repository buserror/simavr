/*
	attiny1634_vcd_test.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.
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
#include <util/delay.h>
#include <avr/sleep.h>

/*
 * This demonstrate how to use the avr_mcu_section.h file
 * The macro adds a section to the ELF file with useful
 * information for the simulator
 */
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "attiny1634");
AVR_MCU_VOLTAGES(3300, 3300, 3300);	// 3.3V VCC, AVCC, VREF

static int uart_putchar(char c, FILE *stream) {
	if (c == '\n')
		uart_putchar('\r', stream);
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);

void adc_init()
{
	// set ADC
	ADMUX = (1 << REFS0);	// use internal AVCC (3.3)
	// enable ADC, start a conversion, with the interrupt
	// and with a /32 convertion clock.
	ADCSRA = (1 << ADEN) | 0x5;
}

int main(void)
{
	uint32_t volts;

	stdout = &mystdout;
	UCSR0B = (1 << TXEN0);
	adc_init();

	// Wait for PA2 to go high.

	while (!(PINA & 4)) ;

	// Do an ADC conversion.

	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC)) ;
	volts = (( ADCL | (ADCH << 8)) * 3300L) >> 10; // div 1024
	printf("Read ADC: %lu mvolts\n", volts);

	// Wait for PA2 to go low.

	while (PINA & 4) ;

	// Do an ADC conversion.

	ADCSRA |= (1 << ADSC);
	while (ADCSRA & (1 << ADSC)) ;
	volts = (( ADCL | (ADCH << 8)) * 3300L) >> 10; // div 1024
	printf("Read ADC: %lu mvolts\n", volts);

	sleep_cpu();
}

