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

#ifndef __AVR_TWI_H__
#define __AVR_TWI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_avr.h"

//#include "sim_twi.h"

enum {
	TWI_IRQ_INPUT = 0,
	TWI_IRQ_OUTPUT,
	TWI_IRQ_STATUS,
	TWI_IRQ_COUNT
};

enum {
	TWI_COND_START = (1 << 0),
	TWI_COND_STOP = (1 << 1),
	TWI_COND_ADDR = (1 << 2),
	TWI_COND_ACK = (1 << 3),
	TWI_COND_WRITE = (1 << 4),
	TWI_COND_READ = (1 << 5),
	// internal state, do not use in irq messages
	TWI_COND_SLAVE	= (1 << 6),
};

typedef struct avr_twi_msg_t {
	uint32_t unused : 8,
		msg : 8,
		addr : 8,
		data : 8;
} avr_twi_msg_t;

typedef struct avr_twi_msg_irq_t {
	union {
		uint32_t v;
		avr_twi_msg_t twi;
	} u;
} avr_twi_msg_irq_t;

// add port number to get the real IRQ
#define AVR_IOCTL_TWI_GETIRQ(_name) AVR_IOCTL_DEF('t','w','i',(_name))

typedef struct avr_twi_t {
	avr_io_t	io;
	char name;

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
	
	avr_int_vector_t twi;	// twi interrupt

	uint8_t state;
	uint8_t peer_addr;
	uint8_t next_twstate;
} avr_twi_t;

void
avr_twi_init(
		avr_t * avr,
		avr_twi_t * port);

/*
 * Create a message value for twi including the 'msg' bitfield,
 * 'addr' and data. This value is what is sent as the IRQ value
 */
uint32_t
avr_twi_irq_msg(
		uint8_t msg,
		uint8_t addr,
		uint8_t data);

#ifdef __cplusplus
};
#endif

#endif /*__AVR_TWI_H__*/
