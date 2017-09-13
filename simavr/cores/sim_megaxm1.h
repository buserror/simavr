/*
	sim_megaxm1.h

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.	If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __SIM_MEGAX8_H__
#define __SIM_MEGAX8_H__

#include "sim_core_declare.h"
#include "avr_eeprom.h"
#include "avr_flash.h"
#include "avr_watchdog.h"
#include "avr_extint.h"
#include "avr_ioport.h"
#include "avr_lin.h"
#include "avr_lin.h"
#include "avr_adc.h"
#include "avr_timer.h"
#include "avr_spi.h"

void mxm1_init(struct avr_t * avr);
void mxm1_reset(struct avr_t * avr);

/*
 * This is a template for all of the xm1 devices, hopefully
 */
struct mcu_t {
	avr_t core;
	avr_eeprom_t 	eeprom;
	avr_watchdog_t	watchdog;
	avr_flash_t 	selfprog;
	avr_extint_t	extint;
	avr_ioport_t	portb,portc,portd,porte;
	avr_lin_t		lin;
	avr_adc_t		adc;
	avr_timer_t		timer0,timer1;
	avr_spi_t		spi;
};

#ifdef SIM_CORENAME

#ifndef SIM_VECTOR_SIZE
#error SIM_VECTOR_SIZE is not declared
#endif
#ifndef SIM_MMCU
#error SIM_MMCU is not declared
#endif

const struct mcu_t SIM_CORENAME = {
	.core = {
		.mmcu = SIM_MMCU,
		DEFAULT_CORE(SIM_VECTOR_SIZE),

		.init = mxm1_init,
		.reset = mxm1_reset,
	},
	AVR_EEPROM_DECLARE_NOEEPM(EE_READY_vect),
	AVR_SELFPROG_DECLARE(SPMCSR, SPMEN, SPM_READY_vect),
	AVR_WATCHDOG_DECLARE(WDTCSR, WDT_vect),
	.extint = {
		AVR_EXTINT_DECLARE(0, 'D', 6),
		AVR_EXTINT_DECLARE(1, 'B', 2),
		AVR_EXTINT_DECLARE(2, 'B', 5),
		AVR_EXTINT_DECLARE(3, 'C', 0),
	},
	.portb = {
		.name = 'B', .r_port = PORTB, .r_ddr = DDRB, .r_pin = PINB,
		.pcint = {
			.enable = AVR_IO_REGBIT(PCICR, PCIE0),
			.raised = AVR_IO_REGBIT(PCIFR, PCIF0),
			.vector = PCINT0_vect,
		},
		.r_pcint = PCMSK0,
	},
	.portc = {
		.name = 'C', .r_port = PORTC, .r_ddr = DDRC, .r_pin = PINC,
		.pcint = {
			.enable = AVR_IO_REGBIT(PCICR, PCIE1),
			.raised = AVR_IO_REGBIT(PCIFR, PCIF1),
			.vector = PCINT1_vect,
		},
		.r_pcint = PCMSK1,
	},
	.portd = {
		.name = 'D', .r_port = PORTD, .r_ddr = DDRD, .r_pin = PIND,
		.pcint = {
			.enable = AVR_IO_REGBIT(PCICR, PCIE2),
			.raised = AVR_IO_REGBIT(PCIFR, PCIF2),
			.vector = PCINT2_vect,
		},
		.r_pcint = PCMSK2,
	},
	.porte = {
		.name = 'E', .r_port = PORTE, .r_ddr = DDRE, .r_pin = PINE,
		.pcint = {
			.enable = AVR_IO_REGBIT(PCICR, PCIE2),
			.raised = AVR_IO_REGBIT(PCIFR, PCIF2),
			.vector = PCINT2_vect,
		},
		.r_pcint = PCMSK3,
	},

	.lin = {
		.r_linbtr = LINBTR,
		.r_linbrrh = LINBRRH,
		.r_linbrrl = LINBRRL,

		.lena = AVR_IO_REGBIT( LINCR, LENA),
		.ldisr = AVR_IO_REGBIT( LINBTR, LDISR),
		.lbt = AVR_IO_REGBITS( LINBTR, LBT0, 0x3F), // 5 bits

		.uart = {
			.name = '0',
			.r_udr = LINDAT,

			.txen = AVR_IO_REGBIT(LINCR, LCMD0),
			.rxen = AVR_IO_REGBIT(LINCR, LCMD1),

			// note that control and BAUD calculation is handled via LIN regs above
			.r_ucsra = 0,
			.r_ucsrb = 0,
			.r_ucsrc = 0,
			.r_ubrrl = 0,
			.r_ubrrh = 0,

			.rxc = {
				.enable = AVR_IO_REGBIT(LINENIR, LENRXOK),
				.raised = AVR_IO_REGBIT(LINSIR, LRXOK),
				.vector = LIN_TC_vect,
			},
			.txc = {
				.enable = AVR_IO_REGBIT(LINENIR, LENTXOK),
				.raised = AVR_IO_REGBIT(LINSIR, LTXOK),
				.vector = LIN_TC_vect,
			},
			/* .udrc doesn't exist in the LIN UART */
		},
	},
	.adc = {
		.r_admux = ADMUX,
		.mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1),
					AVR_IO_REGBIT(ADMUX, MUX2), AVR_IO_REGBIT(ADMUX, MUX3),AVR_IO_REGBIT(ADMUX, MUX4)},
		.ref = { AVR_IO_REGBIT(ADMUX, REFS0), AVR_IO_REGBIT(ADMUX, REFS1)},
		.ref_values = { [1] = ADC_VREF_AVCC, [3] = ADC_VREF_V256, },

		.adlar = AVR_IO_REGBIT(ADMUX, ADLAR),
		.r_adcsra = ADCSRA,
		.aden = AVR_IO_REGBIT(ADCSRA, ADEN),
		.adsc = AVR_IO_REGBIT(ADCSRA, ADSC),
		.adate = AVR_IO_REGBIT(ADCSRA, ADATE),
		.adps = { AVR_IO_REGBIT(ADCSRA, ADPS0), AVR_IO_REGBIT(ADCSRA, ADPS1), AVR_IO_REGBIT(ADCSRA, ADPS2),},

		.r_adch = ADCH,
		.r_adcl = ADCL,

		.r_adcsrb = ADCSRB,
		.adts = { AVR_IO_REGBIT(ADCSRB, ADTS0), AVR_IO_REGBIT(ADCSRB, ADTS1), AVR_IO_REGBIT(ADCSRB, ADTS2), AVR_IO_REGBIT(ADCSRB, ADTS3),},
		.adts_op = {
			[0] = avr_adts_free_running,
			[1] = avr_adts_external_interrupt_0,
			[2] = avr_adts_timer_0_compare_match_a,
			[3] = avr_adts_timer_0_overflow,
			[4] = avr_adts_timer_1_compare_match_b,
			[5] = avr_adts_timer_1_overflow,
			[6] = avr_adts_timer_1_capture_event,
			[7] = avr_adts_psc_module_0_sync_signal,
			[8] = avr_adts_psc_module_1_sync_signal,
			[9] = avr_adts_psc_module_2_sync_signal,
			[10] = avr_adts_analog_comparator_0,
			[11] = avr_adts_analog_comparator_1,
			[12] = avr_adts_analog_comparator_2,
			[13] = avr_adts_analog_comparator_3,
		},

		.muxmode = {
			[0] = AVR_ADC_SINGLE(0), [1] = AVR_ADC_SINGLE(1),
			[2] = AVR_ADC_SINGLE(2), [3] = AVR_ADC_SINGLE(3),
			[4] = AVR_ADC_SINGLE(4), [5] = AVR_ADC_SINGLE(5),
			[6] = AVR_ADC_SINGLE(6), [7] = AVR_ADC_SINGLE(7),
			[8] = AVR_ADC_SINGLE(8), [9] = AVR_ADC_SINGLE(9),
			[10] = AVR_ADC_SINGLE(10), [11] = AVR_ADC_TEMP(),
			// AMP0/1/2 is missing, no clue what to do ...
			[17] = AVR_ADC_REF(2560),	// 1.1V
			[18] = AVR_ADC_REF(0),		// GND
		},
		.adc = {
			.enable = AVR_IO_REGBIT(ADCSRA, ADIE),
			.raised = AVR_IO_REGBIT(ADCSRA, ADIF),
			.vector = ADC_vect,
		},
	},
	.timer0 = {
		.name = '0',
		.disabled = AVR_IO_REGBIT(PRR,PRTIM0),
		.wgm = { AVR_IO_REGBIT(TCCR0A, WGM00), AVR_IO_REGBIT(TCCR0A, WGM01), AVR_IO_REGBIT(TCCR0B, WGM02) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL8(),
			[2] = AVR_TIMER_WGM_CTC(),
			[3] = AVR_TIMER_WGM_FASTPWM8(),
			[7] = AVR_TIMER_WGM_OCPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR0B, CS00), AVR_IO_REGBIT(TCCR0B, CS01), AVR_IO_REGBIT(TCCR0B, CS02) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */, AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE },
		.ext_clock_pin = AVR_IO_REGBIT(PORTC, 2), /* External clock pin */

		.r_tcnt = TCNT0,

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK0, TOIE0),
			.raised = AVR_IO_REGBIT(TIFR0, TOV0),
			.vector = TIMER0_OVF_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR0A,
				.com = AVR_IO_REGBITS(TCCR0A, COM0A0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTD, 3),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK0, OCIE0A),
					.raised = AVR_IO_REGBIT(TIFR0, OCF0A),
					.vector = TIMER0_COMPA_vect,
				},
			},
			[AVR_TIMER_COMPB] = {
				.r_ocr = OCR0B,
				.com = AVR_IO_REGBITS(TCCR0A, COM0B0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTE, 1),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK0, OCIE0B),
					.raised = AVR_IO_REGBIT(TIFR0, OCF0B),
					.vector = TIMER0_COMPB_vect,
				}
			}
		}
	},
	.timer1 = {
		.name = '1',
		.disabled = AVR_IO_REGBIT(PRR,PRTIM1),
		.wgm = { AVR_IO_REGBIT(TCCR1A, WGM10), AVR_IO_REGBIT(TCCR1A, WGM11),
					AVR_IO_REGBIT(TCCR1B, WGM12), AVR_IO_REGBIT(TCCR1B, WGM13) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL16(),
			[4] = AVR_TIMER_WGM_CTC(),
			[5] = AVR_TIMER_WGM_FASTPWM8(),
			[6] = AVR_TIMER_WGM_FASTPWM9(),
			[7] = AVR_TIMER_WGM_FASTPWM10(),
			[12] = AVR_TIMER_WGM_ICCTC(),
			[14] = AVR_TIMER_WGM_ICPWM(),
			[15] = AVR_TIMER_WGM_OCPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR1B, CS10), AVR_IO_REGBIT(TCCR1B, CS11), AVR_IO_REGBIT(TCCR1B, CS12) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */, AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE },
		.ext_clock_pin = AVR_IO_REGBIT(PORTC, 3), /* External clock pin */

		.r_tcnt = TCNT1L,
		.r_tcnth = TCNT1H,
		.r_icr = ICR1L,
		.r_icrh = ICR1H,

		.ices = AVR_IO_REGBIT(TCCR1B, ICES1),
		.icp = AVR_IO_REGBIT(PORTD, 4), // default port for ICP1 (A)

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK1, TOIE1),
			.raised = AVR_IO_REGBIT(TIFR1, TOV1),
			.vector = TIMER1_OVF_vect,
		},
		.icr = {
			.enable = AVR_IO_REGBIT(TIMSK1, ICIE1),
			.raised = AVR_IO_REGBIT(TIFR1, ICF1),
			.vector = TIMER1_CAPT_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR1AL,
				.r_ocrh = OCR1AH,	// 16 bits timers have two bytes of it
				.com = AVR_IO_REGBITS(TCCR1A, COM1A0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTD, 2),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK1, OCIE1A),
					.raised = AVR_IO_REGBIT(TIFR1, OCF1A),
					.vector = TIMER1_COMPA_vect,
				},
			},
			[AVR_TIMER_COMPB] = {
				.r_ocr = OCR1BL,
				.r_ocrh = OCR1BH,
				.com = AVR_IO_REGBITS(TCCR1A, COM1B0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTC, 1),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK1, OCIE1B),
					.raised = AVR_IO_REGBIT(TIFR1, OCF1B),
					.vector = TIMER1_COMPB_vect,
				},
			},
		},
	},
	AVR_SPI_DECLARE(PRR, PRSPI, 'C', 7, 0, 1, 1),
};
#endif /* SIM_CORENAME */

#endif /* __SIM_MEGAX8_H__ */
