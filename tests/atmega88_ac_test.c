/*
	atmega88_ac_test.c

	Copyright 2017 Konstantin Begun

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

/*
 * This demonstrate how to use the avr_mcu_section.h file
 * The macro adds a section to the ELF file with useful
 * information for the simulator
 */
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega88");

static int uart_putchar(char c, FILE *stream) {
  if (c == '\n')
    uart_putchar('\r', stream);
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);

volatile uint8_t int_done;

ISR(ANALOG_COMP_vect)
{
	int_done = 1;
}

ISR(TIMER1_CAPT_vect)
{
	int_done = 2;
}

static inline void output_value() {
	putchar(ACSR & _BV(ACO) ? '1':'0');
}

static inline void output_test(uint8_t expected) {
	putchar(int_done == expected ? 'Y':'N');
}

int main(void)
{
	stdout = &mystdout;

	sei();

	ACSR = 0;

	printf("Check analog comparator with polling values\n");

	// check all the inputs
	for(uint8_t t = 0; t < 2; ++t) {		// run twice, first with AIN0, second with bandgap
		ADCSRB = 0;		// multiplexer off
		_NOP();	// for sync delay
		output_value();
		// now with multiplexer
		ADCSRB = _BV(ACME);
		_NOP();
		for(uint8_t i = 0; i < 8;++i)
		{
			// this is relying that all 3 mux bits are next to each other, which is the case for atmega88
			output_value();
			ADMUX += _BV(MUX0);
			_NOP();
		}
		// switch to bandgap
		ACSR |= _BV(ACBG);
		_NOP();
	}

	putchar('\n');

	// check interrupts
	printf("Check analog comparator interrupts\n");
	// in this test bandgap is expected to be above ADC0 and below ADC1

	ADCSRB = _BV(ACME);
	ADMUX = 0;	// ADC0
	ACSR = _BV(ACBG) | _BV(ACIE) | _BV(ACI);		// enable interrupts on output triggering

	// switch to ADC1
	int_done = 0;
	ADMUX = _BV(MUX0);
	_delay_us(500);
	output_test(1);

	// back to ADC0
	int_done = 0;
	ADMUX = 0;
	_delay_us(500);
	output_test(1);

	// change to trigger on falling edge
	ACSR = _BV(ACBG) | _BV(ACIE) | _BV(ACI) | _BV(ACIS1);

	// switch to ADC1 (falling edge)
	int_done = 0;
	ADMUX = _BV(MUX0);
	_delay_us(500);
	output_test(1);

	// switch to ADC0 (rising edge)
	int_done = 0;
	ADMUX = 0;
	_delay_us(500);
	output_test(0);		// expect no interrupt

	// change to trigger on rising edge
	ACSR = _BV(ACBG) | _BV(ACIE) | _BV(ACI) | _BV(ACIS1) | _BV(ACIS0);

	// switch to ADC1 (falling edge)
	int_done = 0;
	ADMUX = _BV(MUX0);
	_delay_us(500);
	output_test(0);	// expect no interrupt

	// switch to ADC0 (rising edge)
	int_done = 0;
	ADMUX = 0;
	_delay_us(500);
	output_test(1);

	putchar('\n');

	// now check timer1 input capture
	printf("Check analog comparator triggering timer capture\n");
	// setup AC for rising edge, disable comparator own interrupt and enable timer1 capture
	ACSR = _BV(ACBG) | _BV(ACIC) | _BV(ACIS1) | _BV(ACIS0);

	// now set up timer to capture on rising edge
	TCCR1A = 0;
	TCNT1 = 5555;
	TIMSK1 = _BV(ICIE1);		// enable capture interrupt
	TCCR1B = _BV(ICES1) | _BV(CS10);

	// switch to ADC1 (falling edge)
	int_done = 0;
	ADMUX = _BV(MUX0);
	_delay_us(500);
	output_test(0);	// expect no interrupt

	// switch to ADC0 (rising edge)
	int_done = 0;
	ADMUX = 0;
	_delay_us(500);
	output_test(2);

	cli();
	sleep_cpu();

}

