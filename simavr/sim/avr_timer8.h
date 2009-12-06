/*
	avr_timer8.h

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

#ifndef AVR_TIMER8_H_
#define AVR_TIMER8_H_

#include "sim_avr.h"

typedef struct avr_timer8_t {
	avr_io_t	io;
	char name;
	avr_regbit_t	disabled;	// bit in the PRR

	avr_io_addr_t	r_ocra, r_ocrb, r_ocrc, r_tcnt;
	
	avr_regbit_t	wgm[4];
	avr_regbit_t	cs[4];
	uint8_t			cs_div[16];
	avr_regbit_t	as2;		// asynchronous clock 32khz

	avr_int_vector_t compa;	// comparator A
	avr_int_vector_t compb;	// comparator A
	avr_int_vector_t overflow;	// overflow

	uint32_t		compa_cycles;
	uint32_t		compb_cycles;
} avr_timer8_t;

void avr_timer8_init(avr_t * avr, avr_timer8_t * port);

#endif /* AVR_TIMER8_H_ */
