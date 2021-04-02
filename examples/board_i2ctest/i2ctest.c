/*
	i2ctest.c

	Copyright 2008-2011 Michel Pollet <buserror@gmail.com>

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
#include <pthread.h>

#include "sim_avr.h"
#include "avr_twi.h"
#include "sim_elf.h"
#include "sim_gdb.h"
#include "sim_vcd_file.h"
#include "i2c_eeprom.h"

avr_t * avr = NULL;
avr_vcd_t vcd_file;

i2c_eeprom_t ee;

int main(int argc, char *argv[])
{
	elf_firmware_t f = {{0}};
	const char * fname =  "atmega1280_i2ctest.axf";

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

	// initialize our 'peripheral', setting the mask to allow read and write
	i2c_eeprom_init(avr, &ee, 0xa0, 0x01, NULL, 1024);

	i2c_eeprom_attach(avr, &ee, AVR_IOCTL_TWI_GETIRQ(0));
	ee.verbose = 1;

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (0) {
		//avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	/*
	 *	VCD file initialization
	 *
	 *	This will allow you to create a "wave" file and display it in gtkwave
	 *	Pressing "r" and "s" during the demo will start and stop recording
	 *	the pin changes
	 */
//	avr_vcd_init(avr, "gtkwave_output.vcd", &vcd_file, 100000 /* usec */);
//	avr_vcd_add_signal(&vcd_file,
//		avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_STATUS), 8 /* bits */ ,
//		"TWSR" );

	printf( "\nDemo launching:\n");

	int state = cpu_Running;
	while ((state != cpu_Done) && (state != cpu_Crashed))
		state = avr_run(avr);
}
