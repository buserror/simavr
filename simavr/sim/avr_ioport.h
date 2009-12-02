/*
	avr_ioport.h

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

#ifndef __AVR_IOPORT_H__
#define __AVR_IOPORT_H__

#include "sim_avr.h"

enum {
	IOPORT_IRQ_PIN0 = 0,
	IOPORT_IRQ_PIN1,IOPORT_IRQ_PIN2,IOPORT_IRQ_PIN3,IOPORT_IRQ_PIN4,
	IOPORT_IRQ_PIN5,IOPORT_IRQ_PIN6,IOPORT_IRQ_PIN7,
	IOPORT_IRQ_PIN_ALL,
	IOPORT_IRQ_COUNT
};

// add port name (uppercase) to get the real IRQ
#define AVR_IOCTL_IOPORT_GETIRQ(_name) AVR_IOCTL_DEF('i','o','g',(_name))

typedef struct avr_ioport_t {
	avr_io_t	io;
	char name;
	uint8_t r_port;
	uint8_t r_ddr;
	uint8_t r_pin;

	avr_int_vector_t pcint;	// PCINT vector
	uint8_t r_pcint;		// pcint 8 pins mask
} avr_ioport_t;

void avr_ioport_init(avr_t * avr, avr_ioport_t * port);


#endif /* __AVR_IOPORT_H__ */
