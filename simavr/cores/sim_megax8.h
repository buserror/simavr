/*
	sim_megax8.h

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


#ifndef __SIM_MEGAX8_H__
#define __SIM_MEGAX8_H__

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

void mx8_init(struct avr_t * avr);
void mx8_reset(struct avr_t * avr);

/*
 * This is a template for all of the x8 devices, hopefully
 */
struct mcu_t {
	avr_t core;
	avr_eeprom_t 	eeprom;
	avr_watchdog_t	watchdog;
	avr_flash_t 	selfprog;
	avr_extint_t	extint;
	avr_ioport_t	portb,portc,portd;
	avr_uart_t		uart;
	avr_adc_t		adc;
	avr_timer_t		timer0,timer1,timer2;
	avr_spi_t		spi;
	avr_twi_t		twi;
};

#ifdef SIM_CORENAME

#ifndef SIM_VECTOR_SIZE
#error SIM_VECTOR_SIZE is not declared
#endif
#ifndef SIM_MMCU
#error SIM_MMCU is not declared
#endif

struct mcu_t SIM_CORENAME = {
	.core = {
		.mmcu = SIM_MMCU,
		DEFAULT_CORE(SIM_VECTOR_SIZE),

		.init = mx8_init,
		.reset = mx8_reset,
	},
	AVR_EEPROM_DECLARE(EE_READY_vect),
	AVR_SELFPROG_DECLARE(SPMCSR, SELFPRGEN, SPM_READY_vect),
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

	.uart = {
		.disabled = AVR_IO_REGBIT(PRR,PRUSART0),
		.name = '0',
		.r_udr = UDR0,

		.txen = AVR_IO_REGBIT(UCSR0B, TXEN0),
		.rxen = AVR_IO_REGBIT(UCSR0B, RXEN0),
		.usbs = AVR_IO_REGBIT(UCSR0C, USBS0),
		.ucsz = AVR_IO_REGBITS(UCSR0C, UCSZ00, 0x3), // 2 bits
		.ucsz2 = AVR_IO_REGBIT(UCSR0B, UCSZ02), 	// 1 bits

		.r_ucsra = UCSR0A,
		.r_ucsrb = UCSR0B,
		.r_ucsrc = UCSR0C,
		.r_ubrrl = UBRR0L,
		.r_ubrrh = UBRR0H,
		.rxc = {
			.enable = AVR_IO_REGBIT(UCSR0B, RXCIE0),
			.raised = AVR_IO_REGBIT(UCSR0A, RXC0),
			.vector = USART_RX_vect,
		},
		.txc = {
			.enable = AVR_IO_REGBIT(UCSR0B, TXCIE0),
			.raised = AVR_IO_REGBIT(UCSR0A, TXC0),
			.vector = USART_TX_vect,
		},
		.udrc = {
			.enable = AVR_IO_REGBIT(UCSR0B, UDRIE0),
			.raised = AVR_IO_REGBIT(UCSR0A, UDRE0),
			.vector = USART_UDRE_vect,
		},
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

		.muxmode = {
			[0] = AVR_ADC_SINGLE(0), [1] = AVR_ADC_SINGLE(1),
			[2] = AVR_ADC_SINGLE(2), [3] = AVR_ADC_SINGLE(3),
			[4] = AVR_ADC_SINGLE(4), [5] = AVR_ADC_SINGLE(5),
			[6] = AVR_ADC_SINGLE(6), [7] = AVR_ADC_TEMP(),
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
		.disabled = AVR_IO_REGBIT(PRR,PRTIM0),
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
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */  /* External clock T1 is not handled */},

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
		.disabled = AVR_IO_REGBIT(PRR,PRTIM2),
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
	.spi = {
		.disabled = AVR_IO_REGBIT(PRR,PRSPI),

		.r_spdr = SPDR,
		.r_spcr = SPCR,
		.r_spsr = SPSR,

		.spe = AVR_IO_REGBIT(SPCR, SPE),
		.mstr = AVR_IO_REGBIT(SPCR, MSTR),

		.spr = { AVR_IO_REGBIT(SPCR, SPR0), AVR_IO_REGBIT(SPCR, SPR1), AVR_IO_REGBIT(SPSR, SPI2X) },
		.spi = {
			.enable = AVR_IO_REGBIT(SPCR, SPIE),
			.raised = AVR_IO_REGBIT(SPSR, SPIF),
			.vector = SPI_STC_vect,
		},
	},

	.twi = {
		.disabled = AVR_IO_REGBIT(PRR,PRTWI),

		.r_twcr = TWCR,
		.r_twsr = TWSR,
		.r_twbr = TWBR,
		.r_twdr = TWDR,
		.r_twar = TWAR,
		.r_twamr = TWAMR,

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
			.vector = TWI_vect,
		},
	},
	
};
#endif /* SIM_CORENAME */

#endif /* __SIM_MEGAX8_H__ */
