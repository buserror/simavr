/*
	avr_usi.h

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

#ifndef __AVR_USI_H__
#define __AVR_USI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_avr.h"

enum {
	USI_IRQ_DO,
	USI_IRQ_DI,
	USI_IRQ_USCK,
	USI_IRQ_TIM0_COMP,
	USI_IRQ_COUNT
};

enum {
	USI_WM_OFF          = 0,
	USI_WM_THREEWIRE    = 1,
	USI_WM_TWOWIRE      = 2,
	USI_WM_TWOWIRE_HOLD = 3,
};

enum {
	USI_CS_SOFTWARE     = 0,
	USI_CS_TIM0         = 1,
	USI_CS_EXT_POS      = 2,
	USI_CS_EXT_NEG      = 3,
};

#define AVR_USI_COUNTER_MAX 15

#define AVR_IOCTL_USI_GETIRQ() AVR_IOCTL_DEF('u','s','i',' ')

typedef struct avr_usi_t {
	avr_io_t	io;

	avr_io_addr_t	r_usicr;	// control register
	avr_io_addr_t	r_usisr;	// status register
	avr_io_addr_t	r_usidr;	// data register
	avr_io_addr_t	r_usibr;	// buffered data register

	avr_regbit_t 	usipf;		// stop condition flag
	avr_regbit_t 	usidc;		// data collision flag
	avr_regbit_t 	usiwm;		// wire mode
	avr_regbit_t 	usics;		// clock source
	avr_regbit_t 	usiclk;		// clock strobe
	avr_regbit_t 	usitc;		// clock toggle

	uint32_t		port_ioctl;	// ioctl to use to get port
	avr_regbit_t	pin_di;		// data in pin
	avr_regbit_t	pin_do;		// data out pin
	avr_regbit_t	pin_usck;	// clock pin

	avr_int_vector_t usi_start;	// start condition interrupt
	avr_int_vector_t usi_ovf;	// overflow interrupt

	uint8_t			in_bit0;   // DI pin input value to be clocked into USIDR bit 0
	uint8_t			in_usck;   // the last value that USCK was set to
} avr_usi_t;

void avr_usi_init(avr_t * avr, avr_usi_t * port);

#define AVR_USI_DECLARE(_portname, _portreg, _pin_di, _pin_do, _pin_usck) \
	.usi = { \
		.r_usicr = USICR, \
		.r_usisr = USISR, \
		.r_usidr = USIDR, \
		.r_usibr = USIBR, \
		\
		.usipf =  AVR_IO_REGBIT (USISR, USIPF), \
		.usidc =  AVR_IO_REGBIT (USISR, USIDC), \
		.usiwm =  AVR_IO_REGBITS(USICR, USIWM0, 0x3), \
		.usics =  AVR_IO_REGBITS(USICR, USICS0, 0x3), \
		.usiclk = AVR_IO_REGBIT (USICR, USICLK), \
		.usitc =  AVR_IO_REGBIT (USICR, USITC), \
		\
		.port_ioctl = AVR_IOCTL_IOPORT_GETIRQ(_portname), \
		.pin_di =     AVR_IO_REGBIT(_portreg, _pin_di), \
		.pin_do =     AVR_IO_REGBIT(_portreg, _pin_do), \
		.pin_usck =   AVR_IO_REGBIT(_portreg, _pin_usck), \
		\
		.usi_start = { \
			.enable = AVR_IO_REGBIT(USICR, USISIE), \
			.raised = AVR_IO_REGBIT(USISR, USISIF), \
			.vector = USI_START_vect, \
			.raise_sticky = 1, \
		}, \
		\
		.usi_ovf = { \
			.enable = AVR_IO_REGBIT(USICR, USIOIE), \
			.raised = AVR_IO_REGBIT(USISR, USIOIF), \
			.vector = USI_OVF_vect, \
			.raise_sticky = 1, \
		}, \
	}

#ifdef __cplusplus
};
#endif

#endif /*__AVR_USI_H__*/
