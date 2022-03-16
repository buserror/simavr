/*
	sim_gdb.h

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

#ifndef __SIM_GDB_H__
#define __SIM_GDB_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Watchpoint types.
   See GDB User Manual, Appendix E.2 */
enum avr_gdb_watch_type {
	AVR_GDB_BREAK_SOFT   = 1 << 0,
	AVR_GDB_BREAK_HARD   = 1 << 1,

	AVR_GDB_WATCH_WRITE  = 1 << 2,
	AVR_GDB_WATCH_READ   = 1 << 3,
	AVR_GDB_WATCH_ACCESS = 1 << 4
};

int avr_gdb_init(avr_t * avr);

void avr_deinit_gdb(avr_t * avr);

// call from the main AVR decoder thread
int avr_gdb_processor(avr_t * avr, int sleep);

// Called from sim_core.c
void avr_gdb_handle_watchpoints(avr_t * g, uint16_t addr, enum avr_gdb_watch_type type);

#ifdef __cplusplus
};
#endif

#endif
