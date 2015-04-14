/*
	avr_watchdog.h

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


#ifndef __AVR_WATCHDOG_H___
#define __AVR_WATCHDOG_H___

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_avr.h"

typedef struct avr_watchdog_t {
	avr_io_t	io;

	avr_regbit_t 	wdrf;		// watchdog reset flag (in MCU Status Register)

	avr_regbit_t 	wdce;		// watchdog change enable
	avr_regbit_t 	wde;		// watchdog enabled
	avr_regbit_t 	wdp[4];		// watchdog Timer Prescaler

	avr_int_vector_t watchdog;	// watchdog interrupt

	avr_cycle_count_t	cycle_count;

	struct {
		uint8_t		wdrf;		// saved watchdog reset flag
		avr_run_t	avr_run;	// restored during reset
	} reset_context;
} avr_watchdog_t;

/* takes no parameter */
#define AVR_IOCTL_WATCHDOG_RESET	AVR_IOCTL_DEF('w','d','t','r')

void avr_watchdog_init(avr_t * avr, avr_watchdog_t * p);


/*
 * This helps declare a watchdog block into a core.
 * No guarantee it will work with all, but it works
 * with the one we have right now
 */
#define AVR_WATCHDOG_DECLARE(_WDSR, _vec) \
	.watchdog = {\
		.wdrf = AVR_IO_REGBIT(MCUSR, WDRF),\
		.wdce = AVR_IO_REGBIT(_WDSR, WDCE),\
		.wde = AVR_IO_REGBIT(_WDSR, WDE),\
		.wdp = { AVR_IO_REGBIT(_WDSR, WDP0),AVR_IO_REGBIT(_WDSR, WDP1),\
				AVR_IO_REGBIT(_WDSR, WDP2),AVR_IO_REGBIT(_WDSR, WDP3) },\
		.watchdog = {\
			.enable = AVR_IO_REGBIT(_WDSR, WDIE),\
			.raised = AVR_IO_REGBIT(_WDSR, WDIF),\
			.vector = _vec,\
		},\
	}

/* no WDP3, WDIE, WDIF in atmega128 */
/* MCUSR is called MCUCSR in atmega128 */
#define AVR_WATCHDOG_DECLARE_128(_WDSR, _vec) \
	.watchdog = {\
		.wdrf = AVR_IO_REGBIT(MCUCSR, WDRF),\
		.wdce = AVR_IO_REGBIT(_WDSR, WDCE),\
		.wde = AVR_IO_REGBIT(_WDSR, WDE),\
		.wdp = { AVR_IO_REGBIT(_WDSR, WDP0),AVR_IO_REGBIT(_WDSR, WDP1),\
				AVR_IO_REGBIT(_WDSR, WDP2) },\
		.watchdog = {\
			.enable = AVR_IO_REGBIT(_WDSR, 6),\
			.raised = AVR_IO_REGBIT(_WDSR, 7),\
			.vector = _vec,\
		},\
	}

#ifdef __cplusplus
};
#endif

#endif /* __AVR_WATCHDOG_H___ */
