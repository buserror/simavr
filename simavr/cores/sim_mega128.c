/*
	sim_mega128.c

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

#include <stdio.h>
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

void m128_init(struct avr_t * avr);
void m128_reset(struct avr_t * avr);

#define _AVR_IO_H_
#define __ASSEMBLER__
#include "avr/iom128.h"

/*
 * This is a template for all of the 128 devices, hopefuly
 */
struct mcu_t {
	avr_t          core;
	avr_eeprom_t 	eeprom;
	avr_flash_t 	selfprog;
	avr_watchdog_t	watchdog;
	avr_extint_t	extint;
	avr_ioport_t	porta, portb, portc, portd, porte, portf, portg;
	avr_uart_t		uart0,uart1;
	avr_adc_t		adc;
	avr_timer_t		timer0,timer1,timer2,timer3;
	avr_spi_t		spi;
	avr_twi_t		twi;
} mcu_mega128 = {
	.core = {
		.mmcu = "atmega128",
		DEFAULT_CORE(4),

		.init = m128_init,
		.reset = m128_reset,

		.rampz = RAMPZ,	// extended program memory access
	},
	AVR_EEPROM_DECLARE_NOEEPM(EE_READY_vect),
	AVR_SELFPROG_DECLARE(SPMCSR, SPMEN, SPM_READY_vect),
	AVR_WATCHDOG_DECLARE_128(WDTCR, _VECTOR(0)),
	.extint = {
		AVR_EXTINT_DECLARE(0, 'D', PD0),
		AVR_EXTINT_DECLARE(1, 'D', PD1),
		AVR_EXTINT_DECLARE(2, 'D', PD2),
		AVR_EXTINT_DECLARE(3, 'D', PD3),
		AVR_EXTINT_DECLARE(4, 'E', PE4),
		AVR_EXTINT_DECLARE(5, 'E', PE5),
		AVR_EXTINT_DECLARE(6, 'E', PE6),
		AVR_EXTINT_DECLARE(7, 'E', PE7),
	},
	.porta = {  // no PCINTs in atmega128
		.name = 'A', .r_port = PORTA, .r_ddr = DDRA, .r_pin = PINA,
	},
	.portb = {
		.name = 'B', .r_port = PORTB, .r_ddr = DDRB, .r_pin = PINB,
	},
	.portc = {
		.name = 'C', .r_port = PORTC, .r_ddr = DDRC, .r_pin = PINC,
	},
	.portd = {
		.name = 'D', .r_port = PORTD, .r_ddr = DDRD, .r_pin = PIND,
	},
	.porte = {
		.name = 'E', .r_port = PORTE, .r_ddr = DDRE, .r_pin = PINE,
	},
	.portf = {
		.name = 'F', .r_port = PORTF, .r_ddr = DDRF, .r_pin = PINF,
	},
	.portg = {
		.name = 'G', .r_port = PORTG, .r_ddr = DDRG, .r_pin = PING,
	},

	.uart0 = {
	   // no PRUSART .disabled = AVR_IO_REGBIT(PRR,PRUSART0),
		.name = '0',
		.r_udr = UDR0,

		.txen = AVR_IO_REGBIT(UCSR0B, TXEN0),
		.rxen = AVR_IO_REGBIT(UCSR0B, RXEN0),
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
			.vector = USART0_RX_vect,
		},
		.txc = {
			.enable = AVR_IO_REGBIT(UCSR0B, TXCIE0),
			.raised = AVR_IO_REGBIT(UCSR0A, TXC0),
			.vector = USART0_TX_vect,
		},
		.udrc = {
			.enable = AVR_IO_REGBIT(UCSR0B, UDRIE0),
			.raised = AVR_IO_REGBIT(UCSR0A, UDRE0),
			.vector = USART0_UDRE_vect,
		},
	},
	.uart1 = {
	   // no PRUSART .disabled = AVR_IO_REGBIT(PRR,PRUSART1),
		.name = '1',
		.r_udr = UDR1,

		.txen = AVR_IO_REGBIT(UCSR1B, TXEN1),
		.rxen = AVR_IO_REGBIT(UCSR1B, RXEN1),
		.ucsz = AVR_IO_REGBITS(UCSR1C, UCSZ10, 0x3), // 2 bits
		.ucsz2 = AVR_IO_REGBIT(UCSR1B, UCSZ12), 	// 1 bits

		.r_ucsra = UCSR1A,
		.r_ucsrb = UCSR1B,
		.r_ucsrc = UCSR1C,
		.r_ubrrl = UBRR1L,
		.r_ubrrh = UBRR1H,
		.rxc = {
			.enable = AVR_IO_REGBIT(UCSR1B, RXCIE1),
			.raised = AVR_IO_REGBIT(UCSR1A, RXC1),
			.vector = USART1_RX_vect,
		},
		.txc = {
			.enable = AVR_IO_REGBIT(UCSR1B, TXCIE1),
			.raised = AVR_IO_REGBIT(UCSR1A, TXC1),
			.vector = USART1_TX_vect,
		},
		.udrc = {
			.enable = AVR_IO_REGBIT(UCSR1B, UDRIE1),
			.raised = AVR_IO_REGBIT(UCSR1A, UDRE1),
			.vector = USART1_UDRE_vect,
		},
	},
	.adc = {
		.r_admux = ADMUX,
		.mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1),
					AVR_IO_REGBIT(ADMUX, MUX2), AVR_IO_REGBIT(ADMUX, MUX3),
					AVR_IO_REGBIT(ADMUX, MUX4),},
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

			[ 8] = AVR_ADC_DIFF(0, 0,  10), [ 9] = AVR_ADC_DIFF(1, 0,  10),
			[10] = AVR_ADC_DIFF(0, 0, 200), [11] = AVR_ADC_DIFF(1, 0, 200),
			[12] = AVR_ADC_DIFF(2, 2,  10), [13] = AVR_ADC_DIFF(3, 2,  10),
			[14] = AVR_ADC_DIFF(2, 2, 200), [15] = AVR_ADC_DIFF(3, 2, 200),

			[16] = AVR_ADC_DIFF(0, 1,   1), [17] = AVR_ADC_DIFF(1, 1,   1),
			[18] = AVR_ADC_DIFF(2, 1,   1), [19] = AVR_ADC_DIFF(3, 1,   1),
			[20] = AVR_ADC_DIFF(4, 1,   1), [21] = AVR_ADC_DIFF(5, 1,   1),
			[22] = AVR_ADC_DIFF(6, 1,   1), [23] = AVR_ADC_DIFF(7, 1,   1),

			[24] = AVR_ADC_DIFF(0, 2,   1), [25] = AVR_ADC_DIFF(1, 2,   1),
			[26] = AVR_ADC_DIFF(2, 2,   1), [27] = AVR_ADC_DIFF(3, 2,   1),
			[28] = AVR_ADC_DIFF(4, 2,   1), [29] = AVR_ADC_DIFF(5, 2,   1),

			[30] = AVR_ADC_REF(1230),	// 1.1V
			[31] = AVR_ADC_REF(0),		// GND
		},

		.adc = {
			.enable = AVR_IO_REGBIT(ADCSRA, ADIE),
			.raised = AVR_IO_REGBIT(ADCSRA, ADIF),
			.vector = ADC_vect,
		},
	},
	.timer0 = {
		.name = '0',
		.wgm = { AVR_IO_REGBIT(TCCR0, WGM00), AVR_IO_REGBIT(TCCR0, WGM01) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL8(),
			// PHASE CORRECT 
			[2] = AVR_TIMER_WGM_CTC(),
			[3] = AVR_TIMER_WGM_FASTPWM8(),
		},
		.cs = { AVR_IO_REGBIT(TCCR0, CS00), AVR_IO_REGBIT(TCCR0, CS01), AVR_IO_REGBIT(TCCR0, CS02) },
		//		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */ },
		.cs_div = { 0, 0, 3 /* 8 */, 5 /* 32 */, 6 /* 64 */, 7 /* 128 */, 8 /* 256 */, 10 /* 1024 */},

		// asynchronous timer source bit.. if set, use 32khz frequency
		.as2 = AVR_IO_REGBIT(ASSR, AS0),
		
		.r_tcnt = TCNT0,

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE0),
			.raised = AVR_IO_REGBIT(TIFR, TOV0),
			.vector = TIMER0_OVF_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR0,
				.com = AVR_IO_REGBITS(TCCR0, COM00, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTB, PB4),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE0),
					.raised = AVR_IO_REGBIT(TIFR, OCF0),
					.vector = TIMER0_COMP_vect,
				},
			},
		},
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
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */  /* TODO: 2 External clocks */},

		.r_tcnt = TCNT1L,
		.r_icr = ICR1L,
		.r_icrh = ICR1H,
		.r_tcnth = TCNT1H,

		.ices = AVR_IO_REGBIT(TCCR1B, ICES1),
		.icp = AVR_IO_REGBIT(PORTD, 4),

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
				.com_pin = AVR_IO_REGBIT(PORTB, PB5),
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
				.com_pin = AVR_IO_REGBIT(PORTB, PB6),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE1B),
					.raised = AVR_IO_REGBIT(TIFR, OCF1B),
					.vector = TIMER1_COMPB_vect,
				},
			},
			[AVR_TIMER_COMPC] = {
				.r_ocr = OCR1CL,
				.r_ocrh = OCR1CH,
				.com = AVR_IO_REGBITS(TCCR1A, COM1C0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTB, PB7), // same as timer2
				.interrupt = {
					.enable = AVR_IO_REGBIT(ETIMSK, OCIE1C),
					.raised = AVR_IO_REGBIT(ETIFR, OCF1C),
					.vector = TIMER1_COMPC_vect,
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
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */ /* TODO external clock */ },

		.r_tcnt = TCNT2,
		
		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE2),
			.raised = AVR_IO_REGBIT(TIFR, TOV2),
			.vector = TIMER2_OVF_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR2,
				.com = AVR_IO_REGBITS(TCCR2, COM20, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTB, PB7), // same as timer1C
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE2),
					.raised = AVR_IO_REGBIT(TIFR, OCF2),
					.vector = TIMER2_COMP_vect,
				},
			},
		},
	},
	.timer3 = {
		.name = '3',
		.wgm = { AVR_IO_REGBIT(TCCR3A, WGM30), AVR_IO_REGBIT(TCCR3A, WGM31),
					AVR_IO_REGBIT(TCCR3B, WGM32), AVR_IO_REGBIT(TCCR3B, WGM33) },
		.wgm_op = {
			[0] = AVR_TIMER_WGM_NORMAL16(),
			// TODO: 1 PWM phase correct 8bit
			//       2 PWM phase correct 9bit
			//       3 PWM phase correct 10bit
			[4] = AVR_TIMER_WGM_CTC(),
			[5] = AVR_TIMER_WGM_FASTPWM8(),
			[6] = AVR_TIMER_WGM_FASTPWM9(),
			[7] = AVR_TIMER_WGM_FASTPWM10(),
			// TODO: 8 PWM phase and freq correct ICR
			//       9 PWM phase and freq correct OCR
			//       10
			//       11
			[12] = AVR_TIMER_WGM_ICCTC(),
			[14] = AVR_TIMER_WGM_ICPWM(),
			[15] = AVR_TIMER_WGM_OCPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR3B, CS30), AVR_IO_REGBIT(TCCR3B, CS31), AVR_IO_REGBIT(TCCR3B, CS32) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */  /* TODO: 2 External clocks */},

		.r_tcnt = TCNT3L,
		.r_icr = ICR3L,
		.r_icrh = ICR3H,
		.r_tcnth = TCNT3H,

		.ices = AVR_IO_REGBIT(TCCR3B, ICES3),
		.icp = AVR_IO_REGBIT(PORTE, 7),

		.overflow = {
			.enable = AVR_IO_REGBIT(ETIMSK, TOIE3),
			.raised = AVR_IO_REGBIT(ETIFR, TOV3),
			.vector = TIMER3_OVF_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR3AL,
				.r_ocrh = OCR3AH,	// 16 bits timers have two bytes of it
				.com = AVR_IO_REGBITS(TCCR3A, COM3A0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTE, PE3),
				.interrupt = {
					.enable = AVR_IO_REGBIT(ETIMSK, OCIE3A),
					.raised = AVR_IO_REGBIT(ETIFR, OCF3A),
					.vector = TIMER3_COMPA_vect,
				}
			},
			[AVR_TIMER_COMPB] = {
				.r_ocr = OCR3BL,
				.r_ocrh = OCR3BH,
				.com = AVR_IO_REGBITS(TCCR3A, COM3B0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTE, PE4),
				.interrupt = {
					.enable = AVR_IO_REGBIT(ETIMSK, OCIE3B),
					.raised = AVR_IO_REGBIT(ETIFR, OCF3B),
					.vector = TIMER3_COMPB_vect,
				}
			},
			[AVR_TIMER_COMPC] = {
				.r_ocr = OCR3CL,
				.r_ocrh = OCR3CH,
				.com = AVR_IO_REGBITS(TCCR3A, COM3C0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTE, PE5),
				.interrupt = {
					.enable = AVR_IO_REGBIT(ETIMSK, OCIE3C),
					.raised = AVR_IO_REGBIT(ETIFR, OCF3C),
					.vector = TIMER3_COMPC_vect,
				}
			}
		},
		.icr = {
			.enable = AVR_IO_REGBIT(ETIMSK, TICIE3),
			.raised = AVR_IO_REGBIT(ETIFR, ICF3),
			.vector = TIMER3_CAPT_vect,
		},
	},
	.spi = {

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
			.vector = TWI_vect,
		},
	},

};

static avr_t * make()
{
        return &mcu_mega128.core;
}

avr_kind_t mega128 = {
        .names = { "atmega128", "atmega128L" },
        .make = make
};

void m128_init(struct avr_t * avr)
{
	struct mcu_t * mcu = (struct mcu_t*)avr;

	printf("%s init\n", avr->mmcu);
	
	avr_eeprom_init(avr, &mcu->eeprom);
	avr_flash_init(avr, &mcu->selfprog);
	avr_extint_init(avr, &mcu->extint);
	avr_watchdog_init(avr, &mcu->watchdog);
	avr_ioport_init(avr, &mcu->porta);
	avr_ioport_init(avr, &mcu->portb);
	avr_ioport_init(avr, &mcu->portc);
	avr_ioport_init(avr, &mcu->portd);
	avr_ioport_init(avr, &mcu->porte);
	avr_ioport_init(avr, &mcu->portf);
	avr_ioport_init(avr, &mcu->portg);
	avr_uart_init(avr, &mcu->uart0);
	avr_uart_init(avr, &mcu->uart1);
	avr_adc_init(avr, &mcu->adc);
	avr_timer_init(avr, &mcu->timer0);
	avr_timer_init(avr, &mcu->timer1);
	avr_timer_init(avr, &mcu->timer2);
	avr_timer_init(avr, &mcu->timer3);
	avr_spi_init(avr, &mcu->spi);
	avr_twi_init(avr, &mcu->twi);
}

void m128_reset(struct avr_t * avr)
{
//	struct mcu_t * mcu = (struct mcu_t*)avr;
}
