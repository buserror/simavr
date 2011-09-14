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

/*
 * simavr ADC allows external code to feed real voltages to the
 * simulator, and the simulator uses it's 'real' reference voltage
 * to do the right thing and return the 'proper' 10 bits ADC value
 * to the AVR firmware.
 *
 * To send values to the ADC, register your code to wait for the
 * ADC_IRQ_OUT_TRIGGER irq, and at that point send any of the
 * ADC_IRQ_ADC* with Millivolts as value.
 *
 * External trigger is not done yet.
 */

enum {
	// input IRQ values. Values are /always/ volts * 1000 (millivolts)
	ADC_IRQ_ADC0 = 0, ADC_IRQ_ADC1, ADC_IRQ_ADC2, ADC_IRQ_ADC3,
	ADC_IRQ_ADC4, ADC_IRQ_ADC5, ADC_IRQ_ADC6, ADC_IRQ_ADC7,
	ADC_IRQ_TEMP,			// see the datasheet
	ADC_IRQ_IN_TRIGGER,
	ADC_IRQ_OUT_TRIGGER,	// sends a avr_adc_mux_t
	ADC_IRQ_COUNT
};

// Get the internal IRQ corresponding to the INT
#define AVR_IOCTL_ADC_GETIRQ AVR_IOCTL_DEF('a','d','c',' ')

/*
 * Definition of a ADC mux mode.
 */
enum {
	ADC_MUX_NONE = 0,		// Nothing. return 0
	ADC_MUX_NOISE,			// Nothing. return something random
	ADC_MUX_SINGLE,			// Normal ADC pin reading
	ADC_MUX_DIFF,			// differencial channels (src-diff)
	ADC_MUX_TEMP,			// internal temp sensor
	ADC_MUX_REF,			// reference voltage (in src * 100)
	ADC_MUX_VCC4,			// VCC/4
};
typedef struct avr_adc_mux_t {
	unsigned long kind : 3, gain : 8, diff : 8, src : 13;
} avr_adc_mux_t;

enum {
	ADC_VREF_AREF	= 0,	// default mode
	ADC_VREF_VCC,
	ADC_VREF_AVCC,
	ADC_VREF_V110	= 1100,
	ADC_VREF_V256	= 2560,
};

typedef struct avr_adc_t {
	avr_io_t		io;

	uint8_t 		r_admux;
	// if the last bit exists in the mux, we are an extended ADC
	avr_regbit_t	mux[5];
	avr_regbit_t	ref[3];		// reference voltages bits
	uint16_t		ref_values[7]; // ADC_VREF_*

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

	/*
	 * runtime bits
	 */
	avr_adc_mux_t	muxmode[32];// maximum 5 bits of mux modes
	uint16_t		adc_values[8];	// current values on the ADCs
	uint16_t		temp;		// temp sensor reading
	uint8_t			first;
	uint8_t			read_status;	// marked one when adcl is read
} avr_adc_t;

void avr_adc_init(avr_t * avr, avr_adc_t * port);


/*
 * Helper macros for the Cores definition of muxes
 */
#define AVR_ADC_SINGLE(_chan) { \
		.kind = ADC_MUX_SINGLE, \
		.src = (_chan), \
	}
#define AVR_ADC_DIFF(_a,_b,_g) { \
		.kind = ADC_MUX_DIFF, \
		.src = (_a), \
		.diff = (_b), \
		.gain = (_g), \
	}
#define AVR_ADC_REF(_t) { \
		.kind = ADC_MUX_REF, \
		.src = (_t), \
	}
#define AVR_ADC_TEMP() { \
		.kind = ADC_MUX_TEMP, \
	}

#define AVR_ADC_VCC4() { \
		.kind = ADC_MUX_VCC4, \
	}

#endif /* __AVR_ADC_H___ */
