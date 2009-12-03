/*
	sim_tiny85.c

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
#include "sim_avr.h"
#include "sim_core_declare.h"
#include "avr_eeprom.h"
#include "avr_ioport.h"
#include "avr_timer8.h"

#define _AVR_IO_H_
#define __ASSEMBLER__
#include "avr/iotn85.h"

static void init(struct avr_t * avr);
static void reset(struct avr_t * avr);


static struct mcu_t {
	avr_t core;
	avr_eeprom_t 	eeprom;
	avr_ioport_t	portb;
	avr_timer8_t	timer0, timer1;
} mcu = {
	.core = {
		.mmcu = "attiny85",
		DEFAULT_CORE(2),

		.init = init,
		.reset = reset,
	},
	AVR_EEPROM_DECLARE(EE_RDY_vect),
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
		.cs = { AVR_IO_REGBIT(TCCR0B, CS00), AVR_IO_REGBIT(TCCR0B, CS01), AVR_IO_REGBIT(TCCR0B, CS02) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */ },

		.r_ocra = OCR0A,
		.r_ocrb = OCR0B,
		.r_tcnt = TCNT0,

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE0),
			.raised = AVR_IO_REGBIT(TIFR, TOV0),
			.vector = TIMER0_OVF_vect,
		},
		.compa = {
			.enable = AVR_IO_REGBIT(TIMSK, OCIE0A),
			.raised = AVR_IO_REGBIT(TIFR, OCF0A),
			.vector = TIMER0_COMPA_vect,
		},
		.compb = {
			.enable = AVR_IO_REGBIT(TIMSK, OCIE0B),
			.raised = AVR_IO_REGBIT(TIFR, OCF0B),
			.vector = TIMER0_COMPB_vect,
		},
	},
	.timer1 = {
		.name = '1',
		// no wgm bits
		.cs = { AVR_IO_REGBIT(TCCR1, CS10), AVR_IO_REGBIT(TCCR1, CS11), AVR_IO_REGBIT(TCCR1, CS12), AVR_IO_REGBIT(TCCR1, CS13) },
		.cs_div = { 0, 0, 1 /* 2 */, 2 /* 4 */, 3 /* 8 */, 4 /* 16 */ },

		.r_ocra = OCR1A,
		.r_ocrb = OCR1B,
		.r_ocrc = OCR1C,
		.r_tcnt = TCNT1,

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE1),
			.raised = AVR_IO_REGBIT(TIFR, TOV1),
			.vector = TIMER1_OVF_vect,
		},
		.compa = {
			.enable = AVR_IO_REGBIT(TIMSK, OCIE1A),
			.raised = AVR_IO_REGBIT(TIFR, OCF1A),
			.vector = TIMER1_COMPA_vect,
		},
		.compb = {
			.enable = AVR_IO_REGBIT(TIMSK, OCIE1B),
			.raised = AVR_IO_REGBIT(TIFR, OCF1B),
			.vector = TIMER1_COMPB_vect,
		},
	},


};

static avr_t * make()
{
	return &mcu.core;
}

avr_kind_t tiny85 = {
	.names = { "attiny85" },
	.make = make
};

static void init(struct avr_t * avr)
{
	struct mcu_t * mcu = (struct mcu_t*)avr;
	
	printf("%s init\n", avr->mmcu);
	
	avr_eeprom_init(avr, &mcu->eeprom);
	avr_ioport_init(avr, &mcu->portb);
	avr_timer8_init(avr, &mcu->timer0);
	avr_timer8_init(avr, &mcu->timer1);
}

static void reset(struct avr_t * avr)
{
//	struct mcu_t * mcu = (struct mcu_t*)avr;
}
