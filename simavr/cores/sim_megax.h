/*
	sim_megax.h

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

#ifndef __SIM_MEGAX_H__
#define __SIM_MEGAX_H__

#include "sim_core_declare.h"
#include "avr_eeprom.h"
#include "avr_flash.h"
#include "avr_watchdog.h"
#include "avr_extint.h"
#include "avr_ioport.h"
#include "avr_uart.h"
#include "avr_adc.h"
#include "avr_timer.h"
#include "avr_spi.h"
#include "avr_twi.h"
#include "avr_acomp.h"


void mx_init(struct avr_t * avr);
void mx_reset(struct avr_t * avr);

/*
 * This is a template for all of the 8/16/32 devices, hopefully
 */
struct mcu_t {
	avr_t          core;
	avr_eeprom_t 	eeprom;
	avr_flash_t 	selfprog;
	avr_watchdog_t	watchdog;
	avr_extint_t	extint;
	avr_ioport_t	portb, portc, portd;
	avr_uart_t		uart;
	avr_acomp_t		acomp;
	avr_adc_t		adc;
	avr_timer_t		timer0,timer1,timer2;
	avr_spi_t		spi;
	avr_twi_t		twi;
	// PORTA exists on m16 and 32, but not on 8. 
	// It is still necessary to declare this as otherwise
	// the core_megax shared constructor will be confused
	avr_ioport_t	porta;
};

#ifdef SIM_CORENAME

#ifndef SIM_VECTOR_SIZE
#error SIM_VECTOR_SIZE is not declared
#endif
#ifndef SIM_MMCU
#error SIM_MMCU is not declared
#endif

#ifndef EFUSE_DEFAULT
#define EFUSE_DEFAULT 0xff
#endif
#define EICRA MCUCR
#define EIMSK GICR
#define EIFR GIFR

const struct mcu_t SIM_CORENAME = {
	.core = {
		.mmcu = SIM_MMCU,
		DEFAULT_CORE(SIM_VECTOR_SIZE),

		.init = mx_init,
		.reset = mx_reset,
	},
	AVR_EEPROM_DECLARE_NOEEPM(EE_RDY_vect),
	AVR_SELFPROG_DECLARE(SPMCR, SPMEN, SPM_RDY_vect),
	AVR_WATCHDOG_DECLARE_128(WDTCR, _VECTOR(0)),
	.extint = {
		AVR_EXTINT_DECLARE(0, 'D', PD2),
		AVR_EXTINT_DECLARE(1, 'D', PD3),
#ifdef INT2
		AVR_ASYNC_EXTINT_DECLARE(2, 'B', PB2),
#endif
	},
#ifdef PORTA
	AVR_IOPORT_DECLARE(a, 'A', A),
#endif
	AVR_IOPORT_DECLARE(b, 'B', B),
	AVR_IOPORT_DECLARE(c, 'C', C),
	AVR_IOPORT_DECLARE(d, 'D', D),

	//no PRUSART, upe=PE, no reg/bit name index, 'C' in RX/TX vector names
	AVR_UART_DECLARE(0, 0, PE, , C),

	.acomp = {
		.mux_inputs = 8,
		.mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1),
				AVR_IO_REGBIT(ADMUX, MUX2) },
		.aden = AVR_IO_REGBIT(ADCSRA, ADEN),
		.acme = AVR_IO_REGBIT(SFIOR, ACME),

		.r_acsr = ACSR,
		.acis = { AVR_IO_REGBIT(ACSR, ACIS0), AVR_IO_REGBIT(ACSR, ACIS1) },
		.acic = AVR_IO_REGBIT(ACSR, ACIC),
		.aco = AVR_IO_REGBIT(ACSR, ACO),
		.acbg = AVR_IO_REGBIT(ACSR, ACBG),
		.disabled = AVR_IO_REGBIT(ACSR, ACD),

		.timer_name = '1',

		.ac = {
			.enable = AVR_IO_REGBIT(ACSR, ACIE),
			.raised = AVR_IO_REGBIT(ACSR, ACI),
			.vector = ANA_COMP_vect,
		}
	},

	.adc = {
		.r_admux = ADMUX,
		.mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1),
					AVR_IO_REGBIT(ADMUX, MUX2), AVR_IO_REGBIT(ADMUX, MUX3),},
		.ref = { AVR_IO_REGBIT(ADMUX, REFS0), AVR_IO_REGBIT(ADMUX, REFS1)},
		.ref_values = { [1] = ADC_VREF_AVCC, [3] = ADC_VREF_V256 },

		.adlar = AVR_IO_REGBIT(ADMUX, ADLAR),
		.r_adcsra = ADCSRA,
		.aden = AVR_IO_REGBIT(ADCSRA, ADEN),
		.adsc = AVR_IO_REGBIT(ADCSRA, ADSC),
		// no ADATE .adate = AVR_IO_REGBIT(ADCSRA, ADATE),
		.adps = { AVR_IO_REGBIT(ADCSRA, ADPS0), AVR_IO_REGBIT(ADCSRA, ADPS1), AVR_IO_REGBIT(ADCSRA, ADPS2),},

		.r_adch = ADCH,
		.r_adcl = ADCL,

		//.r_adcsrb = ADCSRB,
		// .adts = { AVR_IO_REGBIT(ADCSRB, ADTS0), AVR_IO_REGBIT(ADCSRB, ADTS1), AVR_IO_REGBIT(ADCSRB, ADTS2),},

		.muxmode = {
			[0] = AVR_ADC_SINGLE(0), [1] = AVR_ADC_SINGLE(1),
			[2] = AVR_ADC_SINGLE(2), [3] = AVR_ADC_SINGLE(3),
			[4] = AVR_ADC_SINGLE(4), [5] = AVR_ADC_SINGLE(5),
			[6] = AVR_ADC_SINGLE(6), [7] = AVR_ADC_SINGLE(7),

			[14] = AVR_ADC_REF(1300),	// 1.30V
			[15] = AVR_ADC_REF(0),		// GND
		},

		.adc = {
			.enable = AVR_IO_REGBIT(ADCSRA, ADIE),
			.raised = AVR_IO_REGBIT(ADCSRA, ADIF),
			.vector = ADC_vect,
		},
	},
	.timer0 = {
		.name = '0',
#ifdef OC0_PORT
		.wgm = { AVR_IO_REGBIT(TCCR0, WGM00), AVR_IO_REGBIT(TCCR0, WGM01)},
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL8(),
			// TODO: 1 PWM phase correct 8bit
			[2] = AVR_TIMER_WGM_CTC(),
			[3] = AVR_TIMER_WGM_FASTPWM8(),
		},
#else
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL8(),
			// CTC etc. are missing because atmega8 does not support them on timer0
		},
#endif
		.cs = { AVR_IO_REGBIT(TCCR0, CS00), AVR_IO_REGBIT(TCCR0, CS01), AVR_IO_REGBIT(TCCR0, CS02) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */, AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE  /* AVR_TIMER_EXTCLK_CHOOSE means External clock chosen*/},

		.ext_clock_pin = AVR_IO_REGBIT(EXT_CLOCK0_PORT, EXT_CLOCK0_PIN),

		.r_tcnt = TCNT0,

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE0),
			.raised = AVR_IO_REGBIT(TIFR, TOV0),
			.vector = TIMER0_OVF_vect,
		},
#ifdef OC0_PORT
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR0,
				.com = AVR_IO_REGBITS(TCCR0, COM00, 0x3),
				.com_pin = AVR_IO_REGBIT(OC0_PORT, OC0_PIN),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE0),
					.raised = AVR_IO_REGBIT(TIFR, OCF0),
					.vector = TIMER0_COMP_vect,
				},
			},
		},
#else
// Compare Output Mode is missing for timer0 on atmega8
#endif
	},
	.timer1 = {
		.name = '1',
		.wgm = { AVR_IO_REGBIT(TCCR1A, WGM10), AVR_IO_REGBIT(TCCR1A, WGM11),
					AVR_IO_REGBIT(TCCR1B, WGM12), AVR_IO_REGBIT(TCCR1B, WGM13) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL16(),
			// TODO: 1 PWM phase correct 8bit
			// 		 2 PWM phase correct 9bit
			//       3 PWM phase correct 10bit
			[4] = AVR_TIMER_WGM_CTC(),
			[5] = AVR_TIMER_WGM_FASTPWM8(),
			[6] = AVR_TIMER_WGM_FASTPWM9(),
			[7] = AVR_TIMER_WGM_FASTPWM10(),
			// TODO: 8, 9 PWM phase and freq correct ICR & 10, 11
			[12] = AVR_TIMER_WGM_ICCTC(),
			[14] = AVR_TIMER_WGM_ICPWM(),
			[15] = AVR_TIMER_WGM_OCPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR1B, CS10), AVR_IO_REGBIT(TCCR1B, CS11), AVR_IO_REGBIT(TCCR1B, CS12) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */, AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE  /* AVR_TIMER_EXTCLK_CHOOSE means External clock chosen*/},

		.ext_clock_pin = AVR_IO_REGBIT(EXT_CLOCK1_PORT, EXT_CLOCK1_PIN),

		.r_tcnt = TCNT1L,
		.r_icr = ICR1L,
		.r_icrh = ICR1H,
		.r_tcnth = TCNT1H,

		.ices = AVR_IO_REGBIT(TCCR1B, ICES1),
		.icp = AVR_IO_REGBIT(ICP_PORT, ICP_PIN),

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE1),
			.raised = AVR_IO_REGBIT(TIFR, TOV1),
			.vector = TIMER1_OVF_vect,
		},
		.icr = {
			.enable = AVR_IO_REGBIT(TIMSK, TICIE1),
			.raised = AVR_IO_REGBIT(TIFR, ICF1),
			.vector = TIMER1_CAPT_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR1AL,
				.r_ocrh = OCR1AH,	// 16 bits timers have two bytes of it
				.com = AVR_IO_REGBITS(TCCR1A, COM1A0, 0x3),
				.com_pin = AVR_IO_REGBIT(OC1A_PORT, OC1A_PIN),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE1A),
					.raised = AVR_IO_REGBIT(TIFR, OCF1A),
					.vector = TIMER1_COMPA_vect,
				},
			},
			[AVR_TIMER_COMPB] = {
				.r_ocr = OCR1BL,
				.r_ocrh = OCR1BH,
				.com = AVR_IO_REGBITS(TCCR1A, COM1B0, 0x3),
				.com_pin = AVR_IO_REGBIT(OC1B_PORT, OC1B_PIN),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE1B),
					.raised = AVR_IO_REGBIT(TIFR, OCF1B),
					.vector = TIMER1_COMPB_vect,
				},
			},
		},

	},
	.timer2 = {
		.name = '2',
		.wgm = { AVR_IO_REGBIT(TCCR2, WGM20), AVR_IO_REGBIT(TCCR2, WGM21) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL8(),
			// TODO 1 pwm phase correct 
			[2] = AVR_TIMER_WGM_CTC(),
			[3] = AVR_TIMER_WGM_FASTPWM8(),
		},
		.cs = { AVR_IO_REGBIT(TCCR2, CS20), AVR_IO_REGBIT(TCCR2, CS21), AVR_IO_REGBIT(TCCR2, CS22) },
		.cs_div = { 0, 0, 3 /* 8 */, 4 /* 32 */, 6 /* 64 */, 7 /* 128 */, 8 /* 256 */, 10 /* 1024 */ /* TODO external clock */ },

		.r_tcnt = TCNT2,

		// asynchronous timer source bit.. if set, use 32khz frequency
		.as2 = AVR_IO_REGBIT(ASSR, AS2),

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE2),
			.raised = AVR_IO_REGBIT(TIFR, TOV2),
			.vector = TIMER2_OVF_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR2,
				.com = AVR_IO_REGBITS(TCCR2, COM20, 0x3),
				.com_pin = AVR_IO_REGBIT(OC2_PORT, OC2_PIN), // same as timer1C
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE2),
					.raised = AVR_IO_REGBIT(TIFR, OCF2),
					.vector = TIMER2_COMP_vect,
				},
			},
		},
	},
	AVR_SPI_DECLARE(0, 0),
	.twi = {

		.r_twcr = TWCR,
		.r_twsr = TWSR,
		.r_twbr = TWBR,
		.r_twdr = TWDR,
		.r_twar = TWAR,
		// no .r_twamr = TWAMR,

		.twen = AVR_IO_REGBIT(TWCR, TWEN),
		.twea = AVR_IO_REGBIT(TWCR, TWEA),
		.twsta = AVR_IO_REGBIT(TWCR, TWSTA),
		.twsto = AVR_IO_REGBIT(TWCR, TWSTO),
		.twwc = AVR_IO_REGBIT(TWCR, TWWC),

		.twsr = AVR_IO_REGBITS(TWSR, TWS3, 0x1f),	// 5 bits
		.twps = AVR_IO_REGBITS(TWSR, TWPS0, 0x3),	// 2 bits

		.twi = {
			.enable = AVR_IO_REGBIT(TWCR, TWIE),
			.raised = AVR_IO_REGBIT(TWCR, TWINT),
			.raise_sticky = 1,
			.vector = TWI_vect,
		},
	},

};

#endif /* SIM_CORENAME */

#endif /* __SIM_MEGAX_H__ */
