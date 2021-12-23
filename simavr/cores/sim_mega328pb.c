/*
	sim_mega328pb.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
	Copyright 2019 Bernhard Heinloth <bernhard@heinloth.net>

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

void m328pb_init(struct avr_t * avr);
void m328pb_reset(struct avr_t * avr);

#define _AVR_IO_H_
#define __ASSEMBLER__
#ifndef __AVR_ATmega328pb__
#define __AVR_ATmega328pb__
#endif
#include "avr/iom328pb.h"


const struct mcu_t {
	avr_t			core;
	avr_eeprom_t	eeprom;
	avr_flash_t		selfprog;
	avr_watchdog_t	watchdog;
	avr_extint_t	extint;
	avr_ioport_t	portb, portc, portd, porte;
	avr_uart_t		uart0,uart1;
	avr_acomp_t		acomp;
	avr_adc_t		adc;
	avr_timer_t		timer0,timer1,timer2,timer3,timer4;
	avr_spi_t		spi0,spi1;
	avr_twi_t		twi0,twi1;
 } mcu_mega328pb = {
	.core = {
		.mmcu = "atmega328pb",
		DEFAULT_CORE(4),

		.init = m328pb_init,
		.reset = m328pb_reset,
	},
	AVR_EEPROM_DECLARE(EE_READY_vect),
	AVR_SELFPROG_DECLARE(SPMCSR, 0, SPM_Ready_vect),
	AVR_WATCHDOG_DECLARE(WDTCSR, WDT_vect),
	.extint = {
		AVR_EXTINT_DECLARE(0, 'D', 2),
		AVR_EXTINT_DECLARE(1, 'D', 3),
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
			.enable = AVR_IO_REGBIT(PCICR, PCIE3),
			.raised = AVR_IO_REGBIT(PCIFR, PCIF3),
			.vector = PCINT3_vect,
		},
		.r_pcint = PCMSK3,
	},

	//PRR/PRUSART0, upe=UPE, reg/bit name index=0, no 'C' in RX/TX vector names
	AVR_UARTX_DECLARE(0, PRR0, PRUSART0),
	AVR_UARTX_DECLARE(1, PRR0, PRUSART1),

	.acomp = {
		.mux_inputs = 8,
		.mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1),
				AVR_IO_REGBIT(ADMUX, MUX2) },
		.pradc = AVR_IO_REGBIT(PRR0, PRADC),
		.aden = AVR_IO_REGBIT(ADCSRA, ADEN),
		.acme = AVR_IO_REGBIT(ADCSRB, ACME),

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
			.vector = ANALOG_COMP_vect,
		}
	},
	.adc = {
		.r_admux = ADMUX,
		.mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1),
					AVR_IO_REGBIT(ADMUX, MUX2), AVR_IO_REGBIT(ADMUX, MUX3),},
		.ref = { AVR_IO_REGBIT(ADMUX, REFS0), AVR_IO_REGBIT(ADMUX, REFS1)},
		.ref_values = { [1] = ADC_VREF_AVCC, [3] = ADC_VREF_V110, },

		.adlar = AVR_IO_REGBIT(ADMUX, ADLAR),
		.r_adcsra = ADCSRA,
		.aden = AVR_IO_REGBIT(ADCSRA, ADEN),
		.adsc = AVR_IO_REGBIT(ADCSRA, ADSC),
		.adate = AVR_IO_REGBIT(ADCSRA, ADATE),
		.adps = { AVR_IO_REGBIT(ADCSRA, ADPS0), AVR_IO_REGBIT(ADCSRA, ADPS1), AVR_IO_REGBIT(ADCSRA, ADPS2),},

		.r_adch = ADCH,
		.r_adcl = ADCL,

		.r_adcsrb = ADCSRB,
		.adts = { AVR_IO_REGBIT(ADCSRB, ADTS0), AVR_IO_REGBIT(ADCSRB, ADTS1), AVR_IO_REGBIT(ADCSRB, ADTS2),},
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
			[8] = AVR_ADC_TEMP(),
			[14] = AVR_ADC_REF(1100),	// 1.1V
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
		.disabled = AVR_IO_REGBIT(PRR0,PRTIM0),
		.wgm = { AVR_IO_REGBIT(TCCR0A, WGM00), AVR_IO_REGBIT(TCCR0A, WGM01), AVR_IO_REGBIT(TCCR0B, WGM02) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL8(),
			[2] = AVR_TIMER_WGM_CTC(),
			[3] = AVR_TIMER_WGM_FASTPWM8(),
			[7] = AVR_TIMER_WGM_OCPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR0B, CS00), AVR_IO_REGBIT(TCCR0B, CS01), AVR_IO_REGBIT(TCCR0B, CS02) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */, AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE },
		.ext_clock_pin = AVR_IO_REGBIT(PORTD, 4), /* External clock pin */

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
				.com_pin = AVR_IO_REGBIT(PORTD, 6),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK0, OCIE0A),
					.raised = AVR_IO_REGBIT(TIFR0, OCF0A),
					.vector = TIMER0_COMPA_vect,
				},
			},
			[AVR_TIMER_COMPB] = {
				.r_ocr = OCR0B,
				.com = AVR_IO_REGBITS(TCCR0A, COM0B0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTD, 5),
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
		.disabled = AVR_IO_REGBIT(PRR0,PRTIM1),
		.wgm = { AVR_IO_REGBIT(TCCR1A, WGM10), AVR_IO_REGBIT(TCCR1A, WGM11),
					AVR_IO_REGBIT(TCCR1B, WGM12), AVR_IO_REGBIT(TCCR1B, WGM13) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL16(),
			[4] = AVR_TIMER_WGM_CTC(),
			[5] = AVR_TIMER_WGM_FASTPWM8(),
			[6] = AVR_TIMER_WGM_FASTPWM9(),
			[7] = AVR_TIMER_WGM_FASTPWM10(),
			[8] = AVR_TIMER_WGM_ICPWM(),
			[9] = AVR_TIMER_WGM_OCPWM(),
			[12] = AVR_TIMER_WGM_ICCTC(),
			[14] = AVR_TIMER_WGM_ICPWM(),
			[15] = AVR_TIMER_WGM_OCPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR1B, CS10), AVR_IO_REGBIT(TCCR1B, CS11), AVR_IO_REGBIT(TCCR1B, CS12) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */, AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE },
		.ext_clock_pin = AVR_IO_REGBIT(PORTD, 5), /* External clock pin */

		.r_tcnt = TCNT1L,
		.r_tcnth = TCNT1H,
		.r_icr = ICR1L,
		.r_icrh = ICR1H,

		.ices = AVR_IO_REGBIT(TCCR1B, ICES1),
		.icp = AVR_IO_REGBIT(PORTB, 0),

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
				.com_pin = AVR_IO_REGBIT(PORTB, 1),
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
				.com_pin = AVR_IO_REGBIT(PORTB, 2),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK1, OCIE1B),
					.raised = AVR_IO_REGBIT(TIFR1, OCF1B),
					.vector = TIMER1_COMPB_vect,
				},
			},
		},
	},
	.timer2 = {
		.name = '2',
		.disabled = AVR_IO_REGBIT(PRR0,PRTIM2),
		.wgm = { AVR_IO_REGBIT(TCCR2A, WGM20), AVR_IO_REGBIT(TCCR2A, WGM21), AVR_IO_REGBIT(TCCR2B, WGM22) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL8(),
			[2] = AVR_TIMER_WGM_CTC(),
			[3] = AVR_TIMER_WGM_FASTPWM8(),
			[7] = AVR_TIMER_WGM_OCPWM(),
		},

		.cs = { AVR_IO_REGBIT(TCCR2B, CS20), AVR_IO_REGBIT(TCCR2B, CS21), AVR_IO_REGBIT(TCCR2B, CS22) },
		.cs_div = { 0, 0, 3 /* 8 */, 5 /* 32 */, 6 /* 64 */, 7 /* 128 */, 8 /* 256 */, 10 /* 1024 */ },

		.r_tcnt = TCNT2,
		
		// asynchronous timer source bit.. if set, use 32khz frequency
		.as2 = AVR_IO_REGBIT(ASSR, AS2),
		
		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK2, TOIE2),
			.raised = AVR_IO_REGBIT(TIFR2, TOV2),
			.vector = TIMER2_OVF_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR2A,
				.com = AVR_IO_REGBITS(TCCR2A, COM2A0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTB, 3),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK2, OCIE2A),
					.raised = AVR_IO_REGBIT(TIFR2, OCF2A),
					.vector = TIMER2_COMPA_vect,
				}
			},
			[AVR_TIMER_COMPB] = {
				.r_ocr = OCR2B,
				.com = AVR_IO_REGBITS(TCCR2A, COM2B0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTD, 3),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK2, OCIE2B),
					.raised = AVR_IO_REGBIT(TIFR2, OCF2B),
					.vector = TIMER2_COMPB_vect,
				}
			}
		}
	},
	.timer3 = {
		.name = '3',
		.disabled = AVR_IO_REGBIT(PRR1,PRTIM3),
		.wgm = { AVR_IO_REGBIT(TCCR3A, WGM30), AVR_IO_REGBIT(TCCR3A, WGM31),
					AVR_IO_REGBIT(TCCR3B, WGM32), AVR_IO_REGBIT(TCCR3B, WGM33) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL16(),
			[4] = AVR_TIMER_WGM_CTC(),
			[5] = AVR_TIMER_WGM_FASTPWM8(),
			[6] = AVR_TIMER_WGM_FASTPWM9(),
			[7] = AVR_TIMER_WGM_FASTPWM10(),
			[8] = AVR_TIMER_WGM_ICPWM(),
			[9] = AVR_TIMER_WGM_OCPWM(),
			[12] = AVR_TIMER_WGM_ICCTC(),
			[14] = AVR_TIMER_WGM_ICPWM(),
			[15] = AVR_TIMER_WGM_OCPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR3B, CS30), AVR_IO_REGBIT(TCCR3B, CS31), AVR_IO_REGBIT(TCCR3B, CS32) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */, AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE },
		.ext_clock_pin = AVR_IO_REGBIT(PORTD, 5), /* External clock pin */

		.r_tcnt = TCNT3L,
		.r_tcnth = TCNT3H,
		.r_icr = ICR3L,
		.r_icrh = ICR3H,

		.ices = AVR_IO_REGBIT(TCCR3B, ICES3),
		.icp = AVR_IO_REGBIT(PORTE, 3),

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK3, TOIE3),
			.raised = AVR_IO_REGBIT(TIFR3, TOV3),
			.vector = TIMER3_OVF_vect,
		},
		.icr = {
			.enable = AVR_IO_REGBIT(TIMSK3, ICIE3),
			.raised = AVR_IO_REGBIT(TIFR3, ICF3),
			.vector = TIMER3_CAPT_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR3AL,
				.r_ocrh = OCR3AH,	// 16 bits timers have two bytes of it
				.com = AVR_IO_REGBITS(TCCR3A, COM3A0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTD, 0),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK3, OCIE3A),
					.raised = AVR_IO_REGBIT(TIFR3, OCF3A),
					.vector = TIMER3_COMPA_vect,
				},
			},
			[AVR_TIMER_COMPB] = {
				.r_ocr = OCR3BL,
				.r_ocrh = OCR3BH,
				.com = AVR_IO_REGBITS(TCCR3A, COM3B0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTD, 2),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK3, OCIE3B),
					.raised = AVR_IO_REGBIT(TIFR3, OCF3B),
					.vector = TIMER3_COMPB_vect,
				},
			},
		},
	},
	.timer4 = {
		.name = '4',
		.disabled = AVR_IO_REGBIT(PRR1,PRTIM4),
		.wgm = { AVR_IO_REGBIT(TCCR4A, WGM40), AVR_IO_REGBIT(TCCR4A, WGM41),
					AVR_IO_REGBIT(TCCR4B, WGM12), AVR_IO_REGBIT(TCCR4B, WGM43) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL16(),
			[4] = AVR_TIMER_WGM_CTC(),
			[5] = AVR_TIMER_WGM_FASTPWM8(),
			[6] = AVR_TIMER_WGM_FASTPWM9(),
			[7] = AVR_TIMER_WGM_FASTPWM10(),
			[8] = AVR_TIMER_WGM_ICPWM(),
			[9] = AVR_TIMER_WGM_OCPWM(),
			[12] = AVR_TIMER_WGM_ICCTC(),
			[14] = AVR_TIMER_WGM_ICPWM(),
			[15] = AVR_TIMER_WGM_OCPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR4B, CS40), AVR_IO_REGBIT(TCCR4B, CS41), AVR_IO_REGBIT(TCCR4B, CS42) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */, AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE },
		.ext_clock_pin = AVR_IO_REGBIT(PORTD, 5), /* External clock pin */

		.r_tcnt = TCNT4L,
		.r_tcnth = TCNT4H,
		.r_icr = ICR4L,
		.r_icrh = ICR4H,

		.ices = AVR_IO_REGBIT(TCCR4B, ICES4),
		.icp = AVR_IO_REGBIT(PORTB, 0),

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK4, TOIE4),
			.raised = AVR_IO_REGBIT(TIFR4, TOV4),
			.vector = TIMER4_OVF_vect,
		},
		.icr = {
			.enable = AVR_IO_REGBIT(TIMSK4, ICIE4),
			.raised = AVR_IO_REGBIT(TIFR4, ICF4),
			.vector = TIMER4_CAPT_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR4AL,
				.r_ocrh = OCR4AH,	// 16 bits timers have two bytes of it
				.com = AVR_IO_REGBITS(TCCR4A, COM4A0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTD, 1),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK4, OCIE4A),
					.raised = AVR_IO_REGBIT(TIFR4, OCF4A),
					.vector = TIMER4_COMPA_vect,
				},
			},
			[AVR_TIMER_COMPB] = {
				.r_ocr = OCR4BL,
				.r_ocrh = OCR4BH,
				.com = AVR_IO_REGBITS(TCCR4A, COM4B0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTD, 2),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK4, OCIE4B),
					.raised = AVR_IO_REGBIT(TIFR4, OCF4B),
					.vector = TIMER4_COMPB_vect,
				},
			},
		},
	},
	.spi0 = {
		.name = '0',
		.disabled = AVR_IO_REGBIT(PRR0, PRSPI0),
		.r_spdr = SPDR0,
		.r_spcr = SPCR0,
		.r_spsr = SPSR0,

		.spe = AVR_IO_REGBIT(SPCR0, SPE),
		.mstr = AVR_IO_REGBIT(SPCR0, MSTR),

		.spr = { AVR_IO_REGBIT(SPCR0, SPR0), AVR_IO_REGBIT(SPCR0, SPR1), AVR_IO_REGBIT(SPSR0, SPI2X) },
		.spi = {
			.enable = AVR_IO_REGBIT(SPCR0, SPIE),
			.raised = AVR_IO_REGBIT(SPSR0, SPIF),
			.vector = SPI0_STC_vect,
		},
	},
	.spi1 = {
		.name = '1',
		.disabled = AVR_IO_REGBIT(PRR1, PRSPI1),
		.r_spdr = SPDR1,
		.r_spcr = SPCR1,
		.r_spsr = SPSR1,

		.spe = AVR_IO_REGBIT(SPCR1, SPE1),
		.mstr = AVR_IO_REGBIT(SPCR1, MSTR1),

		.spr = { AVR_IO_REGBIT(SPCR1, SPR10), AVR_IO_REGBIT(SPCR1, SPR11), AVR_IO_REGBIT(SPSR1, SPI2X1) },
		.spi = {
			.enable = AVR_IO_REGBIT(SPCR1, SPIE1),
			.raised = AVR_IO_REGBIT(SPSR1, SPIF1),
			.vector = SPI1_STC_vect,
		},
	},
	.twi0 = {
		.name = '0',
		.disabled = AVR_IO_REGBIT(PRR0,PRTWI0),

		.r_twcr = TWCR0,
		.r_twsr = TWSR0,
		.r_twbr = TWBR0,
		.r_twdr = TWDR0,
		.r_twar = TWAR0,
		.r_twamr = TWAMR0,

		.twen = AVR_IO_REGBIT(TWCR0, TWEN),
		.twea = AVR_IO_REGBIT(TWCR0, TWEA),
		.twsta = AVR_IO_REGBIT(TWCR0, TWSTA),
		.twsto = AVR_IO_REGBIT(TWCR0, TWSTO),
		.twwc = AVR_IO_REGBIT(TWCR0, TWWC),

		.twsr = AVR_IO_REGBITS(TWSR0, TWS3, 0x1f),	// 5 bits
		.twps = AVR_IO_REGBITS(TWSR0, TWPS0, 0x3),	// 2 bits

		.twi = {
			.enable = AVR_IO_REGBIT(TWCR0, TWIE),
			.raised = AVR_IO_REGBIT(TWCR0, TWINT),
			.raise_sticky = 1,
			.vector = TWI0_vect,
		},
	},
	.twi1 = {
		.name = '1',
		.disabled = AVR_IO_REGBIT(PRR1,PRTWI1),

		.r_twcr = TWCR1,
		.r_twsr = TWSR1,
		.r_twbr = TWBR1,
		.r_twdr = TWDR1,
		.r_twar = TWAR1,
		.r_twamr = TWAMR1,

		.twen = AVR_IO_REGBIT(TWCR1, TWEN1),
		.twea = AVR_IO_REGBIT(TWCR1, TWEA1),
		.twsta = AVR_IO_REGBIT(TWCR1, TWSTA1),
		.twsto = AVR_IO_REGBIT(TWCR1, TWSTO1),
		.twwc = AVR_IO_REGBIT(TWCR1, TWWC1),

		.twsr = AVR_IO_REGBITS(TWSR1, TWS13, 0x1f),	// 5 bits
		.twps = AVR_IO_REGBITS(TWSR1, TWPS10, 0x3),	// 2 bits

		.twi = {
			.enable = AVR_IO_REGBIT(TWCR1, TWIE1),
			.raised = AVR_IO_REGBIT(TWCR1, TWINT1),
			.raise_sticky = 1,
			.vector = TWI1_vect,
		},
	},
};

static avr_t * make()
{
	return avr_core_allocate(&mcu_mega328pb.core, sizeof(struct mcu_t));
}

avr_kind_t mega328pb = {
	.names = { "atmega328pb" },
	.make = make
};

void m328pb_init(struct avr_t * avr)
{
	struct mcu_t * mcu = (struct mcu_t*)avr;

	avr_eeprom_init(avr, &mcu->eeprom);
	avr_flash_init(avr, &mcu->selfprog);
	avr_watchdog_init(avr, &mcu->watchdog);
	avr_extint_init(avr, &mcu->extint);
	avr_ioport_init(avr, &mcu->portb);
	avr_ioport_init(avr, &mcu->portc);
	avr_ioport_init(avr, &mcu->portd);
	avr_ioport_init(avr, &mcu->porte);
	avr_uart_init(avr, &mcu->uart0);
	avr_uart_init(avr, &mcu->uart1);
	avr_acomp_init(avr, &mcu->acomp);
	avr_adc_init(avr, &mcu->adc);
	avr_timer_init(avr, &mcu->timer0);
	avr_timer_init(avr, &mcu->timer1);
	avr_timer_init(avr, &mcu->timer2);
	avr_timer_init(avr, &mcu->timer3);
	avr_timer_init(avr, &mcu->timer4);
	avr_spi_init(avr, &mcu->spi0);
	avr_spi_init(avr, &mcu->spi1);
	avr_twi_init(avr, &mcu->twi0);
	avr_twi_init(avr, &mcu->twi1);
}

void m328pb_reset(struct avr_t * avr)
{
//	struct mcu_t * mcu = (struct mcu_t*)avr;
}
