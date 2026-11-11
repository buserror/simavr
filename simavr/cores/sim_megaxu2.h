/*
	sim_megaxu2.h

	Template for the AT90USB82/162 and ATmega8U2/16U2/32U2, which have the
	same USB controller. The ATmega8U2/16U2 are an improved version of the
	AT90USB82/162 (Atmel AVR530, "Migrating from AT90USB162/82 to
	ATmega16U2/8U2"). Besides electrical characteristics they add two
	registers that were reserved on the AT90USB parts, neither of which
	simavr models on either:
	- WDTCKD (0x62), watchdog clock divider and early warning interrupt
	- ACMUX (0x7D), selects AIN1..AIN6 as the comparator's negative input;
	  here it is always AIN1, as on the AT90USB parts
	so the cores built from this template only differ in memory sizes.
	Derived from sim_usb162.c, Copyright 2012 Torbjorn Tyridal <ttyridal@gmail.com>

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

#ifndef __SIM_MEGAXU2_H__
#define __SIM_MEGAXU2_H__

#include "sim_avr.h"
#include "sim_core_declare.h"
#include "avr_eeprom.h"
#include "avr_flash.h"
#include "avr_watchdog.h"
#include "avr_extint.h"
#include "avr_ioport.h"
#include "avr_uart.h"
#include "avr_timer.h"
#include "avr_spi.h"
#include "avr_usb.h"
#include "avr_acomp.h"

void mxu2_init(struct avr_t * avr);
void mxu2_reset(struct avr_t * avr);

struct mcu_t {
	avr_t			core;
	avr_ioport_t	portb, portc, portd;
	avr_eeprom_t 	eeprom;
	avr_flash_t 	selfprog;
	avr_watchdog_t	watchdog;
	avr_extint_t	extint;
	avr_uart_t		uart1;
	avr_timer_t		timer0,timer1;
	avr_spi_t		spi;
	avr_usb_t		usb;
	avr_acomp_t		acomp;
};

#ifdef SIM_CORENAME

#ifndef SIM_MMCU
#error SIM_MMCU is not declared
#endif

const struct mcu_t SIM_CORENAME = {
	.core = {
		.mmcu = SIM_MMCU,
		DEFAULT_CORE(4),

		.init = mxu2_init,
		.reset = mxu2_reset,
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
	AVR_IOPORT_DECLARE(d, 'D', D),

	AVR_EEPROM_DECLARE(EE_READY_vect),
	AVR_SELFPROG_DECLARE(SPMCSR, SPMEN, SPM_READY_vect),
	AVR_WATCHDOG_DECLARE(WDTCSR, WDT_vect),
	.extint = {
		AVR_EXTINT_MEGA_DECLARE(0, 'D', PIND0, A),
		AVR_EXTINT_MEGA_DECLARE(1, 'D', PIND1, A),
		AVR_EXTINT_MEGA_DECLARE(2, 'D', PIND2, A),
		AVR_EXTINT_MEGA_DECLARE(3, 'D', PIND3, A),
		AVR_EXTINT_MEGA_DECLARE(4, 'C', PINC7, B),
		AVR_EXTINT_MEGA_DECLARE(5, 'D', PIND4, B),
		AVR_EXTINT_MEGA_DECLARE(6, 'D', PIND6, B),
		AVR_EXTINT_MEGA_DECLARE(7, 'D', PIND7, B),
	},
	AVR_UARTX_DECLARE(1, PRR1, PRUSART1),

	.timer0 = {
		.name = '0',
		.disabled = AVR_IO_REGBIT(PRR0,PRTIM0),
		.wgm = { AVR_IO_REGBIT(TCCR0A, WGM00), AVR_IO_REGBIT(TCCR0A, WGM01), AVR_IO_REGBIT(TCCR0B, WGM02) },
		WGM_OPS_8,
		.cs = { AVR_IO_REGBIT(TCCR0B, CS00), AVR_IO_REGBIT(TCCR0B, CS01), AVR_IO_REGBIT(TCCR0B, CS02) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */, AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE },
		.ext_clock_pin = AVR_IO_REGBIT(PORTD, 7), /* External clock pin */

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
		WGM_OPS_16,
		.cs = { AVR_IO_REGBIT(TCCR1B, CS10), AVR_IO_REGBIT(TCCR1B, CS11), AVR_IO_REGBIT(TCCR1B, CS12) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */, AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE },
		.ext_clock_pin = AVR_IO_REGBIT(PORTB, 4), /* External clock pin */

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
	AVR_SPI_DECLARE(PRR0, PRSPI),
	.usb = {
		.name='1',
		.disabled=AVR_IO_REGBIT(PRR1, PRUSB),// bit in the PRR

		.usbrf=AVR_IO_REGBIT(MCUSR,USBRF),	// bit in the MCUSR

		.r_usbcon = USBCON,
		.r_pllcsr=PLLCSR,

		.usb_com_vect=USB_COM_vect,
		.usb_gen_vect=USB_GEN_vect,

		// 176 bytes DPRAM, EP0 64, EP1-2 64 single bank, EP3-4 64 dual bank
		.ep_count = 5,
		.dpram_size = 176,
		.ep_size_max = { 64, 64, 64, 64, 64 },
		.ep_banks_max = { 1, 1, 1, 2, 2 },
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
			.vector = ANALOG_COMP_vect,
		}
	}
};

#endif /* SIM_CORENAME */

#endif /* __SIM_MEGAXU2_H__ */
