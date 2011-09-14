/*
	run_avr.c

	Copyright 2008, 2010 Michel Pollet <buserror@gmail.com>

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
#include <signal.h>
#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "sim_hex.h"

extern avr_kind_t * avr_kind[];

void display_usage(char * app)
{
	printf("usage: %s [-t] [-g] [-m <device>] [-f <frequency>] firmware\n", app);
	printf("       -t: run full scale decoder trace\n"
		   "       -g: listen for gdb connection on port 1234\n"
		   "       -ff: Loads next .hex file as flash\n"
		   "       -ee: Loads next .hex file as eeprom\n"
		   "   Supported AVR cores:\n");
	for (int i = 0; avr_kind[i]; i++) {
		printf("       ");
		for (int ti = 0; ti < 4 && avr_kind[i]->names[ti]; ti++)
			printf("%s ", avr_kind[i]->names[ti]);
		printf("\n");
	}
	exit(1);
}

avr_t * avr = NULL;

void
sig_int(
		int sign)
{
	printf("signal caught, simavr terminating\n");
	if (avr)
		avr_terminate(avr);
	exit(0);
}

int main(int argc, char *argv[])
{
	elf_firmware_t f = {{0}};
	long f_cpu = 0;
	int trace = 0;
	int gdb = 0;
	char name[16] = "";
	uint32_t loadBase = AVR_SEGMENT_OFFSET_FLASH;
	int trace_vectors[8] = {0};
	int trace_vectors_count = 0;

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
		} else if (!strcmp(argv[pi], "-t") || !strcmp(argv[pi], "-trace")) {
			trace++;
		} else if (!strcmp(argv[pi], "-ti")) {
			if (pi < argc-1)
				trace_vectors[trace_vectors_count++] = atoi(argv[++pi]);
		} else if (!strcmp(argv[pi], "-g") || !strcmp(argv[pi], "-gdb")) {
			gdb++;
		} else if (!strcmp(argv[pi], "-ee")) {
			loadBase = AVR_SEGMENT_OFFSET_EEPROM;
		} else if (!strcmp(argv[pi], "-ff")) {
			loadBase = AVR_SEGMENT_OFFSET_FLASH;			
		} else if (argv[pi][0] != '-') {
			char * filename = argv[pi];
			char * suffix = strrchr(filename, '.');
			if (suffix && !strcasecmp(suffix, ".hex")) {
				if (!name[0] || !f_cpu) {
					fprintf(stderr, "%s: -mcu and -freq are mandatory to load .hex files\n", argv[0]);
					exit(1);
				}
				struct ihex_chunk_t chunk[4];
				int cnt = read_ihex_chunks(filename, chunk, 4);
				if (cnt <= 0) {
					fprintf(stderr, "%s: Unable to load IHEX file %s\n", 
						argv[0], argv[pi]);
					exit(1);
				}
				printf("Loaded %d section of ihex\n", cnt);
				for (int ci = 0; ci < cnt; ci++) {
					if (chunk[ci].baseaddr < (1*1024*1024)) {
						f.flash = chunk[ci].data;
						f.flashsize = chunk[ci].size;
						f.flashbase = chunk[ci].baseaddr;
						printf("Load HEX flash %08x, %d\n", f.flashbase, f.flashsize);
					} else if (chunk[ci].baseaddr >= AVR_SEGMENT_OFFSET_EEPROM ||
							chunk[ci].baseaddr + loadBase >= AVR_SEGMENT_OFFSET_EEPROM) {
						// eeprom!
						f.eeprom = chunk[ci].data;
						f.eesize = chunk[ci].size;
						printf("Load HEX eeprom %08x, %d\n", chunk[ci].baseaddr, f.eesize);
					}
				}
			} else {
				elf_read_firmware(filename, &f);
			}
		}
	}

	if (strlen(name))
		strcpy(f.mmcu, name);
	if (f_cpu)
		f.frequency = f_cpu;

	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' now known\n", argv[0], f.mmcu);
		exit(1);
	}
	avr_init(avr);
	avr_load_firmware(avr, &f);
	if (f.flashbase) {
		printf("Attempted to load a bootloader at %04x\n", f.flashbase);
		avr->pc = f.flashbase;
	}
	avr->trace = trace;
	for (int ti = 0; ti < trace_vectors_count; ti++)
		if (avr->vector[trace_vectors[ti]])
			avr->vector[trace_vectors[ti]]->trace++;

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (gdb) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	signal(SIGINT, sig_int);
	signal(SIGTERM, sig_int);

	for (;;) {
		int state = avr_run(avr);
		if ( state == cpu_Done || state == cpu_Crashed)
			break;
	}
	
	avr_terminate(avr);
}
