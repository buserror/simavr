/*
	sim_args.h

	Copyright 2017 Michel Pollet <buserror@gmail.com>

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

#ifndef __SIM_ARGS_H__
#define __SIM_ARGS_H__

#include <stdint.h>

struct sim_args_t;

typedef int (*sim_args_parse_t)(
	struct sim_args_t *a,
	int argc,
	const char *argv[],
	int index );

typedef struct sim_args_t {
	elf_firmware_t f;	// ELF firmware
	uint32_t f_cpu;	// AVR core frequency
	char name[24];	// AVR core name
	uint32_t trace : 4, gdb : 1,  log : 4;


	uint8_t trace_vectors[8];
	int trace_vectors_count;

	char vcd_input[1024];
	char flash_file[1024];
} sim_args_t;

int
sim_args_parse(
	sim_args_t *a,
	int argc,
	const char *argv[],
	sim_args_parse_t passthru);

void
sim_args_display_usage(
	const char * app);
void
sim_args_list_cores();


#endif /* __SIM_ARGS_H__ */
