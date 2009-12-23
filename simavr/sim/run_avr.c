/*
	run_avr.c

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

#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "sim_hex.h"

extern avr_kind_t * avr_kind[];

void display_usage(char * app)
{
	printf("usage: %s [-t] [-g] [-m <device>] [-f <frequency>] firmware\n", app);
	printf("       -t: run full scale decoder trace\n");
	printf("       -g: listen for gdb connection on port 1234\n");
	printf("   Supported AVR cores:\n");
	for (int i = 0; avr_kind[i]; i++) {
		printf("       ");
		for (int ti = 0; ti < 4 && avr_kind[i]->names[ti]; ti++)
			printf("%s ", avr_kind[i]->names[ti]);
		printf("\n");
	}
	exit(1);
}

int main(int argc, char *argv[])
{
	elf_firmware_t f = {0};
	long f_cpu = 0;
	int trace = 0;
	int gdb = 0;
	char name[16] = "";

	if (argc == 1)
		display_usage(basename(argv[0]));

	for (int pi = 1; pi < argc; pi++) {
		if (!strcmp(argv[pi], "-h") || !strcmp(argv[pi], "-help")) {
				display_usage(basename(argv[0]));
		} else if (!strcmp(argv[pi], "-m") || !strcmp(argv[pi], "-mcu")) {
			if (pi < argc-1)
				strcpy(name, argv[++pi]);
			else
				display_usage(basename(argv[0]));
		} else if (!strcmp(argv[pi], "-f") || !strcmp(argv[pi], "-freq")) {
			if (pi < argc-1)
				f_cpu = atoi(argv[++pi]);
			else
				display_usage(basename(argv[0]));
				break;
		} else if (!strcmp(argv[pi], "-t") || !strcmp(argv[pi], "-trace")) {
				trace++;
		} else if (!strcmp(argv[pi], "-g") || !strcmp(argv[pi], "-gdb")) {
			gdb++;
		}
	}

	char * filename = argv[argc-1];
	char * suffix = strrchr(filename, '.');
	if (suffix && !strcasecmp(suffix, ".hex")) {
		if (!name[0] || !f_cpu) {
			fprintf(stderr, "%s: -mcu and -freq are mandatory to load .hex files\n", argv[0]);
			exit(1);
		}
		f.flash = read_ihex_file(filename, &f.flashsize, &f.flashbase);
	} else {
		elf_read_firmware(filename, &f);
	}

	if (strlen(name))
		strcpy(f.mmcu, name);
	if (f_cpu)
		f.frequency = f_cpu;

	printf("firmware %s f=%d mmcu=%s\n", argv[argc-1], (int)f.frequency, f.mmcu);

	avr_t * avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' now known\n", argv[0], f.mmcu);
		exit(1);
	}
	avr_init(avr);
	avr_load_firmware(avr, &f);
	if (f.flashbase) {
		printf("Attempted to load a booloader at %04x\n", f.flashbase);
		avr->pc = f.flashbase;
	}
	avr->trace = trace;

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (gdb) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	for (;;)
		avr_run(avr);
	
	avr_terminate(avr);
}
