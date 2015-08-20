/*
	avr_spi.h

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

#ifndef __AVR_SPI_H__
#define __AVR_SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_avr.h"

enum {
	SPI_IRQ_INPUT = 0,
	SPI_IRQ_OUTPUT,
	SPI_IRQ_COUNT
};

// add port number to get the real IRQ
#define AVR_IOCTL_SPI_GETIRQ(_name) AVR_IOCTL_DEF('s','p','i',(_name))

typedef struct avr_spi_t {
	avr_io_t	io;
	char name;
	avr_regbit_t	disabled;	// bit in the PRR

	avr_io_addr_t	r_spdr;			// data register
	avr_io_addr_t	r_spcr;			// control register
	avr_io_addr_t	r_spsr;			// status register
	
	avr_regbit_t spe;		// spi enable
	avr_regbit_t mstr;		// master/slave
	avr_regbit_t spr[4];	// clock divider
	
	avr_int_vector_t spi;	// spi interrupt

	uint8_t		input_data_register;
} avr_spi_t;

void avr_spi_init(avr_t * avr, avr_spi_t * port);

/*
    A few devices have two SPI ports (eg: mega324a)
    Assume user only uses port 1 until dual SPI support is fully implemented
*/

#ifdef SPIF0
#ifndef SPIF
#define SPIF SPIF0
#endif
#endif

#ifdef SPIE0
#ifndef SPIE
#define SPIE SPIE0
#endif
#endif

#ifdef MSTR0
#ifndef MSTR
#define MSTR MSTR0
#endif
#endif

#ifdef SPCR0
#ifndef SPCR
#define SPCR SPCR0
#endif
#endif

#ifdef SPSR0
#ifndef SPSR
#define SPSR SPSR0
#endif
#endif

#ifdef SPI2X0
#ifndef SPI2X
#define SPI2X SPI2X0
#endif
#endif

#ifdef SPR00
#ifndef SPR0
#define SPR0 SPR00
#endif
#endif

#ifdef SPR10
#ifndef SPR1
#define SPR1 SPR10
#endif
#endif

#ifdef SPE0
#ifndef SPE
#define SPE SPE0
#endif
#endif

#ifdef SPDR0
#ifndef SPDR
#define SPDR SPDR0
#endif
#endif

#define AVR_SPI_DECLARE(_prr, _prspi) \
	.spi = { \
		.disabled = AVR_IO_REGBIT(_prr, _prspi), \
	\
		.r_spdr = SPDR, \
		.r_spcr = SPCR, \
		.r_spsr = SPSR, \
	\
		.spe = AVR_IO_REGBIT(SPCR, SPE), \
		.mstr = AVR_IO_REGBIT(SPCR, MSTR), \
	\
		.spr = { AVR_IO_REGBIT(SPCR, SPR0), AVR_IO_REGBIT(SPCR, SPR1), AVR_IO_REGBIT(SPSR, SPI2X) }, \
		.spi = { \
			.enable = AVR_IO_REGBIT(SPCR, SPIE), \
			.raised = AVR_IO_REGBIT(SPSR, SPIF), \
			.vector = SPI_STC_vect, \
		}, \
	}

#ifdef __cplusplus
};
#endif

#endif /*__AVR_SPI_H__*/
