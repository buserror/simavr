/*
	avr_extint.h

	External Interrupt Handling (for INT0-3)

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
	Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>

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

#ifndef __AVR_EXTINT_H__
#define __AVR_EXTINT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_avr.h"


enum {
	EXTINT_IRQ_OUT_INT0 = 0,
	EXTINT_IRQ_OUT_INT1, EXTINT_IRQ_OUT_INT2, EXTINT_IRQ_OUT_INT3,
	EXTINT_IRQ_OUT_INT4, EXTINT_IRQ_OUT_INT5, EXTINT_IRQ_OUT_INT6,
	EXTINT_IRQ_OUT_INT7,
	EXTINT_COUNT
};

// Get the internal IRQ corresponding to the INT
#define AVR_IOCTL_EXTINT_GETIRQ() AVR_IOCTL_DEF('i','n','t',' ')

typedef struct avr_extint_t avr_extint_t;

// This is a context for the cycle timer handling the "level" mode triggering
typedef struct avr_extint_poll_context_t {
	int32_t	eint_no;	// index of particular interrupt source we are monitoring
	int32_t	is_polling; // is this interrupt polling cycle active?
	avr_extint_t *extint;
} avr_extint_poll_context_t;

/*
 * This module is just a "relay" for the pin change IRQ in the IO port
 * module. We hook up to their IRQ and raise out interrupt vectors as needed
 */
typedef struct avr_extint_t {
	avr_io_t	io;

	struct {
		avr_regbit_t	isc[2];		// interrupt sense control bits
		avr_int_vector_t vector;	// interrupt vector

		uint32_t		port_ioctl;		// ioctl to use to get port
		uint8_t			port_pin;		// pin number in said port
		uint8_t			strict_lvl_trig;// enforces a repetitive interrupt triggering while the pin is held low
	}	eint[EXTINT_COUNT];

	avr_extint_poll_context_t poll_contexts[EXTINT_COUNT];

} avr_extint_t;

void avr_extint_init(avr_t * avr, avr_extint_t * p);

int // return -1 if irrelevant extint_no given, strict level triggering flag otherwise.
avr_extint_is_strict_lvl_trig(
		avr_t * avr,
		uint8_t extint_no); //an ext interrupt number, e.g. 0 or 1 (corresponds to INT0 or INT1)
void
avr_extint_set_strict_lvl_trig(
		avr_t * avr,
		uint8_t extint_no, //an ext interrupt number
		uint8_t strict); //new strict level triggering flag


// Declares a typical INT into a avr_extint_t in a core.
// this is a shortcut since INT declarations are pretty standard.
// The Tinies as well as the atmega1280 are slightly different.
// See sim_tinyx5.h and sim_mega1280.h
#define AVR_EXTINT_DECLARE(_index, _portname, _portpin) \
		.eint[_index] = { \
			.port_ioctl = AVR_IOCTL_IOPORT_GETIRQ(_portname), \
			.port_pin = _portpin, \
			.isc = { AVR_IO_REGBIT(EICRA, ISC##_index##0), AVR_IO_REGBIT(EICRA, ISC##_index##1) },\
			.vector = { \
				.enable = AVR_IO_REGBIT(EIMSK, INT##_index), \
				.raised = AVR_IO_REGBIT(EIFR, INTF##_index), \
				.vector = INT##_index##_vect, \
			},\
		}

// Asynchronous External Interrupt, for example INT2 on the m16 and m32
// Uses only 1 interrupt sense control bit
#define AVR_ASYNC_EXTINT_DECLARE(_index, _portname, _portpin) \
		.eint[_index] = { \
			.port_ioctl = AVR_IOCTL_IOPORT_GETIRQ(_portname), \
			.port_pin = _portpin, \
			.isc = { AVR_IO_REGBIT(MCUCSR, ISC##_index) },\
			.vector = { \
				.enable = AVR_IO_REGBIT(GICR, INT##_index), \
				.raised = AVR_IO_REGBIT(GIFR, INTF##_index), \
				.vector = INT##_index##_vect, \
			},\
		}

#define AVR_EXTINT_MEGA_DECLARE(_index, _portname, _portpin, _EICR) \
		.eint[_index] = { \
			.port_ioctl = AVR_IOCTL_IOPORT_GETIRQ(_portname), \
			.port_pin = _portpin, \
			.isc = { AVR_IO_REGBIT(EICR##_EICR, ISC##_index##0), AVR_IO_REGBIT(EICR##_EICR, ISC##_index##1) },\
			.vector = { \
				.enable = AVR_IO_REGBIT(EIMSK, INT##_index), \
				.raised = AVR_IO_REGBIT(EIFR, INTF##_index), \
				.vector = INT##_index##_vect, \
			},\
		}

#define AVR_EXTINT_TINY_DECLARE(_index, _portname, _portpin, _IFR) \
		.eint[_index] = { \
			.port_ioctl = AVR_IOCTL_IOPORT_GETIRQ(_portname), \
			.port_pin = _portpin, \
			.isc = { AVR_IO_REGBIT(MCUCR, ISC##_index##0), AVR_IO_REGBIT(MCUCR, ISC##_index##1) }, \
			.vector = { \
				.enable = AVR_IO_REGBIT(GIMSK, INT##_index), \
				.raised = AVR_IO_REGBIT(_IFR, INTF##_index), \
				.vector = INT##_index##_vect, \
			}, \
		}

#ifdef __cplusplus
};
#endif

#endif /*__AVR_EXTINT_H__*/
