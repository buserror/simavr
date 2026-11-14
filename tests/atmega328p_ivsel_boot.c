/*
	atmega328p_ivsel_boot.c

	The bootloader of test_atmega328p_ivsel, linked at 0x7800, the boot
	section these fuses give: it prints B, takes a Timer0 interrupt with
	IVSEL set (b), clears IVSEL and starts the application.

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
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "atmega328p");

// BOOTSZ = 01 (1024 words, 0x7800), BOOTRST programmed
FUSES = {
	.low = LFUSE_DEFAULT,
	.high = FUSE_SPIEN & FUSE_BOOTSZ1 & FUSE_BOOTRST,
	.extended = EFUSE_DEFAULT,
};

// GPIOR0 flags, shared with atmega328p_ivsel.c
#define BOOT_IRQ	(1 << 0)
#define BOOT_DONE	(1 << 1)

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
	put('b');
	GPIOR0 |= BOOT_IRQ;
}

int
main(void)
{
	put('B');
	MCUCR = (1 << IVCE);
	MCUCR = (1 << IVSEL);
	TIMSK0 = (1 << TOIE0);
	TCCR0B = (1 << CS00);
	sei();
	while (!(GPIOR0 & BOOT_IRQ))
		;
	cli();
	TCCR0B = 0;
	MCUCR = (1 << IVCE);
	MCUCR = 0;
	GPIOR0 |= BOOT_DONE;
	__asm__ volatile ("jmp 0");
}
