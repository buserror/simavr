/*
	usbserial.c

	The USB side of an Arduino Uno R3: its ATmega16U2 at 16MHz running
	Arduino-usbserial, the USB to serial firmware, with the USB port
	exported over USB/IP. USART1, which goes to the ATmega328P on the
	board, is looped back or connected to a pty. See README.

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
#include <signal.h>
#include <unistd.h>
#include <libgen.h>

#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_hex.h"
#include "sim_gdb.h"
#include "avr_uart.h"
#include "avr_ioport.h"
#include "usbip_usb.h"
#ifndef __MINGW32__
#include "uart_pty.h"
#endif

#define MMCU		"atmega16u2"
#define FREQUENCY	16000000
#define FIRMWARE	"Arduino-usbserial-atmega16u2-Uno-Rev3.hex"
#define RESET_PIN	7		// PD7, to the ATmega328P's RESET

static volatile sig_atomic_t stop;

static void
sig_stop(int sig)
{
	stop = 1;
}

// DTR pulls the 328P's reset line low, that's how avrdude resets it
static void
reset_line_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	printf("328P RESET %s\n", value ? "released" : "pulled low (DTR)");
	fflush(stdout);
}

static void
usage(const char * name)
{
	fprintf(stderr,
		"Usage: %s [-a addr] [-p port] [-t] [-g] [-v] [firmware]\n"
		"  firmware  .hex or .elf, default " FIRMWARE "\n"
		"  -a addr   USB/IP listening address, default 127.0.0.1,\n"
		"            0.0.0.0 lets other machines attach the board\n"
		"  -p port   USB/IP TCP port, default %d\n"
#ifndef __MINGW32__
		"  -t        USART1 to a pty (/tmp/simavr-uart1) instead of\n"
		"            looping it back\n"
#endif
		"  -g        wait for gdb on port 1234\n"
		"  -v        more logging, repeat for more\n",
		name, USBIP_USB_DEFAULT_PORT);
	exit(1);
}

int main(int argc, char *argv[])
{
	elf_firmware_t fw = { .mmcu = MMCU, .frequency = FREQUENCY };
	char path[1024];
	const char * firmware = NULL;
	const char * addr = "127.0.0.1";
	uint16_t port = USBIP_USB_DEFAULT_PORT;
	int gdb = 0, verbose = 0, pty = 0;
	int opt;
	avr_t * avr;

	while ((opt = getopt(argc, argv, "a:p:tgvh")) != -1) {
		switch (opt) {
			case 'a': addr = optarg; break;
			case 'p': port = strtoul(optarg, NULL, 0); break;
#ifndef __MINGW32__
			case 't': pty = 1; break;
#endif
			case 'g': gdb = 1; break;
			case 'v': verbose++; break;
			default: usage(argv[0]);
		}
	}
	if (optind < argc)
		firmware = argv[optind++];
	if (optind < argc)
		usage(argv[0]);
	if (!firmware) {
		// next to the obj-* directory of the binary
		snprintf(path, sizeof(path), "%s/../%s", dirname(strdup(argv[0])),
				FIRMWARE);
		firmware = path;
	}

	sim_setup_firmware(firmware, 0, &fw, argv[0]);
	avr = avr_make_mcu_by_name(fw.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: Error creating the AVR core %s\n", argv[0],
				fw.mmcu);
		exit(1);
	}
	avr_init(avr);
	avr_load_firmware(avr, &fw);
	avr->log = LOG_WARNING + verbose;

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (gdb) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'),
			IOPORT_IRQ_PIN0 + RESET_PIN), reset_line_hook, NULL);
#ifndef __MINGW32__
	static uart_pty_t uart_pty;
	if (pty) {
		uart_pty_init(avr, &uart_pty);
		uart_pty_connect(&uart_pty, '1');
	} else
#endif
	{
		// a wire from TX to RX: what is sent to the serial port comes back
		avr_connect_irq(
			avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('1'), UART_IRQ_OUTPUT),
			avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('1'), UART_IRQ_INPUT));
	}

	if (!usbip_usb_init(avr, addr, port))
		exit(1);
	printf("%s at %u Hz, USART1 %s, USB/IP server on %s port %u.\n"
			"To attach it:\n"
			"  Linux:   sudo modprobe vhci-hcd\n"
			"           sudo usbip attach -r 127.0.0.1 -b %s\n"
			"  Windows: usbip attach -r 127.0.0.1 -b %s (usbip-win2,"
			" from an administrator prompt)\n",
			avr->mmcu, (unsigned)avr->frequency,
			pty ? "on /tmp/simavr-uart1" : "looped back",
			addr, port,
			USBIP_USB_BUSID, USBIP_USB_BUSID);
	fflush(stdout);

	signal(SIGINT, sig_stop);
	signal(SIGTERM, sig_stop);
	while (!stop) {
		int state = avr_run(avr);
		if (state == cpu_Done || state == cpu_Crashed)
			break;
	}
#ifndef __MINGW32__
	if (pty)
		uart_pty_stop(&uart_pty);
#endif
	avr_terminate(avr);
	return 0;
}
