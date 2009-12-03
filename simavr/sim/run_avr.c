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
#include <getopt.h>
#include <string.h>
#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "avr_uart.h"

void hdump(const char *w, uint8_t *b, size_t l)
{
	uint32_t i;
	if (l < 16) {
		printf("%s: ",w);
		for (i = 0; i < l; i++) printf("%02x",b[i]);
	} else {
		printf("%s:\n",w);
		for (i = 0; i < l; i++) {
			if (!(i & 0x1f)) printf("    ");
			printf("%02x",b[i]);
			if ((i & 0x1f) == 0x1f) {
				printf(" ");
				printf("\n");
			}
		}
	}
	printf("\n");
}


void display_usage()
{
	printf("usage: simavr [-t] [-g] [-m <device>] [-f <frequency>] firmware\n");
	printf("       -t: run full scale decoder trace\n");
	printf("       -g: listen for gdb connection on port 1234\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	elf_firmware_t f;
	long f_cpu = 0;
	int trace = 0;
	int gdb = 0;
	char name[16] = "";
	int option_count;
	int option_index = 0;

	struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"mcu", required_argument, 0, 'm'},
		{"freq", required_argument, 0, 'f'},
		{"trace", no_argument, 0, 't'},
		{"gdb", no_argument, 0, 'g'},
		{0, 0, 0, 0}
	};

	if (argc == 1)
		display_usage();

	while ((option_count = getopt_long(argc, argv, "tghm:f:", long_options, &option_index)) != -1) {
		switch (option_count) {
			case 'h':
				display_usage();
				break;
			case 'm':
				strcpy(name, optarg);
				break;
			case 'f':
				f_cpu = atoi(optarg);
				break;
			case 't':
				trace++;
				break;
			case 'g':
				gdb++;
				break;
		}
	}

	elf_read_firmware(argv[argc-1], &f);

	if (strlen(name))
		strcpy(f.mmcu.name, name);
	if (f_cpu)
		f.mmcu.f_cpu = f_cpu;

	printf("firmware %s f=%d mmcu=%s\n", argv[argc-1], (int)f.mmcu.f_cpu, f.mmcu.name);

	avr_t * avr = avr_make_mcu_by_name(f.mmcu.name);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' now known\n", argv[0], f.mmcu.name);
		exit(1);
	}
	avr_init(avr);
	avr_load_firmware(avr, &f);
	avr->trace = trace;

	// try to enable "local echo" on the first uart, for testing purposes
	{
		avr_irq_t * src = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT);
		avr_irq_t * dst = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_INPUT);
		if (src && dst) {
			printf("%s:%s activating uart local echo IRQ src %p dst %p\n", __FILE__, __FUNCTION__, src, dst);
			avr_connect_irq(src, dst);
		}
	}
	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (gdb) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	for (;;)
		avr_run(avr);	
}
