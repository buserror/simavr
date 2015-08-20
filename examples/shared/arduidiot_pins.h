/*
	arduidiot_pins.h

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

/*
 * This file helps maps arduino pins to the matching AVR pin
 */
#ifndef __ARDUIDIOT_PINS_H__
#define __ARDUIDIOT_PINS_H__

#include <stdint.h>

#ifdef ARDUIDIO_FULL
typedef struct ardupin_t {
	uint32_t port : 4, pin : 3, analog : 1, adc : 4, pwm : 1, ardupin;
} ardupin_t, *ardupin_p;
#else
typedef struct ardupin_t {
	uint8_t port : 4, pin : 3;
} ardupin_t, *ardupin_p;
#endif

struct avr_t;

struct avr_irq_t *
get_ardu_irq(
		struct avr_t * avr,
		uint8_t ardupin,
		const ardupin_t pins[]);

extern const ardupin_t arduidiot_644[];
extern const ardupin_t arduidiot_2560[];

#endif
