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

#ifndef AVR_SPI_H_
#define AVR_SPI_H_

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

#endif /* AVR_SPI_H_ */
