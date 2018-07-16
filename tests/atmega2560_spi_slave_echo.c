/*
	atmega88_uart_echo.c

	This test case enables uart RX interupts, does a "printf" and then receive characters
	via the interupt handler until it reaches a \r.

	This tests the uart reception fifo system. It relies on the uart "irq" input and output
	to be wired together (see simavr.c)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdio.h>

/*
 * This demonstrate how to use the avr_mcu_section.h file
 * The macro adds a section to the ELF file with useful
 * information for the simulator
 */
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega2560");
// tell simavr to listen to commands written in this (unused) register
AVR_MCU_SIMAVR_COMMAND(&GPIOR0);
AVR_MCU_SIMAVR_CONSOLE(&GPIOR1);

volatile uint8_t cnt = 0;
volatile uint8_t done = 0;
volatile uint8_t bindex = 0;
uint8_t buffer[80];

static int uart_putchar(char c, FILE *stream)
{
	uint8_t startcnt;
	if (c == '\n')
		uart_putchar('\r', stream);
	loop_until_bit_is_set(UCSR3A, UDRE3);

	startcnt = cnt;
	UDR3 = c;
//	_delay_us(100);

	// Wait until we have received the character back
	while(!done && cnt == startcnt)
	{
//		UDR1 = 'a';
//		UDR1 = '\n';
		sleep_cpu();
	}

//    UDR1 = 'b';
//    UDR1 = '\n';

	_delay_us(1000);

	return 0;
}

ISR (SPI_STC_vect)
{
    uint8_t c;

    c = SPDR;

    SPDR = c;
}

ISR(USART3_RX_vect)
{
	uint8_t b = UDR3;
	GPIOR1 = b; // for the trace file
	buffer[bindex++] = b;
	buffer[bindex] = 0;
	cnt++;

    UDR1 = b;
	loop_until_bit_is_set(UCSR1A, UDRE1);

	if (b == '\n')
		done++;
//	sleep_cpu();
}

#define PIN_MISO    3
#define PIN_SIM_SS  4

void
spi_init_slave (void)
{
    DDRB = (1 << PIN_MISO) | (1 << PIN_SIM_SS);

	SPCR |= (1 << SPE) | (1 << SPIE) | (1 << SPR1) | (1 << SPR0);

    PORTB |= (1 << PIN_MISO) | (1 << PIN_SIM_SS);

    SPDR = '0';
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);

int main()
{
	spi_init_slave ();

	stdout = &mystdout;

	UCSR3C = (3 << UCSZ30); // 8 bits
	// see http://www.nongnu.org/avr-libc/user-manual/group__util__setbaud.html
#define BAUD 38400
#include <util/setbaud.h>
	UBRR3H = UBRRH_VALUE;
	UBRR3L = UBRRL_VALUE;
#if USE_2X
	UCSR3A |= (1 << U2X3);
#else
	UCSR3A &= ~(1 << U2X3);
#endif

	// enable receiver & transmitter
	UCSR3B |= (1 << RXCIE3) | (1 << RXEN3) | (1 << TXEN3);

	// this tells simavr to start the trace
	GPIOR0 = SIMAVR_CMD_VCD_START_TRACE;
    PORTB &= (1 << PIN_SIM_SS);
	sei();

	printf("Hey there, this should be received back\n\n");
	loop_until_bit_is_set(UCSR3A, UDRE3);

	while (!done)
		sleep_cpu();

	cli();

    PORTB &= !(~1 << PIN_SIM_SS);

	// this quits the simulator, since interupts are off
	// this is a "feature" that allows running tests cases and exit
	sleep_cpu();
}
