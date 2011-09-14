/*
	atmega88_uart_echo.c

	This test case enables uart RX interupts, does a "printf" and then receive characters
	via the interupt handler until it reaches a \r.

	This tests the uart reception fifo system. It relies on the uart "irq" input and output
	to be wired together (see simavr.c)
 */

#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>

/*
 * This demonstrate how to use the avr_mcu_section.h file
 * The macro adds a section to the ELF file with useful
 * information for the simulator
 */
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega88");
// tell simavr to listen to commands written in this (unused) register
AVR_MCU_SIMAVR_COMMAND(&GPIOR0);

/*
 * This small section tells simavr to generate a VCD trace dump with changes to these
 * registers.
 * Opening it with gtkwave will show you the data being pumped out into the data register
 * UDR0, and the UDRE0 bit being set, then cleared
 */
const struct avr_mmcu_vcd_trace_t _mytrace[]  _MMCU_ = {
	{ AVR_MCU_VCD_SYMBOL("UDR0"), .what = (void*)&UDR0, },
	{ AVR_MCU_VCD_SYMBOL("UDRE0"), .mask = (1 << UDRE0), .what = (void*)&UCSR0A, },
	{ AVR_MCU_VCD_SYMBOL("GPIOR1"), .what = (void*)&GPIOR1, },
};

static int uart_putchar(char c, FILE *stream)
{
	if (c == '\n')
		uart_putchar('\r', stream);
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);

volatile uint8_t bindex = 0;
uint8_t buffer[80];
volatile uint8_t done = 0;

ISR(USART_RX_vect)
{
	uint8_t b = UDR0;
	GPIOR1 = b; // for the trace file
	buffer[bindex++] = b;
	buffer[bindex] = 0;
	if (b == '\n')
		done++;
}

int main()
{
	// this tell simavr to put the UART in loopback mode
	GPIOR0 = SIMAVR_CMD_UART_LOOPBACK;

	stdout = &mystdout;

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

	// this tells simavr to start the trace
	GPIOR0 = SIMAVR_CMD_VCD_START_TRACE;
	sei();
	printf("Hey there, this should be received back\n");

	while (!done)
		sleep_cpu();

	cli();
	printf("Received: %s", buffer);

	// this quits the simulator, since interupts are off
	// this is a "feature" that allows running tests cases and exit
	sleep_cpu();
}
