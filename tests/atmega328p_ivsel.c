/*
	atmega328p_ivsel.c

	The application of test_atmega328p_ivsel: it prints A, starts the
	bootloader (atmega328p_ivsel_boot.c) unless the bootloader started
	it, then takes a Timer0 interrupt through its own vectors (a).

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
#include <avr/sleep.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "atmega328p");

// GPIOR0 flags, shared with atmega328p_ivsel_boot.c
#define BOOT_DONE	(1 << 1)
#define APP_IRQ		(1 << 2)

static void
put(char c)
{
	UCSR0B = (1 << TXEN0);
	while (!(UCSR0A & (1 << UDRE0)))
		;
	UDR0 = c;
}

ISR(TIMER0_OVF_vect)
{
	TIMSK0 = 0;
	put('a');
	GPIOR0 |= APP_IRQ;
}

int
main(void)
{
	put('A');
	if (!(GPIOR0 & BOOT_DONE))
		__asm__ volatile ("jmp 0x7800");
	TIMSK0 = (1 << TOIE0);
	TCCR0B = (1 << CS00);
	sei();
	while (!(GPIOR0 & APP_IRQ))
		;
	cli();
	put('\n');
	sleep_enable();
	sleep_cpu();
}
