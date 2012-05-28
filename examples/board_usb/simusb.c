/* vim: set sts=4:sw=4:ts=4:noexpandtab
	simusb.c

	Copyright 2012 Torbjorn Tyridal <ttyridal@gmail.com>

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

#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>

#include <pthread.h>

#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_elf.h"
#include "sim_hex.h"
#include "sim_gdb.h"
#include "vhci_usb.h"
#include "sim_vcd_file.h"

struct vhci_usb_t vhci_usb;
avr_t * avr = NULL;
avr_vcd_t vcd_file;



char avr_flash_path[1024];
int avr_flash_fd = 0;

// avr special flash initalization
// here: open and map a file to enable a persistent storage for the flash memory
void avr_special_init( avr_t* avr)
{
	//puts(" --=== INIT CALLED ===--");
	// release flash memory if allocated
	if(avr->flash) free(avr->flash);
	// open the file
	avr_flash_fd = open(avr_flash_path, O_RDWR|O_CREAT, 0644);
	if (avr_flash_fd < 0) {
		perror(avr_flash_path);
		exit(1);
	}
	// resize and map the file the file
	(void)ftruncate(avr_flash_fd, avr->flashend + 1);
	avr->flash = (uint8_t*)mmap(NULL, avr->flashend + 1, // 32k is multiple of 4096
							PROT_READ|PROT_WRITE, MAP_SHARED, avr_flash_fd, 0);
	if (!avr->flash) {
		fprintf(stderr, "unable to map memory\n");
		perror(avr_flash_path);
		exit(1);
	}
}

// avr special flash deinitalization
// here: cleanup the persistent storage
void avr_special_deinit( avr_t* avr)
{
	//puts(" --=== DEINIT CALLED ===--");
	// unmap and close the file
	munmap( avr->flash, avr->flashend + 1);
	close( avr_flash_fd);
	// signal that cleanup is done
	avr->flash = NULL;
}

int main(int argc, char *argv[])
{
//		elf_firmware_t f;
	const char * pwd = dirname(argv[0]);

	avr = avr_make_mcu_by_name("at90usb162");
	if (!avr) {
		fprintf(stderr, "%s: Error creating the AVR core\n", argv[0]);
		exit(1);
	}
	strcpy(avr_flash_path,  "simusb_flash.bin");
	// register our own functions
	avr->special_init = avr_special_init;
	avr->special_deinit = avr_special_deinit;
	//avr->reset = NULL;
	avr_init(avr);
	avr->frequency = 8000000;

	// this trick creates a file that contains /and keep/ the flash
	// in the same state as it was before. This allow the bootloader
	// app to be kept, and re-run if the bootloader doesn't get a
	// new one
	{
		char path[1024];
		uint32_t base, size;
		snprintf(path, sizeof(path), "%s/../%s", pwd, "at90usb162_cdc_loopback.hex");

		uint8_t * boot = read_ihex_file(path, &size, &base);
		if (!boot) {
			fprintf(stderr, "%s: Unable to load %s\n", argv[0], path);
			exit(1);
		}
		printf("Booloader %04x: %d\n", base, size);
		memcpy(avr->flash + base, boot, size);
		free(boot);
		avr->pc = base;
		avr->codeend = avr->flashend;
	}

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (0) {
		//avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	vhci_usb_init(avr, &vhci_usb);
	vhci_usb_connect(&vhci_usb, '0');


	while (1) {
		avr_run(avr);
	}
}
