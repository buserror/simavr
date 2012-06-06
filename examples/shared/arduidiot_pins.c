/*
	arduidiot_pins.c

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

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
#include "arduidiot_pins.h"
#include "sim_irq.h"
#include "avr_ioport.h"

ardupin_t arduidiot_644[32] = {
	[ 0] = { .ardupin =  0, .port = 'B', .pin =  0 },
	[ 1] = { .ardupin =  1, .port = 'B', .pin =  1 },
	[ 2] = { .ardupin =  2, .port = 'B', .pin =  2 },
	[ 3] = { .ardupin =  3, .port = 'B', .pin =  3 },
	[ 4] = { .ardupin =  4, .port = 'B', .pin =  4 },
	[ 5] = { .ardupin =  5, .port = 'B', .pin =  5 },
	[ 6] = { .ardupin =  6, .port = 'B', .pin =  6 },
	[ 7] = { .ardupin =  7, .port = 'B', .pin =  7 },

	[ 8] = { .ardupin =  8, .port = 'D', .pin =  0 },
	[ 9] = { .ardupin =  9, .port = 'D', .pin =  1 },
	[10] = { .ardupin = 10, .port = 'D', .pin =  2 },
	[11] = { .ardupin = 11, .port = 'D', .pin =  3 },
	[12] = { .ardupin = 12, .port = 'D', .pin =  4 },
	[13] = { .ardupin = 13, .port = 'D', .pin =  5 },
	[14] = { .ardupin = 14, .port = 'D', .pin =  6 },
	[15] = { .ardupin = 15, .port = 'D', .pin =  7 },

	[16] = { .ardupin = 16, .port = 'C', .pin =  0 },
	[17] = { .ardupin = 17, .port = 'C', .pin =  1 },
	[18] = { .ardupin = 18, .port = 'C', .pin =  2 },
	[19] = { .ardupin = 19, .port = 'C', .pin =  3 },
	[20] = { .ardupin = 20, .port = 'C', .pin =  4 },
	[21] = { .ardupin = 21, .port = 'C', .pin =  5 },
	[22] = { .ardupin = 22, .port = 'C', .pin =  6 },
	[23] = { .ardupin = 23, .port = 'C', .pin =  7 },

	[24] = { .ardupin = 24, .port = 'A', .pin =  7, .analog = 1, .adc = 7 },
	[25] = { .ardupin = 25, .port = 'A', .pin =  6, .analog = 1, .adc = 6 },
	[26] = { .ardupin = 26, .port = 'A', .pin =  5, .analog = 1, .adc = 5 },
	[27] = { .ardupin = 27, .port = 'A', .pin =  4, .analog = 1, .adc = 4 },
	[28] = { .ardupin = 28, .port = 'A', .pin =  3, .analog = 1, .adc = 3 },
	[29] = { .ardupin = 29, .port = 'A', .pin =  2, .analog = 1, .adc = 2 },
	[30] = { .ardupin = 30, .port = 'A', .pin =  1, .analog = 1, .adc = 1 },
	[31] = { .ardupin = 31, .port = 'A', .pin =  0, .analog = 1, .adc = 0 },
};

struct avr_irq_t *
get_ardu_irq(
		struct avr_t * avr,
		int ardupin,
		ardupin_t pins[])
{
	if (pins[ardupin].ardupin != ardupin) {
		printf("%s pin %d isn't correct in table\n", __func__, ardupin);
		return NULL;
	}
	struct avr_irq_t * irq = avr_io_getirq(avr,
			AVR_IOCTL_IOPORT_GETIRQ(pins[ardupin].port), pins[ardupin].pin);
	if (!irq) {
		printf("%s pin %d PORT%C%d not found\n", __func__, ardupin, pins[ardupin].port, pins[ardupin].pin);
		return NULL;
	}
	return irq;
}
