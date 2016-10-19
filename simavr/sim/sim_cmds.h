/*
	sim_cmds.h

	Copyright 2014 Florian Albrechtskirchinger <falbrechtskirchinger@gmail.com>

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

#pragma once

#include "sim_avr_types.h"

#define MAX_AVR_COMMANDS		32

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*avr_cmd_handler_t)(
		struct avr_t * avr,
		uint8_t v,
		void * param);

typedef struct avr_cmd_t {
	avr_cmd_handler_t handler;
	void * param;
} avr_cmd_t;

typedef struct avr_cmd_table_t {
	avr_cmd_t table[MAX_AVR_COMMANDS];
	avr_cmd_t * pending;	// Holds a reference to a pending multi-byte command
} avr_cmd_table_t;

// Called by avr_set_command_register()
void
avr_cmd_set_register(
		struct avr_t * avr,
		avr_io_addr_t addr);

/*
 * Register a command distinguished by 'code'.
 *
 * When 'code' is written to the configured IO address, 'handler' is executed
 * with the value written, as well as 'param'.
 * 'handler' can return non-zero, to indicate, that this is a multi-byte command.
 * Subsequent writes are then dispatched to the same handler, until 0 is returned.
 */
void
avr_cmd_register(
		struct avr_t * avr,
		uint8_t code,
		avr_cmd_handler_t handler,
		void * param);

void
avr_cmd_unregister(
		struct avr_t * avr,
		uint8_t code);

// Private functions

// Called from avr_init() to initialize the avr_cmd_table_t and register builtin commands.
void
avr_cmd_init(
		struct avr_t * avr);

#ifdef __cplusplus
}
#endif
