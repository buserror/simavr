/*
 ds1338_demo.c

 Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>
 Copyright 2011 Michel Pollet <buserror@gmail.com>

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

#include "sim_avr.h"
#include "avr_twi.h"
#include "sim_elf.h"
#include "sim_gdb.h"
#include "ds1338_virt.h"

avr_t * avr = NULL;
ds1338_virt_t ds1338_virt;

int main(int argc, char *argv[])
{
	elf_firmware_t f = {{0}};
	const char * fname =  "atmega32_ds1338.axf";

	printf("Firmware pathname is %s\n", fname);
	elf_read_firmware(fname, &f);

	printf("firmware %s f=%d mmcu=%s\n", fname, (int)f.frequency, f.mmcu);

	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' not known\n", argv[0], f.mmcu);
		exit(1);
	}
	avr_init(avr);
	avr_load_firmware(avr, &f);

	// Initialise our 'peripheral'
	ds1338_virt_init(avr, &ds1338_virt);

	// Hook up the TWI bus
	ds1338_virt_attach_twi(&ds1338_virt, AVR_IOCTL_TWI_GETIRQ(0));

	// Connect the square wave output
	ds1338_pin_t wiring = {
		.port = 'D',
		.pin = 3
	};
	ds1338_virt_attach_square_wave_output (&ds1338_virt, &wiring);

	// Even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (0) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	printf( "\nDS1338 demo launching:\n");

	// Enable debug info
	// TODO: Convert to logger?
	ds1338_virt.verbose = 1;

	int state = cpu_Running;
	while ((state != cpu_Done) && (state != cpu_Crashed))
		state = avr_run(avr);
}
