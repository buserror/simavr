/*
	hc595.h

	This defines a sample for a very simple "peripheral" 
	that can talk to an AVR core.
	It is in fact a bit more involved than strictly necessary,
	but is made to demonstrante a few useful features that are
	easy to use.
	
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

#ifndef __HC595_H__
#define __HC595_H__

#include "sim_irq.h"

/*
 * this one is quite fun, it simulated a 74HC595 shift register
 * driven by an SPI signal.
 * For the interest of the simulation, they can be chained, but 
 * for practicality sake the shift register is kept 32 bits
 * wide so it acts as 4 of them "daisy chained" already. 
 */
enum {
	IRQ_HC595_SPI_BYTE_IN = 0,	// if hooked to a byte based SPI IRQ
	IRQ_HC595_SPI_BYTE_OUT,		// to chain them !!
	IRQ_HC595_IN_LATCH,
	IRQ_HC595_IN_RESET,
	IRQ_HC595_OUT,				// when latched, output on this IRQ
	IRQ_HC595_COUNT
};

typedef struct hc595_t {
	avr_irq_t *	irq;		// irq list
	uint32_t	latch;		// value "on the pins"
	uint32_t 	value;		// value shifted in
} hc595_t;

void
hc595_init(
		struct avr_t * avr,
		hc595_t *p);

#endif
