/*
	attiny24_ioport_test.c

	Copyright 2026 Giles Atkinson

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

#include <stdio.h>
#include <stdlib.h>
#include "tests.h"
#include "avr_ioport.h"

/* Writes to PORTA are reported here. */

static uint8_t  expected[] = { 0xff, 0xfe, 0xde, 0xda, 0 };
static uint8_t *ep = expected;

static void reg_write(struct avr_irq_t *irq, uint32_t value, void *param)
{
	//printf("PORTA: %#02x\n", value);
	if (*ep != value) {
		fail("PORTA value %#02x when %#02x expected at stage %d\n",
			 value, *ep, (int)(ep - expected));
	}
	++ep;
}

int main(int argc, char **argv) {
	avr_t             *avr;

	tests_init(argc, argv);
	avr = tests_init_avr("attiny24_ioport_test.axf");

        /* Request callback when a value is sampled for conversion. */

	avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('A'),
										  IOPORT_IRQ_REG_PORT),
							reg_write, NULL);

	/* Run program and check results. */

	tests_assert_register_receive_avr(avr, 100000, "",
									  (avr_io_addr_t)0x2f /* &USIDR */);
	tests_success();
	return 0;
}
