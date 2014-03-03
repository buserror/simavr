/*
   sim_mega2560.c

   Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
   Copyright 2013 Yann GOUY <yann_gouy@yahoo.fr>

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

void m2560_init(struct avr_t * avr);
void m2560_reset(struct avr_t * avr);

#define _AVR_IO_H_
#define __ASSEMBLER__
#ifndef __AVR_ATmega2560__
#define __AVR_ATmega2560__
#endif
#include "avr/iom2560.h"

/*
 * This is a template for all of the 2560 devices, hopefully
 */
const struct mcu_t {
	avr_t			core;
	avr_eeprom_t	eeprom;
	avr_flash_t		selfprog;
	avr_watchdog_t	watchdog;
	avr_extint_t	extint;
	avr_ioport_t	porta, portb, portc, portd, porte, portf, portg, porth, portj, portk, portl;
	avr_uart_t		uart0,uart1;
	avr_uart_t		uart2,uart3;
	avr_adc_t		adc;
	avr_timer_t		timer0,timer1,timer2,timer3,timer4,timer5;
	avr_spi_t		spi;
	avr_twi_t		twi;
 } mcu_mega2560 = {
	.core = {
		.mmcu = "atmega2560",
		DEFAULT_CORE(4),

		.init = m2560_init,
		.reset = m2560_reset,

		.rampz = RAMPZ, // extended program memory access
	},
	AVR_EEPROM_DECLARE(EE_READY_vect),
	AVR_SELFPROG_DECLARE(SPMCSR, SPMEN, SPM_READY_vect),
	AVR_WATCHDOG_DECLARE(WDTCSR, WDT_vect),
	.extint = {
		AVR_EXTINT_MEGA_DECLARE(0, 'D', PD0, A),
		AVR_EXTINT_MEGA_DECLARE(1, 'D', PD1, A),
		AVR_EXTINT_MEGA_DECLARE(2, 'D', PD2, A),
		AVR_EXTINT_MEGA_DECLARE(3, 'D', PD3, A),
		AVR_EXTINT_MEGA_DECLARE(4, 'E', PE4, B),
		AVR_EXTINT_MEGA_DECLARE(5, 'E', PE5, B),
		AVR_EXTINT_MEGA_DECLARE(6, 'E', PE6, B),
		AVR_EXTINT_MEGA_DECLARE(7, 'E', PE7, B),
	},
	.porta = {
		.name = 'A', .r_port = PORTA, .r_ddr = DDRA, .r_pin = PINA,
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

	.porth = {
		.name = 'H', .r_port = PORTH, .r_ddr = DDRH, .r_pin = PINH,
	},
	.portj = {
		.name = 'J', .r_port = PORTJ, .r_ddr = DDRJ, .r_pin = PINJ,
	},
	.portk = {
		.name = 'K', .r_port = PORTK, .r_ddr = DDRK, .r_pin = PINK,
	},
	.portl = {
		.name = 'L', .r_port = PORTL, .r_ddr = DDRL, .r_pin = PINL,
	},

	.uart0 = {
		.disabled = AVR_IO_REGBIT(PRR0,PRUSART0),
		.name = '0',
		.r_udr = UDR0,

		.txen = AVR_IO_REGBIT(UCSR0B, TXEN0),
		.rxen = AVR_IO_REGBIT(UCSR0B, RXEN0),
		.ucsz = AVR_IO_REGBITS(UCSR0C, UCSZ00, 0x3), // 2 bits
		.ucsz2 = AVR_IO_REGBIT(UCSR0B, UCSZ02),		// 1 bits

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
		.disabled = AVR_IO_REGBIT(PRR1,PRUSART1),
		.name = '1',
		.r_udr = UDR1,

		.txen = AVR_IO_REGBIT(UCSR1B, TXEN1),
		.rxen = AVR_IO_REGBIT(UCSR1B, RXEN1),
		.ucsz = AVR_IO_REGBITS(UCSR1C, UCSZ10, 0x3), // 2 bits
		.ucsz2 = AVR_IO_REGBIT(UCSR1B, UCSZ12),		// 1 bits

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

	.uart2 = {
		.disabled = AVR_IO_REGBIT(PRR1,PRUSART2),
		.name = '2',
		.r_udr = UDR2,

		.txen = AVR_IO_REGBIT(UCSR2B, TXEN2),
		.rxen = AVR_IO_REGBIT(UCSR2B, RXEN2),
		.ucsz = AVR_IO_REGBITS(UCSR2C, UCSZ20, 0x3), // 2 bits
		.ucsz2 = AVR_IO_REGBIT(UCSR2B, UCSZ22),		// 1 bits

		.r_ucsra = UCSR2A,
		.r_ucsrb = UCSR2B,
		.r_ucsrc = UCSR2C,
		.r_ubrrl = UBRR2L,
		.r_ubrrh = UBRR2H,
		.rxc = {
			 .enable = AVR_IO_REGBIT(UCSR2B, RXCIE2),
			 .raised = AVR_IO_REGBIT(UCSR2A, RXC2),
			 .vector = USART2_RX_vect,
		},
		.txc = {
			 .enable = AVR_IO_REGBIT(UCSR2B, TXCIE2),
			 .raised = AVR_IO_REGBIT(UCSR2A, TXC2),
			 .vector = USART2_TX_vect,
		},
		.udrc = {
			 .enable = AVR_IO_REGBIT(UCSR2B, UDRIE2),
			 .raised = AVR_IO_REGBIT(UCSR2A, UDRE2),
			 .vector = USART2_UDRE_vect,
		},
	},

	.uart3 = {
		.disabled = AVR_IO_REGBIT(PRR1,PRUSART3),
		.name = '3',
		.r_udr = UDR3,

		.txen = AVR_IO_REGBIT(UCSR3B, TXEN3),
		.rxen = AVR_IO_REGBIT(UCSR3B, RXEN3),
		.ucsz = AVR_IO_REGBITS(UCSR3C, UCSZ30, 0x3), // 2 bits
		.ucsz2 = AVR_IO_REGBIT(UCSR3B, UCSZ32),		// 1 bits

		.r_ucsra = UCSR3A,
		.r_ucsrb = UCSR3B,
		.r_ucsrc = UCSR3C,
		.r_ubrrl = UBRR3L,
		.r_ubrrh = UBRR3H,
		.rxc = {
			 .enable = AVR_IO_REGBIT(UCSR3B, RXCIE3),
			 .raised = AVR_IO_REGBIT(UCSR3A, RXC3),
			 .vector = USART3_RX_vect,
		},
		.txc = {
			 .enable = AVR_IO_REGBIT(UCSR3B, TXCIE3),
			 .raised = AVR_IO_REGBIT(UCSR3A, TXC3),
			 .vector = USART3_TX_vect,
		},
		.udrc = {
			 .enable = AVR_IO_REGBIT(UCSR3B, UDRIE3),
			 .raised = AVR_IO_REGBIT(UCSR3A, UDRE3),
			 .vector = USART3_UDRE_vect,
		},
	},
	.adc = {
		.r_admux = ADMUX,
		.mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1),
						AVR_IO_REGBIT(ADMUX, MUX2), AVR_IO_REGBIT(ADMUX, MUX3),
						AVR_IO_REGBIT(ADMUX, MUX4),AVR_IO_REGBIT(ADCSRB, MUX5),},
		.ref = { AVR_IO_REGBIT(ADMUX, REFS0), AVR_IO_REGBIT(ADMUX, REFS1)},
		.ref_values = { [1] = ADC_VREF_AVCC, [2] = ADC_VREF_V110, [3] = ADC_VREF_V256 },

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
			 [6] = AVR_ADC_SINGLE(6), [7] = AVR_ADC_SINGLE(7),

			 [ 8] = AVR_ADC_DIFF(0, 0,  10), [ 9] = AVR_ADC_DIFF(1, 0,  10),
			 [10] = AVR_ADC_DIFF(0, 0, 200), [11] = AVR_ADC_DIFF(1, 0, 200),
			 [12] = AVR_ADC_DIFF(2, 2,  10), [13] = AVR_ADC_DIFF(3, 2,  10),
			 [14] = AVR_ADC_DIFF(2, 2, 200), [15] = AVR_ADC_DIFF(3, 2, 200),

			 [16] = AVR_ADC_DIFF(0, 1,	1), [17] = AVR_ADC_DIFF(1, 1,	1),
			 [18] = AVR_ADC_DIFF(2, 1,	1), [19] = AVR_ADC_DIFF(3, 1,	1),
			 [20] = AVR_ADC_DIFF(4, 1,	1), [21] = AVR_ADC_DIFF(5, 1,	1),
			 [22] = AVR_ADC_DIFF(6, 1,	1), [23] = AVR_ADC_DIFF(7, 1,	1),

			 [24] = AVR_ADC_DIFF(0, 2,	1), [25] = AVR_ADC_DIFF(1, 2,	1),
			 [26] = AVR_ADC_DIFF(2, 2,	1), [27] = AVR_ADC_DIFF(3, 2,	1),
			 [28] = AVR_ADC_DIFF(4, 2,	1), [29] = AVR_ADC_DIFF(5, 2,	1),

			 [30] = AVR_ADC_REF(1100),	// 1.1V
			 [31] = AVR_ADC_REF(0),		// GND

			 [32] = AVR_ADC_SINGLE( 8), [33] = AVR_ADC_SINGLE( 9),
			 [34] = AVR_ADC_SINGLE(10), [35] = AVR_ADC_SINGLE(11),
			 [36] = AVR_ADC_SINGLE(12), [37] = AVR_ADC_SINGLE(13),
			 [38] = AVR_ADC_SINGLE(14), [39] = AVR_ADC_SINGLE(15),

			 [40] = AVR_ADC_DIFF( 8,  8,  10), [41] = AVR_ADC_DIFF( 9,  8,  10),
			 [42] = AVR_ADC_DIFF( 8,  8, 200), [43] = AVR_ADC_DIFF( 9,  8, 200),

			 [44] = AVR_ADC_DIFF(10, 10,  10), [45] = AVR_ADC_DIFF(11, 10,  10),
			 [46] = AVR_ADC_DIFF(10, 10, 200), [47] = AVR_ADC_DIFF(11, 10, 200),

			 [48] = AVR_ADC_DIFF( 8,  9,	1), [49] = AVR_ADC_DIFF( 9,  9,	1),
			 [50] = AVR_ADC_DIFF(10,  9,	1), [51] = AVR_ADC_DIFF(11,  9,	1),
			 [52] = AVR_ADC_DIFF(12,  9,	1), [53] = AVR_ADC_DIFF(13,  9,	1),
			 [54] = AVR_ADC_DIFF(14,  9,	1), [55] = AVR_ADC_DIFF(15,  9,	1),

			 [56] = AVR_ADC_DIFF( 8, 10,	1), [57] = AVR_ADC_DIFF( 9, 10,	1),
			 [58] = AVR_ADC_DIFF(10, 10,	1), [59] = AVR_ADC_DIFF(11, 10,	1),
			 [60] = AVR_ADC_DIFF(12, 10,	1), [61] = AVR_ADC_DIFF(13, 10,	1),
		},

		.adc = {
			 .enable = AVR_IO_REGBIT(ADCSRA, ADIE),
			 .raised = AVR_IO_REGBIT(ADCSRA, ADIF),
			 .vector = ADC_vect,
		},
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
			 .vector = TIMER0_OVF_vect,
		},
		.comp = {
			 [AVR_TIMER_COMPA] = {
					.r_ocr = OCR0A,
					.com = AVR_IO_REGBITS(TCCR0A, COM0A0, 0x3),
					.com_pin = AVR_IO_REGBIT(PORTB, PB7), // same as timer1C
					.interrupt = {
						.enable = AVR_IO_REGBIT(TIMSK0, OCIE0A),
						.raised = AVR_IO_REGBIT(TIFR0, OCF0A),
						.vector = TIMER0_COMPA_vect,
					},
			 },
			 [AVR_TIMER_COMPB] = {
					.r_ocr = OCR0B,
					.com = AVR_IO_REGBITS(TCCR0A, COM0B0, 0x3),
					.com_pin = AVR_IO_REGBIT(PORTG, PG5),
					.interrupt = {
						.enable = AVR_IO_REGBIT(TIMSK0, OCIE0B),
						.raised = AVR_IO_REGBIT(TIFR0, OCF0B),
						.vector = TIMER0_COMPB_vect,
					},
			 },
		},
	},
	.timer1 = {
		.name = '1',
		.disabled = AVR_IO_REGBIT(PRR0,PRTIM1),
		.wgm = { AVR_IO_REGBIT(TCCR1A, WGM10), AVR_IO_REGBIT(TCCR1A, WGM11),
						AVR_IO_REGBIT(TCCR1B, WGM12), AVR_IO_REGBIT(TCCR1B, WGM13) },
		.wgm_op = {
			 [0] = AVR_TIMER_WGM_NORMAL16(),
			 // TODO: 1 PWM phase correct 8bit
			 //		 2 PWM phase correct 9bit
			 //		 3 PWM phase correct 10bit
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
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */  /* External clock T1 is not handled */},

		.r_tcnt = TCNT1L,
		.r_tcnth = TCNT1H,
		.r_icr = ICR1L,
		.r_icrh = ICR1H,

		.ices = AVR_IO_REGBIT(TCCR1B, ICES1),
		.icp = AVR_IO_REGBIT(PORTD, PD4),

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
					.com_pin = AVR_IO_REGBIT(PORTB, PB5),
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
					.com_pin = AVR_IO_REGBIT(PORTB, PB6),
					.interrupt = {
						.enable = AVR_IO_REGBIT(TIMSK1, OCIE1B),
						.raised = AVR_IO_REGBIT(TIFR1, OCF1B),
						.vector = TIMER1_COMPB_vect,
					},
			 },
			 [AVR_TIMER_COMPC] = {
					.r_ocr = OCR1CL,
					.r_ocrh = OCR1CH,
					.com = AVR_IO_REGBITS(TCCR1A, COM1C0, 0x3),
					.com_pin = AVR_IO_REGBIT(PORTB, PB7), // same as timer0A
					.interrupt = {
						.enable = AVR_IO_REGBIT(TIMSK1, OCIE1C),
						.raised = AVR_IO_REGBIT(TIFR1, OCF1C),
						.vector = TIMER1_COMPC_vect,
					},
			 },
		},

	},
	.timer2 = {
		.name = '2',
		.wgm = { AVR_IO_REGBIT(TCCR2A, WGM20), AVR_IO_REGBIT(TCCR2A, WGM21), AVR_IO_REGBIT(TCCR2B, WGM22) },
		.wgm_op = {
			 [0] = AVR_TIMER_WGM_NORMAL8(),
			 // TODO 1 pwm phase correct 
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
					.com_pin = AVR_IO_REGBIT(PORTB, PB4),
					.interrupt = {
						.enable = AVR_IO_REGBIT(TIMSK2, OCIE2A),
						.raised = AVR_IO_REGBIT(TIFR2, OCF2A),
						.vector = TIMER2_COMPA_vect,
					},
			 },
				// TIMER2_COMPB is only appeared in 2560
			 //[AVR_TIMER_COMPB] = {
			 //	.r_ocr = OCR2B,
			 //	.com = AVR_IO_REGBITS(TCCR2A, COM2B0, 0x3),
			 //	.com_pin = AVR_IO_REGBIT(PORTH, PH6),
			 //	.interrupt = {
			 //		.enable = AVR_IO_REGBIT(TIMSK2, OCIE2B),
			 //		.raised = AVR_IO_REGBIT(TIFR2, OCF2B),
			 //		.vector = TIMER2_COMPB_vect,
			 //	},
			 //},
		},
	},
	.timer3 = {
		.name = '3',
		.wgm = { AVR_IO_REGBIT(TCCR3A, WGM30), AVR_IO_REGBIT(TCCR3A, WGM31),
						AVR_IO_REGBIT(TCCR3B, WGM32), AVR_IO_REGBIT(TCCR3B, WGM33) },
		.wgm_op = {
			 [0] = AVR_TIMER_WGM_NORMAL16(),
			 // TODO: 1 PWM phase correct 8bit
			 //		 2 PWM phase correct 9bit
			 //		 3 PWM phase correct 10bit
			 [4] = AVR_TIMER_WGM_CTC(),
			 [5] = AVR_TIMER_WGM_FASTPWM8(),
			 [6] = AVR_TIMER_WGM_FASTPWM9(),
			 [7] = AVR_TIMER_WGM_FASTPWM10(),
			 // TODO: 8 PWM phase and freq correct ICR
			 //		 9 PWM phase and freq correct OCR
			 //		 10
			 //		 11
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
		.icp = AVR_IO_REGBIT(PORTE, PE7),

		.overflow = {
			 .enable = AVR_IO_REGBIT(TIMSK3, TOIE3),
			 .raised = AVR_IO_REGBIT(TIFR3, TOV3),
			 .vector = TIMER3_OVF_vect,
		},
		.comp = {
			 [AVR_TIMER_COMPA] = {
					.r_ocr = OCR3AL,
					.r_ocrh = OCR3AH,	// 16 bits timers have two bytes of it
					.com = AVR_IO_REGBITS(TCCR3A, COM3A0, 0x3),
					.com_pin = AVR_IO_REGBIT(PORTE, PE3),
					.interrupt = {
						.enable = AVR_IO_REGBIT(TIMSK3, OCIE3A),
						.raised = AVR_IO_REGBIT(TIFR3, OCF3A),
						.vector = TIMER3_COMPA_vect,
					}
			 },
			 [AVR_TIMER_COMPB] = {
					.r_ocr = OCR3BL,
					.r_ocrh = OCR3BH,
					.com = AVR_IO_REGBITS(TCCR3A, COM3B0, 0x3),
					.com_pin = AVR_IO_REGBIT(PORTE, PE4),
					.interrupt = {
						.enable = AVR_IO_REGBIT(TIMSK3, OCIE3B),
						.raised = AVR_IO_REGBIT(TIFR3, OCF3B),
						.vector = TIMER3_COMPB_vect,
					}
			 },
			 [AVR_TIMER_COMPC] = {
					.r_ocr = OCR3CL,
					.r_ocrh = OCR3CH,
					.com = AVR_IO_REGBITS(TCCR3A, COM3C0, 0x3),
					.com_pin = AVR_IO_REGBIT(PORTE, PE5),
					.interrupt = {
						.enable = AVR_IO_REGBIT(TIMSK3, OCIE3C),
						.raised = AVR_IO_REGBIT(TIFR3, OCF3C),
						.vector = TIMER3_COMPC_vect,
					}
			 }
		},
		.icr = {
			 .enable = AVR_IO_REGBIT(TIMSK3, ICIE3),
			 .raised = AVR_IO_REGBIT(TIFR3, ICF3),
			 .vector = TIMER3_CAPT_vect,
		},
	},
	.timer4 = {
		.name = '4',
		.disabled = AVR_IO_REGBIT(PRR1,PRTIM4),
		.wgm = { AVR_IO_REGBIT(TCCR4A, WGM40), AVR_IO_REGBIT(TCCR4A, WGM41),
						AVR_IO_REGBIT(TCCR4B, WGM42), AVR_IO_REGBIT(TCCR4B, WGM43) },
		.wgm_op = {
			 [0] = AVR_TIMER_WGM_NORMAL16(),
			 // TODO: 1 PWM phase correct 8bit
			 //		 2 PWM phase correct 9bit
			 //		 3 PWM phase correct 10bit
			 [4] = AVR_TIMER_WGM_CTC(),
			 [5] = AVR_TIMER_WGM_FASTPWM8(),
			 [6] = AVR_TIMER_WGM_FASTPWM9(),
			 [7] = AVR_TIMER_WGM_FASTPWM10(),
			 // TODO: 8, 9 PWM phase and freq correct ICR & 10, 11
			 [12] = AVR_TIMER_WGM_ICCTC(),
			 [14] = AVR_TIMER_WGM_ICPWM(),
			 [15] = AVR_TIMER_WGM_OCPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR4B, CS40), AVR_IO_REGBIT(TCCR4B, CS41), AVR_IO_REGBIT(TCCR4B, CS42) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */  /* External clock T1 is not handled */},

		.r_tcnt = TCNT4L,
		.r_tcnth = TCNT4H,
		.r_icr = ICR4L,
		.r_icrh = ICR4H,

		.ices = AVR_IO_REGBIT(TCCR4B, ICES4),
		.icp = AVR_IO_REGBIT(PORTL, PL0),

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
					.com_pin = AVR_IO_REGBIT(PORTH, PH3),
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
					.com_pin = AVR_IO_REGBIT(PORTH, PH4),
					.interrupt = {
						.enable = AVR_IO_REGBIT(TIMSK4, OCIE4B),
						.raised = AVR_IO_REGBIT(TIFR4, OCF4B),
						.vector = TIMER4_COMPB_vect,
					},
			 },
			 [AVR_TIMER_COMPC] = {
					.r_ocr = OCR4CL,
					.r_ocrh = OCR4CH,
					.com = AVR_IO_REGBITS(TCCR4A, COM4C0, 0x3),
					.com_pin = AVR_IO_REGBIT(PORTH, PH5),
					.interrupt = {
						.enable = AVR_IO_REGBIT(TIMSK4, OCIE4C),
						.raised = AVR_IO_REGBIT(TIFR4, OCF4C),
						.vector = TIMER4_COMPC_vect,
					},
			 },
		},

	},
	.timer5 = {
		.name = '5',
		.disabled = AVR_IO_REGBIT(PRR1,PRTIM5),
		.wgm = { AVR_IO_REGBIT(TCCR5A, WGM50), AVR_IO_REGBIT(TCCR5A, WGM51),
						AVR_IO_REGBIT(TCCR5B, WGM52), AVR_IO_REGBIT(TCCR5B, WGM53) },
		.wgm_op = {
			 [0] = AVR_TIMER_WGM_NORMAL16(),
			 // TODO: 1 PWM phase correct 8bit
			 //		 2 PWM phase correct 9bit
			 //		 3 PWM phase correct 10bit
			 [4] = AVR_TIMER_WGM_CTC(),
			 [5] = AVR_TIMER_WGM_FASTPWM8(),
			 [6] = AVR_TIMER_WGM_FASTPWM9(),
			 [7] = AVR_TIMER_WGM_FASTPWM10(),
			 // TODO: 8, 9 PWM phase and freq correct ICR & 10, 11
			 [12] = AVR_TIMER_WGM_ICCTC(),
			 [14] = AVR_TIMER_WGM_ICPWM(),
			 [15] = AVR_TIMER_WGM_OCPWM(),
		},
		.cs = { AVR_IO_REGBIT(TCCR5B, CS50), AVR_IO_REGBIT(TCCR5B, CS51), AVR_IO_REGBIT(TCCR5B, CS52) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */  /* External clock T1 is not handled */},

		.r_tcnt = TCNT5L,
		.r_tcnth = TCNT5H,
		.r_icr = ICR5L,
		.r_icrh = ICR5H,

		.ices = AVR_IO_REGBIT(TCCR5B, ICES5),
		.icp = AVR_IO_REGBIT(PORTL, PL1),

		.overflow = {
			 .enable = AVR_IO_REGBIT(TIMSK5, TOIE5),
			 .raised = AVR_IO_REGBIT(TIFR5, TOV5),
			 .vector = TIMER5_OVF_vect,
		},
		.icr = {
			 .enable = AVR_IO_REGBIT(TIMSK5, ICIE5),
			 .raised = AVR_IO_REGBIT(TIFR5, ICF5),
			 .vector = TIMER5_CAPT_vect,
		},
		.comp = {
			 [AVR_TIMER_COMPA] = {
					.r_ocr = OCR5AL,
					.r_ocrh = OCR5AH,	// 16 bits timers have two bytes of it
					.com = AVR_IO_REGBITS(TCCR5A, COM5A0, 0x3),
					.com_pin = AVR_IO_REGBIT(PORTL, PL3),
					.interrupt = {
						.enable = AVR_IO_REGBIT(TIMSK5, OCIE5A),
						.raised = AVR_IO_REGBIT(TIFR5, OCF5A),
						.vector = TIMER5_COMPA_vect,
					},
			 },
			 [AVR_TIMER_COMPB] = {
					.r_ocr = OCR5BL,
					.r_ocrh = OCR5BH,
					.com = AVR_IO_REGBITS(TCCR5A, COM5B0, 0x3),
					.com_pin = AVR_IO_REGBIT(PORTL, PL4),
					.interrupt = {
						.enable = AVR_IO_REGBIT(TIMSK5, OCIE5B),
						.raised = AVR_IO_REGBIT(TIFR5, OCF5B),
						.vector = TIMER5_COMPB_vect,
					},
			 },
			 [AVR_TIMER_COMPC] = {
					.r_ocr = OCR5CL,
					.r_ocrh = OCR5CH,
					.com = AVR_IO_REGBITS(TCCR5A, COM5C0, 0x3),
					.com_pin = AVR_IO_REGBIT(PORTL, PL5),
					.interrupt = {
						.enable = AVR_IO_REGBIT(TIMSK5, OCIE5C),
						.raised = AVR_IO_REGBIT(TIFR5, OCF5C),
						.vector = TIMER5_COMPC_vect,
					},
			 },
		},

	},
	.spi = {
		.disabled = AVR_IO_REGBIT(PRR0,PRSPI),

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
			 .raise_sticky = 1,
			 .vector = TWI_vect,
		},
	},
};

static avr_t * make()
{
	return avr_core_allocate(&mcu_mega2560.core, sizeof(struct mcu_t));
}

avr_kind_t mega2560 = {
		 .names = { "atmega2560" },
		 .make = make
};

void m2560_init(struct avr_t * avr)
{
	struct mcu_t * mcu = (struct mcu_t*)avr;
	
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
	avr_ioport_init(avr, &mcu->porth);
	avr_ioport_init(avr, &mcu->portj);
	avr_ioport_init(avr, &mcu->portk);
	avr_ioport_init(avr, &mcu->portl);

	avr_uart_init(avr, &mcu->uart0);
	avr_uart_init(avr, &mcu->uart1);
	avr_uart_init(avr, &mcu->uart2);
	avr_uart_init(avr, &mcu->uart3);
	avr_adc_init(avr, &mcu->adc);
	avr_timer_init(avr, &mcu->timer0);
	avr_timer_init(avr, &mcu->timer1);
	avr_timer_init(avr, &mcu->timer2);
	avr_timer_init(avr, &mcu->timer3);
	avr_timer_init(avr, &mcu->timer4);
	avr_timer_init(avr, &mcu->timer5);
	avr_spi_init(avr, &mcu->spi);
	avr_twi_init(avr, &mcu->twi);
}

void m2560_reset(struct avr_t * avr)
{
// struct mcu_t * mcu = (struct mcu_t*)avr;
}


