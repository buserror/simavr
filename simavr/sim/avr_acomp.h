/*
	avr_acomp.h

	Copyright 2017 Konstantin Begun

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

#ifndef __AVR_COMP_H___
#define __AVR_COMP_H___

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_avr.h"

/*
 * simavr Analog Comparator allows external code to feed real voltages to the
 * simulator, and the simulator uses it's 'real' reference voltage
 * to set comparator output accordingly and trigger in interrupt, if set up this way
 *
 */

enum {
	// input IRQ values. Values are /always/ volts * 1000 (millivolts)
	ACOMP_IRQ_AIN0 = 0, ACOMP_IRQ_AIN1,
	ACOMP_IRQ_ADC0, ACOMP_IRQ_ADC1, ACOMP_IRQ_ADC2, ACOMP_IRQ_ADC3,
	ACOMP_IRQ_ADC4, ACOMP_IRQ_ADC5, ACOMP_IRQ_ADC6, ACOMP_IRQ_ADC7,
	ACOMP_IRQ_ADC8, ACOMP_IRQ_ADC9, ACOMP_IRQ_ADC10, ACOMP_IRQ_ADC11,
	ACOMP_IRQ_ADC12, ACOMP_IRQ_ADC13, ACOMP_IRQ_ADC14, ACOMP_IRQ_ADC15,
	ACOMP_IRQ_OUT,		// output has changed
	ACOMP_IRQ_COUNT
};

// Get the internal IRQ corresponding to the INT
#define AVR_IOCTL_ACOMP_GETIRQ AVR_IOCTL_DEF('a','c','m','p')

enum {
	ACOMP_BANDGAP = 1100
};

typedef struct avr_acomp_t {
	avr_io_t		io;

	uint8_t			mux_inputs; // number of inputs (not mux bits!) in multiplexer. Other bits in mux below would be expected to be zero
	avr_regbit_t	mux[4];
	avr_regbit_t	pradc;		// ADC power reduction, this impacts on ability to use adc multiplexer
	avr_regbit_t 	aden;		// ADC Enabled, this impacts on ability to use adc multiplexer
	avr_regbit_t	acme;		// AC multiplexed input enabled

	avr_io_addr_t	r_acsr;		// control & status register
	avr_regbit_t	acis[2];	//
	avr_regbit_t	acic;		// input capture enable
	avr_regbit_t	aco;		// output
	avr_regbit_t	acbg;		// bandgap select
	avr_regbit_t	disabled;

	char			timer_name;	// connected timer for incput capture triggering

	// use ACI and ACIE bits
	avr_int_vector_t ac;

	// runtime data
	uint16_t		adc_values[16];	// current values on the ADCs inputs
	uint16_t		ain_values[2];  // current values on AIN inputs
	avr_irq_t*		timer_irq;
} avr_acomp_t;

void avr_acomp_init(avr_t * avr, avr_acomp_t * port);

#ifdef __cplusplus
};
#endif

#endif // __AVR_COMP_H___
