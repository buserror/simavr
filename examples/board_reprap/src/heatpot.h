/*
	heatpot.h

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


#ifndef __HEATPOT_H___
#define __HEATPOT_H___

#include "sim_irq.h"

enum {
	IRQ_HEATPOT_TALLY = 0,		// heatpot_data_t
	IRQ_HEATPOT_TEMP_OUT,		// Celcius * 256
	IRQ_HEATPOT_COUNT
};

typedef union {
	int32_t sid : 8, cost;
	uint32_t v;
} heatpot_data_t;

typedef struct heatpot_t {
	avr_irq_t *	irq;		// irq list
	struct avr_t * avr;
	char name[32];

	struct { int sid; float cost; } tally[32];

	float ambiant;
	float current;

	avr_cycle_count_t	cycle;
} heatpot_t, *heatpot_p;

void
heatpot_init(
		struct avr_t * avr,
		heatpot_p p,
		const char * name,
		float ambiant );

void
heatpot_tally(
		heatpot_p p,
		int sid,
		float cost );

#endif /* __HEATPOT_H___ */
