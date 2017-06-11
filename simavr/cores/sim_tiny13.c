/*
	sim_tiny13.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
	                     Jon Escombe <lists@dresco.co.uk>

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

#include "sim_avr.h"
#include "sim_core_declare.h"
#include "avr_eeprom.h"
#include "avr_watchdog.h"
#include "avr_extint.h"
#include "avr_ioport.h"
#include "avr_timer.h"
#include "avr_adc.h"
#include "avr_acomp.h"

#define _AVR_IO_H_
#define __ASSEMBLER__
#include "avr/iotn13.h"

static void init(struct avr_t * avr);
static void reset(struct avr_t * avr);


static const struct mcu_t {
	avr_t core;
	avr_eeprom_t 	eeprom;
	avr_watchdog_t	watchdog;
	avr_extint_t	extint;
	avr_ioport_t	portb;
	avr_timer_t		timer0;
	avr_acomp_t		acomp;
	avr_adc_t       adc;
} mcu = {
	.core = {
		.mmcu = "attiny13",

		/*
		 * tiny13 has no extended fuse byte, so can not use DEFAULT_CORE macro
		 */
		.ramend = RAMEND,
		.flashend = FLASHEND,
		.e2end = E2END,
		.vector_size = 2,
// Disable signature when using an old avr toolchain
#ifdef SIGNATURE_0
		.signature = { SIGNATURE_0,SIGNATURE_1,SIGNATURE_2 },
		.fuse = { LFUSE_DEFAULT, HFUSE_DEFAULT },
#endif
		.init = init,
		.reset = reset,
	},
	AVR_EEPROM_DECLARE_8BIT(EE_RDY_vect),
	// tiny13 has different names for these...
	#define WDIF WDTIF
	#define WDIE WDTIE
	AVR_WATCHDOG_DECLARE(WDTCR, WDT_vect),
	.extint = {
		AVR_EXTINT_TINY_DECLARE(0, 'B', 1, GIFR),
	},
	.portb = {
		.name = 'B',  .r_port = PORTB, .r_ddr = DDRB, .r_pin = PINB,
		.pcint = {
			.enable = AVR_IO_REGBIT(GIMSK, PCIE),
			.raised = AVR_IO_REGBIT(GIFR, PCIF),
			.vector = PCINT0_vect,
		},
		.r_pcint = PCMSK,
	},
	.timer0 = {
		.name = '0',
		.wgm = { AVR_IO_REGBIT(TCCR0A, WGM00), AVR_IO_REGBIT(TCCR0A, WGM01), AVR_IO_REGBIT(TCCR0B, WGM02) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL8(),
			[2] = AVR_TIMER_WGM_CTC(),
			[3] = AVR_TIMER_WGM_FASTPWM8(),
			[7] = AVR_TIMER_WGM_OCPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR0B, CS00), AVR_IO_REGBIT(TCCR0B, CS01), AVR_IO_REGBIT(TCCR0B, CS02) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */ },

		.r_tcnt = TCNT0,

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK0, TOIE0),
			.raised = AVR_IO_REGBIT(TIFR0, TOV0),
			.vector = TIM0_OVF_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR0A,
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK0, OCIE0A),
					.raised = AVR_IO_REGBIT(TIFR0, OCF0A),
					.vector = TIM0_COMPA_vect,
				}
			},
			[AVR_TIMER_COMPB] = {
				.r_ocr = OCR0B,
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK0, OCIE0B),
					.raised = AVR_IO_REGBIT(TIFR0, OCF0B),
					.vector = TIM0_COMPB_vect,
				}
			}
		}
	},

	.acomp = {
		.mux_inputs = 4,
		.mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1) },
		.aden = AVR_IO_REGBIT(ADCSRA, ADEN),
		.acme = AVR_IO_REGBIT(ADCSRB, ACME),

		.r_acsr = ACSR,
		.acis = { AVR_IO_REGBIT(ACSR, ACIS0), AVR_IO_REGBIT(ACSR, ACIS1) },
		.aco = AVR_IO_REGBIT(ACSR, ACO),
		.acbg = AVR_IO_REGBIT(ACSR, ACBG),
		.disabled = AVR_IO_REGBIT(ACSR, ACD),

		.ac = {
			.enable = AVR_IO_REGBIT(ACSR, ACIE),
			.raised = AVR_IO_REGBIT(ACSR, ACI),
			.vector = ANA_COMP_vect,
		}
	},

	.adc = {
		.r_admux = ADMUX,
		.mux = { AVR_IO_REGBIT(ADMUX, MUX0),
			 AVR_IO_REGBIT(ADMUX, MUX1), },
		.ref = { AVR_IO_REGBIT(ADMUX, REFS0), },
		.ref_values = {
			[0] = ADC_VREF_VCC, [1] = ADC_VREF_V110,
		},

		.adlar = AVR_IO_REGBIT(ADMUX, ADLAR),
		.r_adcsra = ADCSRA,
		.aden = AVR_IO_REGBIT(ADCSRA, ADEN),
		.adsc = AVR_IO_REGBIT(ADCSRA, ADSC),
		.adate = AVR_IO_REGBIT(ADCSRA, ADATE),
		.adps = { AVR_IO_REGBIT(ADCSRA, ADPS0),
			  AVR_IO_REGBIT(ADCSRA, ADPS1),
			  AVR_IO_REGBIT(ADCSRA, ADPS2),},

		.r_adch = ADCH,
		.r_adcl = ADCL,

		.r_adcsrb = ADCSRB,
		.adts = { AVR_IO_REGBIT(ADCSRB, ADTS0),
			  AVR_IO_REGBIT(ADCSRB, ADTS1),
			  AVR_IO_REGBIT(ADCSRB, ADTS2),},
		.adts_op = {
			[0] = avr_adts_free_running,
			[1] = avr_adts_analog_comparator_0,
			[2] = avr_adts_external_interrupt_0,
			[3] = avr_adts_timer_0_compare_match_a,
			[4] = avr_adts_timer_0_overflow,
			[5] = avr_adts_timer_0_compare_match_b,
			[6] = avr_adts_pin_change_interrupt,
		},
		.muxmode = {
			[0] = AVR_ADC_SINGLE(0), [1] = AVR_ADC_SINGLE(1),
			[2] = AVR_ADC_SINGLE(2), [3] = AVR_ADC_SINGLE(3),
		},

		.adc = {
			.enable = AVR_IO_REGBIT(ADCSRA, ADIE),
			.raised = AVR_IO_REGBIT(ADCSRA, ADIF),
			.vector = ADC_vect,
		},
	},
};

static avr_t * make()
{
	return avr_core_allocate(&mcu.core, sizeof(struct mcu_t));
}

avr_kind_t tiny13 = {
	.names = { "attiny13", "attiny13a" },
	.make = make
};

static void init(struct avr_t * avr)
{
	struct mcu_t * mcu = (struct mcu_t*)avr;

	avr_eeprom_init(avr, &mcu->eeprom);
	avr_watchdog_init(avr, &mcu->watchdog);
	avr_extint_init(avr, &mcu->extint);
	avr_ioport_init(avr, &mcu->portb);
	avr_timer_init(avr, &mcu->timer0);
	avr_acomp_init(avr, &mcu->acomp);
	avr_adc_init(avr, &mcu->adc);
}

static void reset(struct avr_t * avr)
{
//	struct mcu_t * mcu = (struct mcu_t*)avr;
}
