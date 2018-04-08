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
#include "avr_bitbang.h"

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

	avr_io_addr_t	r_spdr;		// data register
	avr_io_addr_t	r_spcr;		// control register
	avr_io_addr_t	r_spsr;		// status register
	
	avr_regbit_t	spe;		// spi enable
	avr_regbit_t	dord;		// if zero, the MSB of the data word is transmitted first
	avr_regbit_t	mstr;		// master/slave
	avr_regbit_t	cpol;		// if zero, SCK is low when idle
	avr_regbit_t	cpha;		// if zero, sampling on the SCK leading edge
	avr_regbit_t	spr[4];		// clock divider

	avr_iopin_t		p_miso;		// data in/out pin
	avr_iopin_t		p_mosi;		// data in/out pin
	avr_bitbang_t	bit_bang;
	avr_iopin_t		p_ss;		// slave select pin

	avr_int_vector_t spi;		// spi interrupt

	uint8_t		input_data_register;
	uint8_t		output_data_register;
} avr_spi_t;

void avr_spi_init(avr_t * avr, avr_spi_t * port);

#define AVR_SPIX_DECLARE(_name, _prr, _prspi, _ss_pin_port, _p_sck, _p_miso, _p_mosi, _p_ss) \
	.spi = { \
		.name = '0' + _name,\
		.disabled = AVR_IO_REGBIT(_prr, _prspi), \
	\
		.r_spdr = SPDR ## _name, \
		.r_spcr = SPCR ## _name, \
		.r_spsr = SPSR ## _name, \
	\
		.spe = AVR_IO_REGBIT(SPCR ## _name, SPE ## _name), \
		.dord = AVR_IO_REGBIT(SPCR ## _name, DORD ## _name), \
		.mstr = AVR_IO_REGBIT(SPCR ## _name, MSTR ## _name), \
		.cpol = AVR_IO_REGBIT(SPCR ## _name, CPOL ## _name), \
		.cpha = AVR_IO_REGBIT(SPCR ## _name, CPHA ## _name), \
	\
		.bit_bang.p_clk = AVR_IOPIN('B', _p_sck), \
		.p_miso = AVR_IOPIN('B', _p_miso), \
		.p_mosi = AVR_IOPIN('B', _p_mosi), \
		.p_ss = AVR_IOPIN(_ss_pin_port, _p_ss), \
	\
		.spr = { AVR_IO_REGBIT(SPCR ## _name, SPR0 ## _name), \
					AVR_IO_REGBIT(SPCR ## _name, SPR1 ## _name), \
					AVR_IO_REGBIT(SPSR ## _name, SPI2X ## _name) }, \
		.spi = { \
			.enable = AVR_IO_REGBIT(SPCR ## _name, SPIE ## _name), \
			.raised = AVR_IO_REGBIT(SPSR ## _name, SPIF ## _name), \
			.vector = SPI_STC_vect, \
		}, \
	}

#define AVR_SPI_DECLARE(_prr, _prspi, _ss_pin_port, _p_sck, _p_miso, _p_mosi, _p_ss) \
	.spi = { \
		.disabled = AVR_IO_REGBIT(_prr, _prspi), \
	\
		.r_spdr = SPDR, \
		.r_spcr = SPCR, \
		.r_spsr = SPSR, \
	\
		.spe = AVR_IO_REGBIT(SPCR, SPE), \
		.dord = AVR_IO_REGBIT(SPCR, DORD), \
		.mstr = AVR_IO_REGBIT(SPCR, MSTR), \
		.cpol = AVR_IO_REGBIT(SPCR, CPOL), \
		.cpha = AVR_IO_REGBIT(SPCR, CPHA), \
	\
		.bit_bang.p_clk = AVR_IOPIN('B', _p_sck), \
		.p_miso = AVR_IOPIN('B', _p_miso), \
		.p_mosi = AVR_IOPIN('B', _p_mosi), \
		.p_ss = AVR_IOPIN(_ss_pin_port, _p_ss), \
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
