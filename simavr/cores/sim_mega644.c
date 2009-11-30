/*
	sim_mega644.c

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

#include </usr/include/stdio.h>
#include "simavr.h"
#include "sim_core_declare.h"
#include "avr_eeprom.h"
#include "avr_ioport.h"
#include "avr_uart.h"
#include "avr_timer8.h"

#define _AVR_IO_H_
#define __ASSEMBLER__
#include "avr/iom644.h"

static void init(struct avr_t * avr);
static void reset(struct avr_t * avr);


static struct mcu_t {
	avr_t core;
	avr_eeprom_t 	eeprom;
	avr_ioport_t	porta, portb, portc, portd;
	avr_uart_t		uart0,uart1;
	avr_timer8_t	timer0,timer2;
} mcu = {
	.core = {
		.mmcu = "atmega644",
		DEFAULT_CORE(4),

		.init = init,
		.reset = reset,
	},
	AVR_EEPROM_DECLARE(EE_READY_vect),
	.porta = {
		.name = 'A', .r_port = PORTA, .r_ddr = DDRA, .r_pin = PINA,
		.pcint = {
			.enable = AVR_IO_REGBIT(PCICR, PCIE0),
			.raised = AVR_IO_REGBIT(PCIFR, PCIF0),
			.vector = PCINT0_vect,
		},
		.r_pcint = PCMSK0,
	},
	.portb = {
		.name = 'B', .r_port = PORTB, .r_ddr = DDRB, .r_pin = PINB,
		.pcint = {
			.enable = AVR_IO_REGBIT(PCICR, PCIE1),
			.raised = AVR_IO_REGBIT(PCIFR, PCIF1),
			.vector = PCINT1_vect,
		},
		.r_pcint = PCMSK1,
	},
	.portc = {
		.name = 'C', .r_port = PORTC, .r_ddr = DDRC, .r_pin = PINC,
		.pcint = {
			.enable = AVR_IO_REGBIT(PCICR, PCIE2),
			.raised = AVR_IO_REGBIT(PCIFR, PCIF2),
			.vector = PCINT2_vect,
		},
		.r_pcint = PCMSK2,
	},
	.portd = {
		.name = 'D', .r_port = PORTD, .r_ddr = DDRD, .r_pin = PIND,
		.pcint = {
			.enable = AVR_IO_REGBIT(PCICR, PCIE3),
			.raised = AVR_IO_REGBIT(PCIFR, PCIF3),
			.vector = PCINT3_vect,
		},
		.r_pcint = PCMSK3,
	},

	.uart0 = {
		.disabled = AVR_IO_REGBIT(PRR,PRUSART0),
		.name = '0',
		.r_udr = UDR0,

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
		.disabled = AVR_IO_REGBIT(PRR,PRUSART1),
		.name = '1',
		.r_udr = UDR1,

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

	.timer0 = {
		.name = '0',
		.wgm = { AVR_IO_REGBIT(TCCR0A, WGM00), AVR_IO_REGBIT(TCCR0A, WGM01), AVR_IO_REGBIT(TCCR0B, WGM02) },
		.cs = { AVR_IO_REGBIT(TCCR0B, CS00), AVR_IO_REGBIT(TCCR0B, CS01), AVR_IO_REGBIT(TCCR0B, CS02) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */ },

		.r_ocra = OCR0A,
		.r_ocrb = OCR0B,
		.r_tcnt = TCNT0,

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK0, TOIE0),
			.raised = AVR_IO_REGBIT(TIFR0, TOV0),
			.vector = TIMER0_OVF_vect,
		},
		.compa = {
			.enable = AVR_IO_REGBIT(TIMSK0, OCIE0A),
			.raised = AVR_IO_REGBIT(TIFR0, OCF0A),
			.vector = TIMER0_COMPA_vect,
		},
		.compb = {
			.enable = AVR_IO_REGBIT(TIMSK0, OCIE0B),
			.raised = AVR_IO_REGBIT(TIFR0, OCF0B),
			.vector = TIMER0_COMPB_vect,
		},
	},
	.timer2 = {
		.name = '2',
		.wgm = { AVR_IO_REGBIT(TCCR2A, WGM20), AVR_IO_REGBIT(TCCR2A, WGM21), AVR_IO_REGBIT(TCCR2B, WGM22) },
		.cs = { AVR_IO_REGBIT(TCCR2B, CS20), AVR_IO_REGBIT(TCCR2B, CS21), AVR_IO_REGBIT(TCCR2B, CS22) },
		.cs_div = { 0, 0, 3 /* 8 */, 5 /* 32 */, 6 /* 64 */, 7 /* 128 */, 8 /* 256 */, 10 /* 1024 */ },

		.r_ocra = OCR2A,
		.r_ocrb = OCR2B,
		.r_tcnt = TCNT2,
		
		// asynchronous timer source bit.. if set, use 32khz frequency
		.as2 = AVR_IO_REGBIT(ASSR, AS2),
		
		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK2, TOIE2),
			.raised = AVR_IO_REGBIT(TIFR2, TOV2),
			.vector = TIMER2_OVF_vect,
		},
		.compa = {
			.enable = AVR_IO_REGBIT(TIMSK2, OCIE2A),
			.raised = AVR_IO_REGBIT(TIFR2, OCF2A),
			.vector = TIMER2_COMPA_vect,
		},
		.compb = {
			.enable = AVR_IO_REGBIT(TIMSK2, OCIE2B),
			.raised = AVR_IO_REGBIT(TIFR2, OCF2B),
			.vector = TIMER2_COMPB_vect,
		},
	},
};

static avr_t * make()
{
	return &mcu.core;
}

avr_kind_t mega644 = {
	.names = { "atmega644", "atmega644p" },
	.make = make
};

static void init(struct avr_t * avr)
{
	struct mcu_t * mcu = (struct mcu_t*)avr;

	printf("%s init\n", avr->mmcu);
	
	avr_eeprom_init(avr, &mcu->eeprom);
	avr_ioport_init(avr, &mcu->porta);
	avr_ioport_init(avr, &mcu->portb);
	avr_ioport_init(avr, &mcu->portc);
	avr_ioport_init(avr, &mcu->portd);
	avr_uart_init(avr, &mcu->uart0);
	avr_uart_init(avr, &mcu->uart1);
	avr_timer8_init(avr, &mcu->timer0);
	avr_timer8_init(avr, &mcu->timer2);
}

static void reset(struct avr_t * avr)
{
//	struct mcu_t * mcu = (struct mcu_t*)avr;
}
