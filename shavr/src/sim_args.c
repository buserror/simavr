/*
	sim_args.c

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

#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "sim_hex.h"
#include "sim_vcd_file.h"

#include "sim_core_decl.h"
#include "sim_args.h"

void
sim_args_display_usage(
	const char * app)
{
	printf("Usage: %s [...] <firmware>\n", app);
	printf( "		[--freq|-f <freq>]  Sets the frequency for an .hex firmware\n"
			"		[--mcu|-m <device>] Sets the MCU type for an .hex firmware\n"
			"       [--list-cores]      List all supported AVR cores and exit\n"
			"       [--help|-h]         Display this usage message and exit\n"
			"       [--trace, -t]       Run full scale decoder trace\n"
			"       [-ti <vector>]      Add traces for IRQ vector <vector>\n"
			"       [--gdb|-g]          Listen for gdb connection on port 1234\n"
			"       [-ff <.hex file>]   Load next .hex file as flash\n"
			"       [-ee <.hex file>]   Load next .hex file as eeprom\n"
			"       [--input|-i <file>] A .vcd file to use as input signals\n"
			"       [--persist|-p <file>] Save/Load flash+eeprom from <file>\n"
			"       [-v]                Raise verbosity level\n"
			"                           (can be passed more than once)\n"
			"       <firmware>          A .hex or an ELF file. ELF files are\n"
			"                           prefered, and can include debugging syms\n");
}

void
sim_args_list_cores()
{
	printf( "Supported AVR cores:\n");
	for (int i = 0; avr_kind[i]; i++) {
		printf("       ");
		for (int ti = 0; ti < 4 && avr_kind[i]->names[ti]; ti++)
			printf("%s ", avr_kind[i]->names[ti]);
		printf("\n");
	}
}


int
sim_args_parse(
	sim_args_t *a,
	int argc,
	const char *argv[],
	sim_args_parse_t passthru)
{
	uint32_t loadBase = AVR_SEGMENT_OFFSET_FLASH;

	if (!a || !argc || !argv)
		return -1;
	memset(a, 0, sizeof(*a));

	const char * progname = basename(strdup(argv[0]));
	if (argc == 1)
		sim_args_display_usage(progname);

	for (int pi = 1; pi < argc; pi++) {
		if (!strcmp(argv[pi], "--list-cores")) {
			sim_args_list_cores();
		} else if (!strcmp(argv[pi], "-h") || !strcmp(argv[pi], "--help")) {
			sim_args_display_usage(progname);
		} else if (!strcmp(argv[pi], "-m") || !strcmp(argv[pi], "--mcu")) {
			if (pi < argc-1)
				strncpy(a->name, argv[++pi], sizeof(a->name));
			else
				sim_args_display_usage(progname);
		} else if (!strcmp(argv[pi], "-f") || !strcmp(argv[pi], "--freq")) {
			if (pi < argc-1)
				a->f_cpu = atoi(argv[++pi]);
			else
				sim_args_display_usage(progname);
		} else if (!strcmp(argv[pi], "-i") || !strcmp(argv[pi], "--input")) {
			if (pi < argc-1)
				strncpy(a->vcd_input, argv[++pi], sizeof(a->vcd_input));
			else
				sim_args_display_usage(progname);
		} else if (!strcmp(argv[pi], "-p") || !strcmp(argv[pi], "--persist")) {
			if (argv[pi + 1] && argv[pi + 1][0] != '-')
				strncpy(a->flash_file, argv[++pi], sizeof(a->flash_file));
			else
				strncpy(a->flash_file, "simavr_flash.bin", sizeof(a->flash_file));
		} else if (!strcmp(argv[pi], "-t") || !strcmp(argv[pi], "--trace")) {
			a->trace++;
		} else if (!strcmp(argv[pi], "-ti")) {
			if (pi < argc-1)
				a->trace_vectors[a->trace_vectors_count++] = atoi(argv[++pi]);
		} else if (!strcmp(argv[pi], "-g") || !strcmp(argv[pi], "--gdb")) {
			a->gdb++;
		} else if (!strcmp(argv[pi], "-v")) {
			a->log++;
		} else if (!strcmp(argv[pi], "-ee")) {
			loadBase = AVR_SEGMENT_OFFSET_EEPROM;
		} else if (!strcmp(argv[pi], "-ff")) {
			loadBase = AVR_SEGMENT_OFFSET_FLASH;
		} else if (argv[pi][0] == '-') {
			// call the passthru callback
			int npi = passthru(a, argc, argv, pi);
			if (npi < 0)
				return npi;
			if (npi > pi)
				pi = npi;
		} else if (argv[pi][0] != '-') {
			const char * filename = argv[pi];
			char * suffix = strrchr(filename, '.');
			if (suffix && !strcasecmp(suffix, ".hex")) {
				if (!a->name[0] || !a->f_cpu) {
					fprintf(stderr,
							"%s: --mcu and --freq are mandatory to load .hex files\n",
							argv[0]);
					return -1;
				}
				ihex_chunk_p chunk = NULL;
				int cnt = read_ihex_chunks(filename, &chunk);
				if (cnt <= 0) {
					fprintf(stderr, "%s: Unable to load IHEX file %s\n",
						argv[0], argv[pi]);
					return -1;
				}
				if (a->log)
					printf("Loaded %d section of ihex\n", cnt);
				for (int ci = 0; ci < cnt; ci++) {
					if (chunk[ci].baseaddr < (1*1024*1024)) {
						a->f.flash = chunk[ci].data;
						a->f.flashsize = chunk[ci].size;
						a->f.flashbase = chunk[ci].baseaddr;
						if (a->log)
							printf("Load HEX flash %08x, %d\n", a->f.flashbase, a->f.flashsize);
					} else if (chunk[ci].baseaddr >= AVR_SEGMENT_OFFSET_EEPROM ||
							chunk[ci].baseaddr + loadBase >= AVR_SEGMENT_OFFSET_EEPROM) {
						// eeprom!
						a->f.eeprom = chunk[ci].data;
						a->f.eesize = chunk[ci].size;
						if (a->log)
							printf("Load HEX eeprom %08x, %d\n", chunk[ci].baseaddr, a->f.eesize);
					}
				}
			} else {
				if (elf_read_firmware(filename, &a->f) == -1) {
					fprintf(stderr, "%s: Unable to load firmware from file %s\n",
							argv[0], filename);
					return -1;
				}
			}
		}
	}

	if (strlen(a->name))
		strcpy(a->f.mmcu, a->name);
	if (a->f_cpu)
		a->f.frequency = a->f_cpu;

	return 0;
}

