/*
	atmega88_ac_test.c

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
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <avr/cpufunc.h>

/*
 * This test verifies correctness of handling AVR internal pull-ups
 * The macro adds a section to the ELF file with useful
 * information for the simulator
 */
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega88");

void
init_uart(unsigned int ubrr)
{
	/*Set baud rate */
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;
	/*Enable receiver and transmitter */
	UCSR0B = (1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0);
	/* Set frame format: 8data, 2stop bit */
	UCSR0C = (1<<USBS0)|(1<<UCSZ00)|(1<<UCSZ01);
}

static int
uart_putchar(char c, FILE *stream)
{
	if (c == '\n')
	uart_putchar('\r', stream);
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
}

static int
uart_getchar(FILE *stream)
{
	/* Wait for data to be received */
	while ( !(UCSR0A & (1<<RXC0)) )
	;
	/* Get and return received data from buffer */
	char data = UDR0; //Temporarly store received data
	if(data == '\r')
	data = '\n';
	uart_putchar(data, stream); //Send to console what has been received, so we can see when typing
	return data;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, uart_getchar,
                                         _FDEV_SETUP_RW);

#define FOSC 8000000UL // Clock Speed
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD-1

static uint8_t
read_pin(int pin_c)
{
	uint8_t mask = (1 << (pin_c - '0'));
	return (PIND & mask)?1:0;
}

static void
handle_pin(int pin_c, uint8_t exp)
{
	uint8_t pin_state = read_pin(pin_c);
	uint8_t output_mask = (1 << (pin_c - '0' + 3));
	if (pin_state)
		PORTD |= output_mask;
	else
		PORTD &= ~output_mask;

	printf("PIND%c expected=%s, actual=%s\n", pin_c, (exp?"HIGH":"LOW"), (pin_state?"HIGH":"LOW"));
}

int main()
{
	int input_c = 0;
	DDRD = 0xE0;
	PORTD = 0x1C;

	stdout = &mystdout;
	init_uart(MYUBRR);

	sei();

	printf("Read internally pulled-up input pins on PIND2:4 and mirror its state to PORTD5:7 outputs\n");

	handle_pin('2', 1);
	handle_pin('3', 1);
	handle_pin('4', 1);

	input_c = getchar();
	while (input_c != 'q') {
		handle_pin(input_c, 0);
		handle_pin(input_c, 0);
		putchar('\n');
		input_c = getchar();
	}


	cli();
	sleep_cpu();

}
