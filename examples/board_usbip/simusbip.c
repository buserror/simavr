/* vim: set sts=4:sw=4:ts=4:noexpandtab
	simusbip.c

	Runs USB device firmware and exports it over USB/IP, see README.
	Derived from extra_board_usb/simusb.c.

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

#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_hex.h"
#include "sim_gdb.h"
#include "usbip_usb.h"

avr_t * avr = NULL;

struct avr_flash {
	char avr_flash_path[1024];
	int avr_flash_fd;
};

// avr special flash initalization
// here: open and map a file to enable a persistent storage for the flash memory
void avr_special_init( avr_t* avr, void *data)
{
	struct avr_flash *flash_data = (struct avr_flash *)data;
	// release flash memory if allocated
	if(avr->flash) free(avr->flash);
	// open the file
	flash_data->avr_flash_fd = open(flash_data->avr_flash_path,
            O_RDWR|O_CREAT, 0644);
	if (flash_data->avr_flash_fd < 0) {
		perror(flash_data->avr_flash_path);
		exit(1);
	}
	// resize and map the file the file
	(void)ftruncate(flash_data->avr_flash_fd, avr->flashend + 1);
	avr->flash = (uint8_t*)mmap(NULL, avr->flashend + 1, // 32k is multiple of 4096
            PROT_READ|PROT_WRITE, MAP_SHARED, flash_data->avr_flash_fd, 0);
	if (avr->flash == MAP_FAILED) {
		fprintf(stderr, "unable to map memory\n");
		perror(flash_data->avr_flash_path);
		exit(1);
	}
}

// avr special flash deinitalization
// here: cleanup the persistent storage
void avr_special_deinit( avr_t* avr, void *data)
{
	struct avr_flash *flash_data = (struct avr_flash *)data;
	// unmap and close the file
	munmap( avr->flash, avr->flashend + 1);
	close(flash_data->avr_flash_fd);
	// signal that cleanup is done
	avr->flash = NULL;
}

static void
usage(const char * name)
{
	fprintf(stderr,
		"Usage: %s [-m mcu] [-f hz] [-p port] [-b boot_address] [-g] [-v] [firmware]\n"
		"  firmware  .hex or .elf, default ../extra_board_usb/at90usb162_cdc_loopback.hex\n"
		"  -m mcu    part, default at90usb162 or the one named in the .elf\n"
		"  -f hz     clock, default 8000000 or the one named in the .elf\n"
		"  -p port   USB/IP TCP port, default %d\n"
		"  -b addr   reset vector, e.g. 0x7000 for a 4KB bootloader on a 32U4\n"
		"  -g        wait for gdb on port 1234\n"
		"  -v        more logging, repeat for more\n"
		"The flash is kept in simusbip_flash.bin between runs, so that a\n"
		"bootloader finds the application it was given last time.\n",
		name, USBIP_USB_DEFAULT_PORT);
	exit(1);
}

int main(int argc, char *argv[])
{
	struct avr_flash flash_data;
	elf_firmware_t fw = {{0}};
	const char * mmcu = NULL;
	char path[1024];
	const char * firmware = NULL;
	uint32_t frequency = 0;
	uint32_t boot = 0;
	uint16_t port = 0;
	int gdb = 0, elf = 0, verbose = 0;
	int opt;

	while ((opt = getopt(argc, argv, "m:f:p:b:gvh")) != -1) {
		switch (opt) {
			case 'm': mmcu = optarg; break;
			case 'f': frequency = strtoul(optarg, NULL, 0); break;
			case 'p': port = strtoul(optarg, NULL, 0); break;
			case 'b': boot = strtoul(optarg, NULL, 0); break;
			case 'g': gdb = 1; break;
			case 'v': verbose++; break;
			default: usage(argv[0]);
		}
	}
	if (optind < argc)
		firmware = argv[optind];
	else {
		snprintf(path, sizeof(path), "%s/../../extra_board_usb/%s",
				dirname(strdup(argv[0])), "at90usb162_cdc_loopback.hex");
		firmware = path;
	}
	elf = strlen(firmware) > 4 && !strcmp(firmware + strlen(firmware) - 4, ".elf");
	if (elf) {
		if (elf_read_firmware(firmware, &fw)) {
			fprintf(stderr, "%s: Unable to load %s\n", argv[0], firmware);
			exit(1);
		}
		if (!mmcu && fw.mmcu[0])
			mmcu = fw.mmcu;
		if (!frequency)
			frequency = fw.frequency;
	}
	if (!mmcu)
		mmcu = "at90usb162";
	if (!frequency)
		frequency = 8000000;

	avr = avr_make_mcu_by_name(mmcu);
	if (!avr) {
		fprintf(stderr, "%s: Error creating the AVR core %s\n", argv[0], mmcu);
		exit(1);
	}
	strcpy(flash_data.avr_flash_path,  "simusbip_flash.bin");
	flash_data.avr_flash_fd = 0;
	// register our own functions
	avr->custom.init = avr_special_init;
	avr->custom.deinit = avr_special_deinit;
	avr->custom.data = &flash_data;
	avr_init(avr);
	avr->frequency = frequency;
	avr->log = LOG_WARNING + verbose;

	// this trick creates a file that contains /and keep/ the flash
	// in the same state as it was before. This allow the bootloader
	// app to be kept, and re-run if the bootloader doesn't get a
	// new one
	if (elf) {
		fw.frequency = frequency;
		avr_load_firmware(avr, &fw);
	} else {
		uint32_t base, size;
		uint8_t * code = read_ihex_file(firmware, &size, &base);
		if (!code) {
			fprintf(stderr, "%s: Unable to load %s\n", argv[0], firmware);
			exit(1);
		}
		printf("Loaded %s, %04x: %d bytes\n", firmware, base, size);
		memcpy(avr->flash + base, code, size);
		free(code);
		if (!boot)
			boot = base;
	}
	avr->codeend = avr->flashend;
	avr->reset_pc = avr->pc = boot;

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (gdb) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	if (!usbip_usb_init(avr, NULL, port))
		exit(1);
	printf("%s at %u Hz, USB/IP server on port %u. To attach it:\n"
			"  sudo modprobe vhci-hcd\n"
			"  sudo usbip attach -r 127.0.0.1 -b %s\n",
			mmcu, frequency, port ? port : USBIP_USB_DEFAULT_PORT,
			USBIP_USB_BUSID);

	for (;;) {
		int state = avr_run(avr);
		if (state == cpu_Done || state == cpu_Crashed)
			break;
	}
	avr_terminate(avr);
	return 0;
}
