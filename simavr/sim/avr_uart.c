/*
	avr_uart.c

	Handles UART access
	Right now just handle "write" to the serial port at any speed
	and printf to the console when '\n' is written.

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
#include "avr_uart.h"

static void avr_uart_run(avr_t * avr, avr_io_t * port)
{
//	printf("%s\n", __FUNCTION__);
}

static uint8_t avr_uart_read(struct avr_t * avr, uint8_t addr, void * param)
{
//	avr_uart_t * p = (avr_uart_t *)param;
	uint8_t v = avr->data[addr];
//	printf("** PIN%c = %02x\n", p->name, v);
	return v;
}

static void avr_uart_write(struct avr_t * avr, uint8_t addr, uint8_t v, void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;

	if (addr == p->r_udr) {
	//	printf("UDR%c(%02x) = %02x\n", p->name, addr, v);
		avr_core_watch_write(avr, addr, v);
		avr_regbit_set(avr, p->udre);

		static char buf[128];
		static int l = 0;
		buf[l++] = v < ' ' ? '.' : v;
		buf[l] = 0;
		if (v == '\n' || l == 127) {
			l = 0;
			printf("\e[32m%s\e[0m\n", buf);
		}
	}
}

void avr_uart_reset(avr_t * avr, struct avr_io_t *io)
{
	avr_uart_t * p = (avr_uart_t *)io;
	avr_regbit_set(avr, p->udre);
}

static	avr_io_t	_io = {
	.kind = "uart",
	.run = avr_uart_run,
	.reset = avr_uart_reset,
};

void avr_uart_init(avr_t * avr, avr_uart_t * p)
{
	p->io = _io;
	avr_register_io(avr, &p->io);

	printf("%s UART%c UDR=%02x\n", __FUNCTION__, p->name, p->r_udr);

	avr_register_io_write(avr, p->r_udr, avr_uart_write, p);
	avr_register_io_read(avr, p->r_udr, avr_uart_read, p);

}

