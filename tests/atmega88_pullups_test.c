/*
	atmega88_pullups_test.c

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
// tell simavr to listen to commands written in this (unused) register
//AVR_MCU_SIMAVR_COMMAND(&GPIOR0);

void
init_uart()
{
	UCSR0C |= (3 << UCSZ00); // 8 bits
	// see http://www.nongnu.org/avr-libc/user-manual/group__util__setbaud.html
#define BAUD 38400
#include <util/setbaud.h>
	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;
#if USE_2X
	UCSR0A |= (1 << U2X0);
#else
	UCSR0A &= ~(1 << U2X0);
#endif

	// enable receiver & transmitter
	UCSR0B |= (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);
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

/*
static int
uart_getchar(FILE *stream)
{
	// Wait for data to be received
	while ( !(UCSR0A & (1<<RXC0)) )
	;
	// Get and return received data from buffer
	char data = UDR0; //Temporarly store received data
	if(data == '\r')
	data = '\n';
	uart_putchar(data, stream); //Send to console what has been received, so we can see when typing
	return data;
}
*/

//static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);
static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
										 _FDEV_SETUP_WRITE);


volatile uint8_t bindex = 0;
uint8_t buffer[80];
volatile uint8_t done = 0;

ISR(USART_RX_vect)
{
	uint8_t b = UDR0;
	buffer[bindex++] = b;
	buffer[bindex] = 0;
	if (b == '\n')
		done++;
}

static void
reset_buffer()
{
	cli();
	done = 0;
	bindex = 0;
	buffer[bindex] = 0;
	sei();
}

static void
press_button(int pin_c)
{
	reset_buffer();
	printf("~p%c\n", pin_c);
	while (!done) sleep_cpu();
	reset_buffer();
}

static void
release_button(int pin_c)
{
	reset_buffer();
	printf("~r%c\n", pin_c);
	while (!done) sleep_cpu();
	reset_buffer();
}

static uint8_t
read_pin(int pin_c)
{
	uint8_t mask = (1 << (pin_c - '0'));
	return (PIND & mask)?1:0;
}

static void
translate_pin_state(int pin_c, uint8_t exp)
{
	uint8_t pin_state = read_pin(pin_c);
	uint8_t output_mask = (1 << (pin_c - '0' + 3));
	uint8_t portd = PORTD;
	if (pin_state)
		portd |= output_mask;
	else
		portd &= ~output_mask;
	PORTD = portd;

	reset_buffer();
	printf("PIND%c expected=%s, actual=%s\n", pin_c, (exp?"HIGH":"LOW"), (pin_state?"HIGH":"LOW"));
	while (!done);// sleep_cpu();
	reset_buffer();
}

int main()
{
	// this tell simavr to put the UART in loopback mode
	//GPIOR0 = SIMAVR_CMD_UART_LOOPBACK;

	stdout = &mystdout;
	init_uart();

	DDRD = 0xE0;
	PORTD = 0x1C;

	sei();

	reset_buffer();
	printf("Read internally pulled-up input pins on PIND2:4 and mirror its state to PORTD5:7 outputs\n");

	while (!done);// sleep_cpu();
	reset_buffer();

	translate_pin_state('2', 1);
	translate_pin_state('3', 1);
	translate_pin_state('4', 1);

	press_button('2');
	translate_pin_state('2', 0);
	translate_pin_state('3', 1);
	translate_pin_state('4', 1);
	release_button('2');
	translate_pin_state('2', 1);
	translate_pin_state('3', 1);
	translate_pin_state('4', 1);

	press_button('3');
	translate_pin_state('2', 1);
	translate_pin_state('3', 0);
	translate_pin_state('4', 1);
	release_button('3');
	translate_pin_state('2', 1);
	translate_pin_state('3', 1);
	translate_pin_state('4', 1);

	press_button('4');
	translate_pin_state('2', 1);
	translate_pin_state('3', 1);
	translate_pin_state('4', 0);
	release_button('4');
	translate_pin_state('2', 1);
	translate_pin_state('3', 1);
	translate_pin_state('4', 1);

	cli();
	sleep_cpu();

}
