/*
	sim_tiny2313.c

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

#include "avr_eeprom.h"
#include "avr_watchdog.h"
#include "avr_extint.h"
#include "avr_ioport.h"
#include "avr_uart.h"
#include "avr_timer.h"
#include "avr_acomp.h"
#include "avr_usi.h"

static void init(struct avr_t * avr);
static void reset(struct avr_t * avr);

#define _AVR_IO_H_
#define __ASSEMBLER__
#include "avr/iotn2313.h"

#include "sim_core_declare.h"

/*
 * This is a template for all of the tinyx5 devices, hopefully
 */
static const struct mcu_t {
	avr_t core;
	avr_eeprom_t 	eeprom;
	avr_watchdog_t	watchdog;
	avr_extint_t	extint;
	avr_ioport_t	porta, portb, portd;
	avr_uart_t		uart;
	avr_timer_t		timer0,timer1;
	avr_acomp_t		acomp;
	avr_usi_t	    usi;
} mcu = {
	.core = {
		.mmcu = "attiny2313",
		DEFAULT_CORE(2),

		.init = init,
		.reset = reset,
	},
	AVR_EEPROM_DECLARE_8BIT(EEPROM_READY_vect),
	AVR_WATCHDOG_DECLARE(WDTCSR, WDT_OVERFLOW_vect),
	.extint = {
		AVR_EXTINT_TINY_DECLARE(0, 'D', 2, EIFR),
		AVR_EXTINT_TINY_DECLARE(1, 'D', 3, EIFR),
	},
	AVR_IOPORT_DECLARE(a, 'A', A), // port A has no PCInts..
	.portb = {
		.name = 'B',  .r_port = PORTB, .r_ddr = DDRB, .r_pin = PINB,
		.pcint = {
			.enable = AVR_IO_REGBIT(GIMSK, PCIE),
			.raised = AVR_IO_REGBIT(EIFR, PCIF),
			.vector = PCINT_vect,
		},
		.r_pcint = PCMSK,
	},
	AVR_IOPORT_DECLARE(d, 'D', D), // port D has no PCInts..

	//no PRUSART, upe=UPE, no reg/bit name index, no 'C' in RX/TX vector names
	AVR_UART_DECLARE(0, 0, UPE, , ),

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
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */, AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE },
		.ext_clock_pin = AVR_IO_REGBIT(PORTD, 4), /* External clock pin */

		.r_tcnt = TCNT0,

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE0),
			.raised = AVR_IO_REGBIT(TIFR, TOV0),
			.vector = TIMER0_OVF_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR0A,
				.com = AVR_IO_REGBITS(TCCR0A, COM0A0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTB, 2),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE0A),
					.raised = AVR_IO_REGBIT(TIFR, OCF0A),
					.vector = TIMER0_COMPA_vect,
				},
			},
			[AVR_TIMER_COMPB] = {
				.r_ocr = OCR0B,
				.com = AVR_IO_REGBITS(TCCR0A, COM0B0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTD, 5),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE0B),
					.raised = AVR_IO_REGBIT(TIFR, OCF0B),
					.vector = TIMER0_COMPB_vect,
				}
			}
		},
	},
	.timer1 = {
		.name = '1',
	//	.disabled = AVR_IO_REGBIT(PRR,PRTIM1),
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
		.ext_clock_pin = AVR_IO_REGBIT(PORTD, 5), /* External clock pin */

		.r_tcnt = TCNT1L,
		.r_icr = ICR1L,
		.r_icrh = ICR1H,
		.r_tcnth = TCNT1H,

		.ices = AVR_IO_REGBIT(TCCR1B, ICES1),
		.icp = AVR_IO_REGBIT(PORTD, 6),

		.overflow = {
			.enable = AVR_IO_REGBIT(TIMSK, TOIE1),
			.raised = AVR_IO_REGBIT(TIFR, TOV1),
			.vector = TIMER1_OVF_vect,
		},
		.icr = {
			.enable = AVR_IO_REGBIT(TIMSK, ICIE1),
			.raised = AVR_IO_REGBIT(TIFR, ICF1),
			.vector = TIMER1_CAPT_vect,
		},
		.comp = {
			[AVR_TIMER_COMPA] = {
				.r_ocr = OCR1AL,
				.r_ocrh = OCR1AH,	// 16 bits timers have two bytes of it
				.com = AVR_IO_REGBITS(TCCR1A, COM1A0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTB, 3),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE1A),
					.raised = AVR_IO_REGBIT(TIFR, OCF1A),
					.vector = TIMER1_COMPA_vect,
				},
			},
			[AVR_TIMER_COMPB] = {
				.r_ocr = OCR1BL,
				.r_ocrh = OCR1BH,
				.com = AVR_IO_REGBITS(TCCR1A, COM1A0, 0x3),
				.com_pin = AVR_IO_REGBIT(PORTB, 4),
				.interrupt = {
					.enable = AVR_IO_REGBIT(TIMSK, OCIE1B),
					.raised = AVR_IO_REGBIT(TIFR, OCF1B),
					.vector = TIMER1_COMPB_vect,
				}
			}
		}
	},

	.acomp = {
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
	.usi = {
		.r_usicr = USICR,
		.r_usisr = USISR,
		.r_usidr = USIDR,


		.usipf =  AVR_IO_REGBIT (USISR, USIPF),
		.usidc =  AVR_IO_REGBIT (USISR, USIDC),
		.usiwm =  AVR_IO_REGBITS(USICR, USIWM0, 0x3),
		.usics =  AVR_IO_REGBITS(USICR, USICS0, 0x3),
		.usiclk = AVR_IO_REGBIT (USICR, USICLK),
		.usitc =  AVR_IO_REGBIT (USICR, USITC),

		.port_ioctl = AVR_IOCTL_IOPORT_GETIRQ('B'),
		.pin_di =     AVR_IO_REGBIT(PORTB, 5),
		.pin_do =     AVR_IO_REGBIT(PORTB, 6),
		.pin_usck =   AVR_IO_REGBIT(PORTB, 7),

		.usi_start = {
			.enable = AVR_IO_REGBIT(USICR, USISIE),
			.raised = AVR_IO_REGBIT(USISR, USISIF),
			.vector = USI_START_vect,
			.raise_sticky = 1,
		},

		.usi_ovf = {
			.enable = AVR_IO_REGBIT(USICR, USIOIE),
			.raised = AVR_IO_REGBIT(USISR, USIOIF),
			.vector = USI_OVERFLOW_vect,
			.raise_sticky = 1,
		},
	}
};

static avr_t * make()
{
	return avr_core_allocate(&mcu.core, sizeof(struct mcu_t));
}

avr_kind_t tiny2313 = {
	.names = { "attiny2313", "attiny2313v" },
	.make = make
};

static void init(struct avr_t * avr)
{
	struct mcu_t * mcu = (struct mcu_t*)avr;

	avr_eeprom_init(avr, &mcu->eeprom);
	avr_watchdog_init(avr, &mcu->watchdog);
	avr_extint_init(avr, &mcu->extint);
	avr_ioport_init(avr, &mcu->porta);
	avr_ioport_init(avr, &mcu->portb);
	avr_ioport_init(avr, &mcu->portd);
	avr_uart_init(avr, &mcu->uart);
	avr_timer_init(avr, &mcu->timer0);
	avr_timer_init(avr, &mcu->timer1);
	avr_acomp_init(avr, &mcu->acomp);
	avr_usi_init(avr, &mcu->usi);
}

static void reset(struct avr_t * avr)
{
//	struct mcu_t * mcu = (struct mcu_t*)avr;
}

