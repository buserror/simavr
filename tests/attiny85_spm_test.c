/*
	attiny85_spm_test.c

	Copyright 2022 Peter Smith

	Test that the tiny85 can write to a page in the program memory space, by writing some junk data and
	reading it back

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
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include <avr/sleep.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");

/* No UART in tiny85, so simply write to unimplemented register ISIDR. */
static int uart_putchar(char c, FILE *stream) {
  if (c == '\n')
	uart_putchar('\r', stream);
  USIDR = c;
  return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

int main(void)
{
	static const int page = 0x1000;
	static const uint16_t w = 0x1234;

	for (int i = 0; i < SPM_PAGESIZE; i+=2) {
		boot_page_fill(page + i, w);    
	}

	boot_page_erase(page);
	boot_spm_busy_wait(); 

	boot_page_write(page);
	boot_spm_busy_wait();

	stdout = &mystdout;
	printf("Wrote %d bytes to address %d\n", SPM_PAGESIZE, page);

	for (int i = 0; i < SPM_PAGESIZE; i+=2) {
		uint16_t read_address = page + i;
		uint16_t word = pgm_read_word_near(read_address);

		if (word != w) {
			printf("Address: %d, Unexpected value: %d\n", read_address, word);
			cli();
			sleep_cpu();
		}
	}

	printf("Check Pass");
	cli();
	sleep_cpu();
}
