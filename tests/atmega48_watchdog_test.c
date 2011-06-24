/*
	attiny13_watchdog_test.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

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
#include <avr/wdt.h>

/*
 * This demonstrate how to use the avr_mcu_section.h file
 * The macro adds a section to the ELF file with useful
 * information for the simulator
 */
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega48");

static int uart_putchar(char c, FILE *stream) {
  if (c == '\n')
    uart_putchar('\r', stream);
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);


ISR(WDT_vect)
{
	// nothing to do here, we're just here to wake the CPU
}

int main()
{
	stdout = &mystdout;
	DDRD = (1<<PD1); // configure TxD as output
	
	wdt_enable(WDTO_120MS);

	// enable watchdog interupt
	// NOTE: since the Change Enable bit is no longer on, it should not
	// change the timer value that is already set!
	WDTCSR = (1 << WDIE);

	sei();

	printf("Watchdog is active\n");
	uint8_t count = 20;
	while (count--) {
		_delay_ms(10);
		wdt_reset();
	}
	printf("Waiting for Watchdog to kick\n");
	// now , stop calling the watchdog reset, and just sleep until it fires
	sleep_cpu();
	printf("Watchdog kicked us!\n");

	// when arriving here, the watchdog timer interupt was called and woke up
	// the core from sleep, so we can just quit properly
	cli();
	sleep_cpu();
}
