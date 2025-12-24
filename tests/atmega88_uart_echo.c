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
#define F_CPU 8000000L
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega88");
// tell simavr to listen to commands written in this (unused) register
AVR_MCU_SIMAVR_COMMAND(&GPIOR0);

/*
 * This small section tells simavr to generate a VCD trace dump.
 * Opening it with gtkwave will show you the data being pumped out into
 * the data register UDR0, and the UDRE0 ("empty") bit being set, then cleared
 *
 * The output bytes are logged as the value of an IRQ, because the special
 * nature of UDRE0 (separate read and write buffers) means that logging
 * the register UDRE0 would return any data received by the UART.
 * The "console" output is also logged.
 */

AVR_MCU_VCD_REGISTER_BIT(UCSR0A, UDRE0);
AVR_MCU_VCD_IO_IRQ(uar0, 1 /* UART_IRQ_OUTPUT */, "UART_output");
AVR_MCU_VCD_REGISTER(GPIOR1);

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


	// this tells simavr to quit with error code 1
    GPIOR0 = SIMAVR_CMD_EXIT_CODE_1;
}
