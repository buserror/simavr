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

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_avr.h"

enum {
	IOPORT_IRQ_PIN0 = 0,
	IOPORT_IRQ_PIN1,IOPORT_IRQ_PIN2,IOPORT_IRQ_PIN3,IOPORT_IRQ_PIN4,
	IOPORT_IRQ_PIN5,IOPORT_IRQ_PIN6,IOPORT_IRQ_PIN7,
	IOPORT_IRQ_PIN_ALL,
	IOPORT_IRQ_DIRECTION_ALL,
	IOPORT_IRQ_REG_PORT,
	IOPORT_IRQ_REG_PIN,
	IOPORT_IRQ_PIN0_SRC_IMP,IOPORT_IRQ_PIN1_SRC_IMP,IOPORT_IRQ_PIN2_SRC_IMP,IOPORT_IRQ_PIN3_SRC_IMP,
	IOPORT_IRQ_PIN4_SRC_IMP,IOPORT_IRQ_PIN5_SRC_IMP,IOPORT_IRQ_PIN6_SRC_IMP,IOPORT_IRQ_PIN7_SRC_IMP,
	IOPORT_IRQ_COUNT
};

#define AVR_IOPORT_OUTPUT 0x100
#define AVR_IOPORT_INTRN_PULLUP_IMP 100000L

// add port name (uppercase) to get the real IRQ
#define AVR_IOCTL_IOPORT_GETIRQ(_name) AVR_IOCTL_DEF('i','o','g',(_name))


// this ioctl takes a avr_regbit_t, compares the register address
// to PORT/PIN/DDR and return the corresponding IRQ(s) if it matches
typedef struct avr_ioport_getirq_t {
	avr_regbit_t bit;	// bit wanted
	avr_irq_t * irq[8];	// result, terminated by NULL if < 8
} avr_ioport_getirq_t;

#define AVR_IOCTL_IOPORT_GETIRQ_REGBIT AVR_IOCTL_DEF('i','o','g','r')

/*
 * ioctl used to get a port state.
 *
 * for (int i = 'A'; i <= 'F'; i++) {
 * 	avr_ioport_state_t state;
 * 	if (avr_ioctl(AVR_IOCTL_IOPORT_GETSTATE(i), &state) == 0)
 * 		printf("PORT%c %02x DDR %02x PIN %02x\n",
 * 			state.name, state.port, state.ddr, state.pin);
 * }
 */
typedef struct avr_ioport_state_t {
	unsigned long name : 7,
		port : 8, ddr : 8, pin : 8;
} avr_ioport_state_t;

// add port name (uppercase) to get the port state
#define AVR_IOCTL_IOPORT_GETSTATE(_name) AVR_IOCTL_DEF('i','o','s',(_name))

/*
 * ioctl used to set default port state when set as input.
 *
 */
typedef struct avr_ioport_external_t {
	unsigned long name : 7,
		mask : 8, value : 8;
} avr_ioport_external_t;

// add port name (uppercase) to set default input pin IRQ values
#define AVR_IOCTL_IOPORT_SET_EXTERNAL(_name) AVR_IOCTL_DEF('i','o','p',(_name))

/**
 * pin structure
 */
typedef struct avr_iopin_t {
	uint16_t port : 8;			///< port e.g. 'B'
	uint16_t pin : 8;		///< pin number
} avr_iopin_t;
#define AVR_IOPIN(_port, _pin)	{ .port = _port, .pin = _pin }

/*
 * Definition for an IO port
 */
typedef struct avr_ioport_t {
	avr_io_t	io;
	char name;
	avr_io_addr_t r_port;
	avr_io_addr_t r_ddr;
	avr_io_addr_t r_pin;

	avr_int_vector_t pcint;	// PCINT vector
	avr_io_addr_t r_pcint;		// pcint 8 pins mask

	// this represent the default IRQ value when
	// the port is set as input.
	// If the mask is not set, no output value is sent
	// on the output IRQ. If the mask is set, the specified
	// value is sent.
	struct {
		uint8_t pull_mask, pull_value;
	} external;
} avr_ioport_t;

void avr_ioport_init(avr_t * avr, avr_ioport_t * port);

#define AVR_IOPORT_DECLARE(_lname, _cname, _uname) \
	.port ## _lname = { \
		.name = _cname, .r_port = PORT ## _uname, .r_ddr = DDR ## _uname, .r_pin = PIN ## _uname, \
	}

#ifdef __cplusplus
};
#endif

#endif /* __AVR_IOPORT_H__ */
