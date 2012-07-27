/*
	sim_avr_types.h

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

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


#ifndef __SIM_AVR_TYPES_H___
#define __SIM_AVR_TYPES_H___

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <inttypes.h>

typedef uint64_t	avr_cycle_count_t;
typedef uint16_t	avr_io_addr_t;

/*
 * this 'structure' is a packed representation of an IO register 'bit'
 * (or consecutive bits). This allows a way to set/get/clear them.
 * gcc is happy passing these as register value, so you don't need to
 * use a pointer when passing them along to functions.
 *
 * 9 bits ought to be enough, as it's the maximum I've seen (atmega2560)
 */
typedef struct avr_regbit_t {
	uint32_t reg : 9, bit : 3, mask : 8;
} avr_regbit_t;

// printf() conversion specifier for avr_cycle_count_t
#define PRI_avr_cycle_count PRIu64

struct avr_t;

#ifdef __cplusplus
};
#endif

#endif /* __SIM_AVR_TYPES_H___ */
