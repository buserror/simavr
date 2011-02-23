/*
	sim_io.c

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
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include "sim_io.h"

int
avr_ioctl(
		avr_t *avr,
		uint32_t ctl,
		void * io_param)
{
	avr_io_t * port = avr->io_port;
	int res = -1;
	while (port && res == -1) {
		if (port->ioctl)
			res = port->ioctl(port, ctl, io_param);
		port = port->next;
	}
	return res;
}

void
avr_register_io(
		avr_t *avr,
		avr_io_t * io)
{
	io->next = avr->io_port;
	io->avr = avr;
	avr->io_port = io;
}

void
avr_register_io_read(
		avr_t *avr,
		avr_io_addr_t addr,
		avr_io_read_t readp,
		void * param)
{
	avr_io_addr_t a = AVR_DATA_TO_IO(addr);
	if (avr->io[a].r.param || avr->io[a].r.c) {
		if (avr->io[a].r.param != param || avr->io[a].r.c != readp) {
			fprintf(stderr,
					"Error: avr_register_io_read(): Already registered, refusing to override.\n");
			fprintf(stderr,
					"Error: avr_register_io_read(%04x : %p/%p): %p/%p\n", a,
					avr->io[a].r.c, avr->io[a].r.param, readp, param);
			abort();
		}
	}
	avr->io[a].r.param = param;
	avr->io[a].r.c = readp;
}

static void
_avr_io_mux_write(
		avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	int io = (intptr_t)param;
	for (int i = 0; i < avr->io_shared_io[io].used; i++) {
		avr_io_write_t c = avr->io_shared_io[io].io[i].c;
		if (c)
			c(avr, addr, v, avr->io_shared_io[io].io[i].param);
	}
}

void
avr_register_io_write(
		avr_t *avr,
		avr_io_addr_t addr,
		avr_io_write_t writep,
		void * param)
{
	avr_io_addr_t a = AVR_DATA_TO_IO(addr);

	/*
	 * Verifying that some other piece of code is not installed to watch write
	 * on this address. If there is, this code installs a "dispatcher" callback
	 * instead to handle multiple clients, otherwise, it continues as usual
	 */
	if (avr->io[a].w.param || avr->io[a].w.c) {
		if (avr->io[a].w.param != param || avr->io[a].w.c != writep) {
			// if the muxer not already installed, allocate a new slot
			if (avr->io[a].w.c != _avr_io_mux_write) {
				int no = avr->io_shared_io_count++;
				if (avr->io_shared_io_count > 4) {
					fprintf(stderr,
							"Error: avr_register_io_write(): Too many shared IO registers.\n");
					abort();
				}
				fprintf(stderr,
						"Note: avr_register_io_write(%04x): Installing muxer on register.\n", addr);
				avr->io_shared_io[no].used = 1;
				avr->io_shared_io[no].io[0].param = avr->io[a].w.param;
				avr->io_shared_io[no].io[0].c = avr->io[a].w.c;
				avr->io[a].w.param = (void*)(intptr_t)no;
				avr->io[a].w.c = _avr_io_mux_write;
			}
			int no = (intptr_t)avr->io[a].w.param;
			int d = avr->io_shared_io[no].used++;
			if (avr->io_shared_io[no].used > 4) {
				fprintf(stderr,
						"Error: avr_register_io_write(): Too many callbacks on %04x.\n", addr);
				abort();
			}
			avr->io_shared_io[no].io[d].param = param;
			avr->io_shared_io[no].io[d].c = writep;
		}
	}

	avr->io[a].w.param = param;
	avr->io[a].w.c = writep;
}

avr_irq_t *
avr_io_getirq(
		avr_t * avr,
		uint32_t ctl,
		int index)
{
	avr_io_t * port = avr->io_port;
	while (port) {
		if (port->irq && port->irq_ioctl_get == ctl && port->irq_count > index)
			return port->irq + index;
		port = port->next;
	}
	return NULL;
	
}

avr_irq_t *
avr_iomem_getirq(
		avr_t * avr,
		avr_io_addr_t addr,
		int index)
{
	avr_io_addr_t a = AVR_DATA_TO_IO(addr);
	if (avr->io[a].irq == NULL) {
		avr->io[a].irq = avr_alloc_irq(&avr->irq_pool, 0, 9, NULL);
		// mark the pin ones as filtered, so they only are raised when changing
		for (int i = 0; i < 8; i++)
			avr->io[a].irq[i].flags |= IRQ_FLAG_FILTERED;
	}
	return index < 9 ? avr->io[a].irq + index : NULL;
}

avr_irq_t *
avr_io_setirqs(
		avr_io_t * io,
		uint32_t ctl,
		int count,
		avr_irq_t * irqs )
{
	// allocate this module's IRQ
	io->irq_count = count;

	if (!irqs) {
		const char ** irq_names = NULL;

		if (io->irq_names) {
			irq_names = malloc(count * sizeof(char*));
			memset(irq_names, 0, count * sizeof(char*));
			char buf[64];
			for (int i = 0; i < count; i++) {
				/*
				 * this bit takes the io module 'kind' ("port")
				 * the IRQ name ("=0") and the last character of the ioctl ('p','o','r','A')
				 * to create a full name "=porta.0"
				 */
				char * dst = buf;
				// copy the 'flags' of the name out
				const char * kind = io->irq_names[i];
				while (!isalpha(*kind))
					*dst++ = *kind++;
				// add avr name
//				strcpy(dst, io->avr->mmcu);
				strcpy(dst, "avr");
				dst += strlen(dst);
				*dst ++ = '.';
				// add module 'kind'
				strcpy(dst, io->kind);
				dst += strlen(dst);
				// add port name, if any
				if ((ctl & 0xff) > ' ')
					*dst ++ = tolower(ctl & 0xff);
				*dst ++ = '.';
				// add the rest of the irq name
				strcpy(dst, kind);
				dst += strlen(dst);
				*dst = 0;

//				printf("%s\n", buf);
				irq_names[i] = strdup(buf);
			}
		}
		irqs = avr_alloc_irq(&io->avr->irq_pool, 0,
						count, irq_names);
		if (irq_names) {
			for (int i = 0; i < count; i++)
				free((char*)irq_names[i]);
			free((char*)irq_names);
		}
	}

	io->irq = irqs;
	io->irq_ioctl_get = ctl;
	return io->irq;
}

static void
avr_deallocate_io(
		avr_io_t * io)
{
	if (io->dealloc)
		io->dealloc(io);
	avr_free_irq(io->irq, io->irq_count);
	io->irq_count = 0;
	io->irq_ioctl_get = 0;
	io->avr = NULL;
	io->next = NULL;
}

void
avr_deallocate_ios(
		avr_t * avr)
{
	avr_io_t * port = avr->io_port;
	while (port) {
		avr_io_t * next = port->next;
		avr_deallocate_io(port);
		port = next;
	}
	avr->io_port = NULL;
}
