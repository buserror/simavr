/*
	avr_ioport.c

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

#include <stdio.h>
#include "avr_ioport.h"

static void avr_ioport_run(avr_t * avr, avr_io_t * port)
{
	//printf("%s\n", __FUNCTION__);
}

static uint8_t avr_ioport_read(struct avr_t * avr, uint8_t addr, void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;
	uint8_t v = avr->data[addr];

	if (addr == p->r_pin) {
		uint8_t v = avr->data[p->r_port];
		avr->data[addr] = v;
		// made to trigger potential watchpoints
		v = avr_core_watch_read(avr, addr);
//		printf("** PIN%c(%02x) = %02x\n", p->name, addr, v);
	}
	return v;
}

static void avr_ioport_write(struct avr_t * avr, uint8_t addr, uint8_t v, void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;
	uint8_t oldv = avr->data[addr];

	if (addr == p->r_port) {
	//	printf("PORT%c(%02x) = %02x (was %02x)\n", p->name, addr, v, oldv);

		avr_core_watch_write(avr, addr, v);
		if (v != oldv) {
			int raise = 1;
			int mask = v ^ oldv;
			if (p->r_pcint)
				raise = avr->data[p->r_pcint] & mask;
			if (raise)
				avr_raise_interupt(avr, &p->pcint);
		}


		if (p->name == 'D') {
			static int cs = -1;
			if ((oldv & 0xf0) != (v & 0xf0)) {
				for (int i = 0; i < 4; i++) {
					
				}
			} 
			{
			}
		}
	}
}

static void avr_ioport_reset(avr_t * avr, avr_io_t * port)
{
}

static	avr_io_t	_io = {
	.kind = "io",
	.run = avr_ioport_run,
	.reset = avr_ioport_reset,
};

void avr_ioport_init(avr_t * avr, avr_ioport_t * port)
{
	port->io = _io;
	printf("%s PIN%c 0x%02x DDR%c 0x%02x PORT%c 0x%02x\n",
		__FUNCTION__,
		port->name, port->r_pin,
		port->name, port->r_ddr,
		port->name, port->r_port);

	avr_register_io(avr, &port->io);
	avr_register_vector(avr, &port->pcint);

	avr_register_io_write(avr, port->r_port, avr_ioport_write, port);
	avr_register_io_read(avr, port->r_pin, avr_ioport_read, port);
}

