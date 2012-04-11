/*
	thermistor.h

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


#ifndef __THERMISTOR_H___
#define __THERMISTOR_H___

#include "sim_irq.h"

enum {
	IRQ_TERM_ADC_TRIGGER_IN = 0,
	IRQ_TERM_ADC_VALUE_OUT,
	IRQ_TERM_TEMP_VALUE_OUT,	// Celcius * 256
	IRQ_TERM_TEMP_VALUE_IN,		// Celcius * 256
	IRQ_TERM_COUNT
};

typedef struct thermistor_t {
	avr_irq_t *	irq;		// irq list
	struct avr_t *avr;		// keep it around so we can pause it
	uint8_t		adc_mux_number;

	short * 	table;
	int			table_entries;
	int 		oversampling;

	float	current;
} thermistor_t, *thermistor_p;

void
thermistor_init(
		struct avr_t * avr,
		thermistor_p t,
		int adc_mux_number,
		short * table,
		int	table_entries,
		int oversampling,
		float start_temp );

void
thermistor_set_temp(
		thermistor_p t,
		float temp );


#endif /* __THERMISTOR_H___ */
