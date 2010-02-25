/*
	avr_twi.h

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

#ifndef AVR_TWI_H_
#define AVR_TWI_H_

#include "sim_avr.h"

#include "sim_twi.h"

enum {
	TWI_IRQ_INPUT = 0,
	TWI_IRQ_OUTPUT,
	TWI_IRQ_COUNT
};


// add port number to get the real IRQ
#define AVR_IOCTL_TWI_GETIRQ(_name) AVR_IOCTL_DEF('t','w','i',(_name))
// return a pointer to the slave structure related to this TWI port
#define AVR_IOCTL_TWI_GETSLAVE(_name) AVR_IOCTL_DEF('t','w','s',(_name))
// retutn this twi interface "master" bus
#define AVR_IOCTL_TWI_GETBUS(_name) AVR_IOCTL_DEF('t','w','b',(_name))

typedef struct avr_twi_t {
	avr_io_t	io;
	char name;
	
	twi_slave_t 	slave;		// when we are a slave, to be attached to some bus
	twi_bus_t 		bus;		// when we are a master, to attach slaves to
	
	avr_regbit_t	disabled;	// bit in the PRR

	avr_io_addr_t	r_twbr;			// bit rate register
	avr_io_addr_t	r_twcr;			// control register
	avr_io_addr_t	r_twsr;			// status register
	avr_io_addr_t	r_twar;			// address register (slave)
	avr_io_addr_t	r_twamr;		// address mask register
	avr_io_addr_t	r_twdr;			// data register
	
	avr_regbit_t twen;		// twi enable bit
	avr_regbit_t twea;		// enable acknowledge bit
	avr_regbit_t twsta;		// start condition
	avr_regbit_t twsto;		// stop condition
	avr_regbit_t twwc;		// write collision
	
	avr_regbit_t twsr;		// status registers, (5 bits)
	avr_regbit_t twps;		// prescaler bits (2 bits)
	
	avr_int_vector_t twi;	// spi interrupt
} avr_twi_t;

void avr_twi_init(avr_t * avr, avr_twi_t * port);

#endif /* AVR_TWI_H_ */
