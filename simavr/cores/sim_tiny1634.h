/*
	sim_tiny1634.h

	Copyright 2024 Ian Dobbie <ian.dobbie@gmail.com>

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

/*
 * ATtiny1634 specifications:
 * - 16KB Flash, 1KB SRAM, 256B EEPROM
 * - 3 GPIO ports (A: 8 pins, B: 4 pins, C: 6 pins)
 * - Timer0: 8-bit with PWM on OC0A (PC0), OC0B (PA5)
 * - Timer1: 16-bit with PWM on OC1A (PB3), OC1B (PA6)
 * - 12-channel 10-bit ADC
 * - WDT, PCINT on all 3 ports
 */

#ifndef __SIM_TINY1634_H__
#define __SIM_TINY1634_H__

#include "sim_core_declare.h"
#include "avr_eeprom.h"
#include "avr_flash.h"
#include "avr_watchdog.h"
#include "avr_extint.h"
#include "avr_ioport.h"
#include "avr_timer.h"
#include "avr_adc.h"

void tn1634_init(struct avr_t * avr);
void tn1634_reset(struct avr_t * avr);

/*
 * ATtiny1634 MCU structure
 * Note: No UART/USI in this implementation - add as needed
 */
struct mcu_t {
	avr_t core;
	avr_eeprom_t eeprom;
	avr_flash_t selfprog;
	avr_watchdog_t watchdog;
	avr_extint_t extint;
	avr_ioport_t porta, portb, portc;
	avr_timer_t timer0, timer1;
	avr_adc_t adc;
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

		.init = tn1634_init,
		.reset = tn1634_reset,
	},
	AVR_EEPROM_DECLARE(EE_RDY_vect),
	// ATtiny1634 has no WDCE bit - simavr still needs a valid register for wdce
	// Point wdce to WDTCSR bit 4 (unused) to satisfy simavr's init
	.watchdog = {
		.wdrf = AVR_IO_REGBIT(MCUSR, WDRF),
		.wdce = AVR_IO_REGBIT(WDTCSR, 4),  // Dummy - bit 4 is unused in ATtiny1634
		.wde = AVR_IO_REGBIT(WDTCSR, WDE),
		.wdp = { AVR_IO_REGBIT(WDTCSR, WDP0), AVR_IO_REGBIT(WDTCSR, WDP1),
				 AVR_IO_REGBIT(WDTCSR, WDP2), AVR_IO_REGBIT(WDTCSR, WDP3) },
		.watchdog = {
			.enable = AVR_IO_REGBIT(WDTCSR, WDIE),
			.raised = AVR_IO_REGBIT(WDTCSR, WDIF),
			.vector = WDT_vect,
		},
	},
	.selfprog = {
		.flags = 0,
		.r_spm = SPMCSR,
		.spm_pagesize = SPM_PAGESIZE,
		.selfprgen = AVR_IO_REGBIT(SPMCSR, SPMEN),
		.pgers = AVR_IO_REGBIT(SPMCSR, PGERS),
		.pgwrt = AVR_IO_REGBIT(SPMCSR, PGWRT),
		.blbset = AVR_IO_REGBIT(SPMCSR, RFLB),
	},
	.extint = {
		AVR_EXTINT_TINY_DECLARE(0, 'A', 2, GIFR),
	},
	.porta = {
		.name = 'A', .r_port = PORTA, .r_ddr = DDRA, .r_pin = PINA,
		.pcint = {
			.enable = AVR_IO_REGBIT(GIMSK, PCIE0),
			.raised = AVR_IO_REGBIT(GIFR, PCIF0),
			.vector = PCINT0_vect,
		},
		.r_pcint = PCMSK0,
	},
	.portb = {
		.name = 'B', .r_port = PORTB, .r_ddr = DDRB, .r_pin = PINB,
		.pcint = {
			.enable = AVR_IO_REGBIT(GIMSK, PCIE1),
			.raised = AVR_IO_REGBIT(GIFR, PCIF1),
			.vector = PCINT1_vect,
		},
		.r_pcint = PCMSK1,
	},
	.portc = {
		.name = 'C', .r_port = PORTC, .r_ddr = DDRC, .r_pin = PINC,
		.pcint = {
			.enable = AVR_IO_REGBIT(GIMSK, PCIE2),
			.raised = AVR_IO_REGBIT(GIFR, PCIF2),
			.vector = PCINT2_vect,
		},
		.r_pcint = PCMSK2,
	},

	// Timer0 - 8-bit with PWM on OC0A (PC0) and OC0B (PA5)
	.timer0 = {
		.name = '0',
		.disabled = AVR_IO_REGBIT(PRR, PRTIM0),
		.wgm = { AVR_IO_REGBIT(TCCR0A, WGM00), AVR_IO_REGBIT(TCCR0A, WGM01),
				 AVR_IO_REGBIT(TCCR0B, WGM02) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL8(),
			[1] = AVR_TIMER_WGM_FCPWM8(),
			[2] = AVR_TIMER_WGM_CTC(),
			[3] = AVR_TIMER_WGM_FASTPWM8(),
			[5] = AVR_TIMER_WGM_OCPWM(),
			[7] = { .kind = avr_timer_wgm_fast_pwm, .top = avr_timer_wgm_reg_ocra },
		},
		.cs = { AVR_IO_REGBIT(TCCR0B, CS00), AVR_IO_REGBIT(TCCR0B, CS01),
				AVR_IO_REGBIT(TCCR0B, CS02) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */,
					AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE },

		.r_tcnt = TCNT0,

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE0),
			.raised = AVR_IO_REGBIT(TIFR, TOV0),
			.vector = TIM0_OVF_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR0A,
				.com = AVR_IO_REGBITS(TCCR0A, COM0A0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTC, 0),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE0A),
					.raised = AVR_IO_REGBIT(TIFR, OCF0A),
					.vector = TIM0_COMPA_vect,
				},
			},
			[AVR_TIMER_COMPB] = {
				.r_ocr = OCR0B,
				.com = AVR_IO_REGBITS(TCCR0A, COM0B0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTA, 5),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE0B),
					.raised = AVR_IO_REGBIT(TIFR, OCF0B),
					.vector = TIM0_COMPB_vect,
				},
			},
		},
	},

	// Timer1 - 16-bit with PWM on OC1A (PB3) and OC1B (PA6)
	.timer1 = {
		.name = '1',
		.disabled = AVR_IO_REGBIT(PRR, PRTIM1),
		.wgm = { AVR_IO_REGBIT(TCCR1A, WGM10), AVR_IO_REGBIT(TCCR1A, WGM11),
				 AVR_IO_REGBIT(TCCR1B, WGM12), AVR_IO_REGBIT(TCCR1B, WGM13) },
		.wgm_op = {
			[0]  = AVR_TIMER_WGM_NORMAL16(),
			[1]  = AVR_TIMER_WGM_FCPWM8(),
			[2]  = AVR_TIMER_WGM_FCPWM9(),
			[3]  = AVR_TIMER_WGM_FCPWM10(),
			[4]  = AVR_TIMER_WGM_CTC(),
			[5]  = AVR_TIMER_WGM_FASTPWM8(),
			[6]  = AVR_TIMER_WGM_FASTPWM9(),
			[7]  = AVR_TIMER_WGM_FASTPWM10(),
			[8]  = AVR_TIMER_WGM_ICPWM(),
			[9]  = AVR_TIMER_WGM_OCPWM(),
			[10] = AVR_TIMER_WGM_ICPWM(),
			[11] = AVR_TIMER_WGM_OCPWM(),
			[12] = AVR_TIMER_WGM_ICCTC(),
			[14] = AVR_TIMER_WGM_ICFASTPWM(),
			[15] = { .kind = avr_timer_wgm_fast_pwm, .top = avr_timer_wgm_reg_ocra },
		},
		.cs = { AVR_IO_REGBIT(TCCR1B, CS10), AVR_IO_REGBIT(TCCR1B, CS11),
				AVR_IO_REGBIT(TCCR1B, CS12) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */,
					AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE },

		.r_tcnt = TCNT1L,
		.r_tcnth = TCNT1H,
		.r_icr = ICR1L,
		.r_icrh = ICR1H,

		.ices = AVR_IO_REGBIT(TCCR1B, ICES1),
		.icp = AVR_IO_REGBIT(PORTB, 1), /* ICP1 on PB1 */

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE1),
			.raised = AVR_IO_REGBIT(TIFR, TOV1),
			.vector = TIM1_OVF_vect,
		},
		.icr = {
			.enable = AVR_IO_REGBIT(TIMSK, ICIE1),
			.raised = AVR_IO_REGBIT(TIFR, ICF1),
			.vector = TIM1_CAPT_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR1AL,
				.r_ocrh = OCR1AH,
				.com = AVR_IO_REGBITS(TCCR1A, COM1A0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTB, 3),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE1A),
					.raised = AVR_IO_REGBIT(TIFR, OCF1A),
					.vector = TIM1_COMPA_vect,
				},
			},
			[AVR_TIMER_COMPB] = {
				.r_ocr = OCR1BL,
				.r_ocrh = OCR1BH,
				.com = AVR_IO_REGBITS(TCCR1A, COM1B0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTA, 6),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE1B),
					.raised = AVR_IO_REGBIT(TIFR, OCF1B),
					.vector = TIM1_COMPB_vect,
				},
			},
		},
	},

	// ADC - 12 channels, 10-bit
	.adc = {
		.r_admux = ADMUX,
		.mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1),
				 AVR_IO_REGBIT(ADMUX, MUX2), AVR_IO_REGBIT(ADMUX, MUX3) },
		.ref = { AVR_IO_REGBIT(ADMUX, REFS0), AVR_IO_REGBIT(ADMUX, REFS1) },
		.ref_values = {
			[0] = ADC_VREF_VCC,
			[1] = ADC_VREF_V110,
		},

		.adlar = AVR_IO_REGBIT(ADCSRB, ADLAR),
		.r_adcsra = ADCSRA,
		.aden = AVR_IO_REGBIT(ADCSRA, ADEN),
		.adsc = AVR_IO_REGBIT(ADCSRA, ADSC),
		.adate = AVR_IO_REGBIT(ADCSRA, ADATE),
		.adps = { AVR_IO_REGBIT(ADCSRA, ADPS0), AVR_IO_REGBIT(ADCSRA, ADPS1),
				  AVR_IO_REGBIT(ADCSRA, ADPS2) },

		.r_adch = ADCH,
		.r_adcl = ADCL,

		.r_adcsrb = ADCSRB,
		.adts = { AVR_IO_REGBIT(ADCSRB, ADTS0), AVR_IO_REGBIT(ADCSRB, ADTS1),
				  AVR_IO_REGBIT(ADCSRB, ADTS2) },
		.adts_op = {
			[0] = avr_adts_free_running,
			[1] = avr_adts_analog_comparator_0,
			[2] = avr_adts_external_interrupt_0,
			[3] = avr_adts_timer_0_compare_match_a,
			[4] = avr_adts_timer_0_overflow,
			[5] = avr_adts_timer_1_compare_match_b,
			[6] = avr_adts_timer_1_overflow,
			[7] = avr_adts_timer_1_capture_event,
		},

		.muxmode = {
			[0] = AVR_ADC_SINGLE(0), [1] = AVR_ADC_SINGLE(1),
			[2] = AVR_ADC_SINGLE(2), [3] = AVR_ADC_SINGLE(3),
			[4] = AVR_ADC_SINGLE(4), [5] = AVR_ADC_SINGLE(5),
			[6] = AVR_ADC_SINGLE(6), [7] = AVR_ADC_SINGLE(7),
			[8] = AVR_ADC_SINGLE(8), [9] = AVR_ADC_SINGLE(9),
			[10] = AVR_ADC_SINGLE(10), [11] = AVR_ADC_SINGLE(11),
			[14] = AVR_ADC_TEMP(),
			[15] = AVR_ADC_REF(0),  // 0V (GND)
		},

		.adc = {
			.enable = AVR_IO_REGBIT(ADCSRA, ADIE),
			.raised = AVR_IO_REGBIT(ADCSRA, ADIF),
			.vector = ADC_vect,
		},
	},
};

#endif /* SIM_CORENAME */

#endif /* __SIM_TINY1634_H__ */
