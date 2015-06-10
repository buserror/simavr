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

#define PIN(_n, _p, _b) \
	[ _n] = { .ardupin =  _n, .port = _p, .pin =  _b }

#define ADC(_n, _p, _b) \
	[ _n] = { .ardupin =  _n, .port = _p, .pin =  _b, .analog = 1, .adc = _b }

#define ADCL(_n, _p, _b, _a) \
	[ _n] = { .ardupin =  _n, .port = _p, .pin =  _b, .analog = 1, .adc = _a }

#define PORT(_B, _P) \
	PIN( _B + 0, _P, 0), \
	PIN( _B + 1, _P, 1), \
	PIN( _B + 2, _P, 2), \
	PIN( _B + 3, _P, 3), \
	PIN( _B + 4, _P, 4), \
	PIN( _B + 5, _P, 5), \
	PIN( _B + 6, _P, 6), \
	PIN( _B + 7, _P, 7)

const ardupin_t arduidiot_644[] = {
	PORT( 0, 'B'),
	PORT( 8, 'D'),
	PORT(16, 'C'),
	
	ADC(24, 'A', 7),
	ADC(25, 'A', 6),
	ADC(26, 'A', 5),
	ADC(27, 'A', 4),
	ADC(28, 'A', 3),
	ADC(29, 'A', 2),
	ADC(30, 'A', 1),
	ADC(31, 'A', 0),
};

const ardupin_t arduidiot_2560[] = {
	PIN( 0, 'E', 0),
	PIN( 1, 'E', 1),
	PIN( 2, 'E', 4),
	PIN( 3, 'E', 5),
	PIN( 4, 'G', 5),
	PIN( 5, 'E', 3),
	PIN( 6, 'H', 3),
	PIN( 7, 'H', 4),
	PIN( 8, 'H', 5),
	PIN( 9, 'H', 6),

	PIN(10, 'B', 4),
	PIN(11, 'B', 5),
	PIN(12, 'B', 6),
	PIN(13, 'B', 7),
	PIN(14, 'J', 1),
	PIN(15, 'J', 0),
	PIN(16, 'H', 1),
	PIN(17, 'H', 0),
	PIN(18, 'D', 3),
	PIN(19, 'D', 2),

	PIN(20, 'D', 1),
	PIN(21, 'D', 0),
	PORT(22, 'A'),	

	PIN(30, 'C', 7),
	PIN(31, 'C', 6),
	PIN(32, 'C', 5),
	PIN(33, 'C', 4),
	PIN(34, 'C', 3),
	PIN(35, 'C', 2),
	PIN(36, 'C', 1),
	PIN(37, 'C', 0),
	PIN(38, 'D', 7),
	PIN(39, 'G', 2),

	PIN(40, 'G', 1),
	PIN(41, 'G', 0),
	PIN(42, 'L', 7),
	PIN(43, 'L', 6),
	PIN(44, 'L', 5),
	PIN(45, 'L', 4),
	PIN(46, 'L', 3),
	PIN(47, 'L', 2),
	PIN(48, 'L', 1),
	PIN(49, 'L', 0),

	PIN(50, 'B', 3),
	PIN(51, 'B', 2),
	PIN(52, 'B', 1),
	PIN(53, 'B', 0),
	ADC(54, 'F', 0),
	ADC(55, 'F', 1),
	ADC(56, 'F', 2),
	ADC(57, 'F', 3),
	ADC(58, 'F', 4),
	ADC(59, 'F', 5),

	ADC(60, 'F', 6),
	ADC(61, 'F', 7),
	ADCL(62, 'K', 0, 8),
	ADCL(63, 'K', 1, 9),
	ADCL(64, 'K', 2, 10),
	ADCL(65, 'K', 3, 11),
	ADCL(66, 'K', 4, 12),
	ADCL(67, 'K', 5, 13),
	ADCL(68, 'K', 6, 14),
	ADCL(69, 'K', 7, 15),

	PIN(70, 'G', 4),
	PIN(71, 'G', 3),
	PIN(72, 'J', 2),
	PIN(73, 'J', 3),
	PIN(74, 'J', 7),
	PIN(75, 'J', 4),
	PIN(76, 'J', 5),
	PIN(77, 'J', 6),
	PIN(78, 'E', 2),
	PIN(79, 'E', 6),

	PIN(80, 'E', 7),
	PIN(81, 'D', 4),
	PIN(82, 'D', 5),
	PIN(83, 'D', 6),
	PIN(84, 'H', 2),
	PIN(85, 'H', 7),
};

struct avr_irq_t *
get_ardu_irq(
		struct avr_t * avr,
		uint8_t ardupin,
		const ardupin_t pins[])
{
	if (pins[ardupin].ardupin != ardupin) {
		printf("%s pin %d isn't correct in table\n", __func__, ardupin);
		return NULL;
	}
	struct avr_irq_t * irq = avr_io_getirq(avr,
			AVR_IOCTL_IOPORT_GETIRQ(pins[ardupin].port), pins[ardupin].pin);
	if (!irq) {
		printf("%s pin %d PORT%C%d not found\n", __func__, ardupin, 
			pins[ardupin].port, pins[ardupin].pin);
		return NULL;
	} else
		printf("%s pin %2d is PORT%C%d\n", __func__, ardupin, 
			pins[ardupin].port, pins[ardupin].pin);

	return irq;
}
