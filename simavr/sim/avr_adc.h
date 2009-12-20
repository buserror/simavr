/*
	avr_adc.h

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

#ifndef __AVR_ADC_H___
#define __AVR_ADC_H___

#include "sim_avr.h"

typedef struct avr_adc_t {
	avr_io_t		io;

	uint8_t 		r_admux;
	// if the last bit exists in the mux, we are an extended ADC
	avr_regbit_t	mux[5];
	avr_regbit_t	ref[3];		// reference voltage
	avr_regbit_t 	adlar;		// left/right adjustment bit

	uint8_t			r_adcsra;	// ADC Control and Status Register A
	avr_regbit_t 	aden;		// ADC Enabled
	avr_regbit_t 	adsc;		// ADC Start Conversion
	avr_regbit_t 	adate;		// ADC Auto Trigger Enable

	avr_regbit_t	adps[3];	// Prescaler bits. Note that it's a frequency bit shift

	uint8_t			r_adcl, r_adch;	// Data Registers

	uint8_t			r_adcsrb;	// ADC Control and Status Register B
	avr_regbit_t	adts[3];	// Timing Source
	avr_regbit_t 	bin;		// Bipolar Input Mode (tinyx5 have it)
	avr_regbit_t 	ipr;		// Input Polarity Reversal (tinyx5 have it)


	// use ADIF and ADIE bits
	avr_int_vector_t adc;
} avr_adc_t;

void avr_adc_init(avr_t * avr, avr_adc_t * port);

#endif /* __AVR_ADC_H___ */
