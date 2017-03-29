/*
	sim_prepare.c

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <signal.h>

#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "sim_hex.h"
#include "sim_vcd_file.h"

#include "sim_core_decl.h"
#include "sim_args.h"

static avr_t * avr = NULL;
static avr_vcd_t input;

static void
sig_int(
		int sign)
{
	printf("signal caught, simavr terminating\n");
	if (avr)
		avr_terminate(avr);
	exit(0);
}

extern char * __progname;

avr_t *
sim_prepare(
	sim_args_t * a )
{
//	avr_t * avr;

	avr = avr_make_mcu_by_name(a->f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' not known\n", __progname, a->f.mmcu);
		return NULL;
	}
	avr_init(avr);
	avr_load_firmware(avr, &a->f);
	if (a->f.flashbase) {
		printf("Attempted to load a bootloader at %04x\n", a->f.flashbase);
		avr->pc = a->f.flashbase;
	}
	avr->log = a->log > LOG_TRACE ? LOG_TRACE : a->log;
	avr->trace = a->trace;
	for (int ti = 0; ti < a->trace_vectors_count; ti++) {
		for (int vi = 0; vi < avr->interrupts.vector_count; vi++)
			if (avr->interrupts.vector[vi]->vector == a->trace_vectors[ti])
				avr->interrupts.vector[vi]->trace = 1;
	}
	if (a->vcd_input[0]) {
		if (avr_vcd_init_input(avr, a->vcd_input, &input)) {
			fprintf(stderr, "%s: Warning: VCD input file %s failed\n",
					__progname, a->vcd_input);
		}
	}

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (a->gdb) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	signal(SIGINT, sig_int);
	signal(SIGTERM, sig_int);
	return avr;
}
