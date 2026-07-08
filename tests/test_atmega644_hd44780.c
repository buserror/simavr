/*
	test_atmega644_hd44780.c

	Regression test for the two hd44780 read-back defects (issue #582):
	the part clobbering its own RS/RW/E state via IRQ_HD44780_ALL during
	reads, and stale data bits caused by the FILTERED pin IRQ caches.

	The firmware initialises the LCD in 4-bit mode and busy-polls before
	every byte, wired bidirectionally exactly like
	examples/board_hd44780/charlcd.c. Afterwards the DDRAM content is
	compared against the strings the firmware wrote. Without the fixes
	the init sequence already derails (phantom writes flip the nibble
	phase, DDRAM stays empty or garbled); with only the first fix single
	bits are still dropped from characters written after busy polls.

	Copyright 2026 Felix Mertins

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

#include <string.h>

#include "avr_ioport.h"
#include "hd44780.h"
#include "tests.h"

static void
check_line(hd44780_t *lcd, int offset, const char *want)
{
	int len = (int)strlen(want);

	if (memcmp(lcd->vram + offset, want, (size_t)len) == 0)
		return;

	char got[32];
	for (int i = 0; i < len; i++) {
		uint8_t c = lcd->vram[offset + i];
		got[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
	}
	got[len] = 0;
	fail("DDRAM@0x%02x mismatch: got \"%s\", want \"%s\"",
	     offset, got, want);
}

int
main(int argc, char **argv)
{
	tests_init(argc, argv);

	avr_t *avr = tests_init_avr("atmega644_hd44780.axf");

	hd44780_t lcd;
	hd44780_init(avr, &lcd, 16, 2);

	/* wiring as in examples/board_hd44780/charlcd.c:
	 * data lines bidirectional, control lines AVR -> LCD */
	for (int i = 0; i < 4; i++) {
		avr_irq_t *iavr = avr_io_getirq(avr,
				AVR_IOCTL_IOPORT_GETIRQ('B'), i);
		avr_irq_t *ilcd = lcd.irq + IRQ_HD44780_D4 + i;
		avr_connect_irq(iavr, ilcd);
		avr_connect_irq(ilcd, iavr);
	}
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 4),
			lcd.irq + IRQ_HD44780_RS);
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 5),
			lcd.irq + IRQ_HD44780_E);
	avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 6),
			lcd.irq + IRQ_HD44780_RW);

	/* the firmware ends in sleep with interrupts off; the timeout is
	 * only a backstop in case the busy poll never returns */
	tests_run_test(avr, 1000000);

	check_line(&lcd, 0x00, "READBACK 4BIT");
	check_line(&lcd, 0x40, "SOFT RESET");

	tests_success();
	return 0;
}
