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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <libgen.h>

#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_hex.h"
#include "sim_gdb.h"
#include "usbip_usb.h"

static volatile sig_atomic_t stop;

static void
sig_stop(int sig)
{
	stop = 1;
}

/*
 * Persistent flash: read when the core starts and written back when the
 * simulation ends, so that a bootloader finds the application it was
 * given last time. Plain stdio, so that it works on Windows too.
 */
static void
flash_load(avr_t * avr, const char * path)
{
	FILE * f = fopen(path, "rb");

	if (!f)
		return;	// first run
	size_t n = fread(avr->flash, 1, avr->flashend + 1, f);
	fclose(f);
	printf("Flash: %zu bytes from %s\n", n, path);
}

static void
flash_save(avr_t * avr, const char * path)
{
	FILE * f = fopen(path, "wb");

	if (!f || fwrite(avr->flash, 1, avr->flashend + 1, f) != avr->flashend + 1)
		perror(path);
	if (f)
		fclose(f);
}

static void
usage(const char * name)
{
	fprintf(stderr,
		"Usage: %s [-m mcu] [-f hz] [-a addr] [-p port] [-b boot_address]\n"
		"          [-F flash.bin] [-g] [-v] [firmware]\n"
		"  firmware  .hex or .elf, default ../extra_board_usb/at90usb162_cdc_loopback.hex\n"
		"  -m mcu    part, default at90usb162 or the one named in the .elf\n"
		"  -f hz     clock, default 8000000 or the one named in the .elf\n"
		"  -a addr   USB/IP listening address, default 127.0.0.1,\n"
		"            0.0.0.0 lets other machines attach the device\n"
		"  -p port   USB/IP TCP port, default %d\n"
		"  -b addr   reset vector, e.g. 0x7000 for a 4KB bootloader on a 32U4\n"
		"  -F file   keep the flash in file between runs, the firmware is\n"
		"            loaded over it, so a bootloader keeps its application\n"
		"  -g        wait for gdb on port 1234\n"
		"  -v        more logging, repeat for more\n",
		name, USBIP_USB_DEFAULT_PORT);
	exit(1);
}

int main(int argc, char *argv[])
{
	elf_firmware_t fw;
	const char * mmcu = NULL;
	char path[1024];
	const char * firmware = NULL;
	const char * flash_file = NULL;
	const char * addr = NULL;
	uint32_t frequency = 0;
	uint32_t boot = 0;
	uint16_t port = 0;
	int gdb = 0, elf = 0, verbose = 0;
	int opt;
	avr_t * avr;

	memset(&fw, 0, sizeof(fw));
	while ((opt = getopt(argc, argv, "m:f:a:p:b:F:gvh")) != -1) {
		switch (opt) {
			case 'm': mmcu = optarg; break;
			case 'f': frequency = strtoul(optarg, NULL, 0); break;
			case 'a': addr = optarg; break;
			case 'p': port = strtoul(optarg, NULL, 0); break;
			case 'b': boot = strtoul(optarg, NULL, 0); break;
			case 'F': flash_file = optarg; break;
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
	avr_init(avr);
	avr->frequency = frequency;
	avr->log = LOG_WARNING + verbose;

	if (flash_file)
		flash_load(avr, flash_file);
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

	if (!usbip_usb_init(avr, addr, port))
		exit(1);
	printf("%s at %u Hz, USB/IP server on %s port %u. To attach it:\n"
			"  Linux:   sudo modprobe vhci-hcd\n"
			"           sudo usbip attach -r 127.0.0.1 -b %s\n"
			"  Windows: usbip attach -r 127.0.0.1 -b %s (usbip-win2,"
			" from an administrator prompt)\n",
			mmcu, frequency, addr ? addr : "127.0.0.1",
			port ? port : USBIP_USB_DEFAULT_PORT,
			USBIP_USB_BUSID, USBIP_USB_BUSID);
	fflush(stdout);

	signal(SIGINT, sig_stop);
	signal(SIGTERM, sig_stop);
	while (!stop) {
		int state = avr_run(avr);
		if (state == cpu_Done || state == cpu_Crashed)
			break;
	}
	if (flash_file)
		flash_save(avr, flash_file);
	avr_terminate(avr);
	return 0;
}
