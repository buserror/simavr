/*
	attiny85_fuses.c

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

/*
 * The fuses and lock bits, as firmware reads them: the chip's defaults.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/boot.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");

/* No UART in the tiny85: the test reads what is written to USIDR. */
static void say(const char *s)
{
	while (*s)
		USIDR = *s++;
}

static void hex(uint8_t b)
{
	static const char digit[] = "0123456789ABCDEF";
	char s[] = { digit[b >> 4], digit[b & 15], ' ', 0 };
	say(s);
}

int main(void)
{
	hex(boot_lock_fuse_bits_get(GET_LOW_FUSE_BITS));
	hex(boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS));
	hex(boot_lock_fuse_bits_get(GET_EXTENDED_FUSE_BITS));
	hex(boot_lock_fuse_bits_get(GET_LOCK_BITS));
	cli();
	sleep_cpu();
}
