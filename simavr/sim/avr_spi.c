/*
	avr_spi.c

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
#include "avr_spi.h"

static void avr_spi_run(avr_t * avr, avr_io_t * port)
{
//	printf("%s\n", __FUNCTION__);
}

#if 0
static uint8_t avr_spi_read(struct avr_t * avr, uint8_t addr, void * param)
{
	avr_spi_t * p = (avr_spi_t *)param;
	uint8_t v = avr->data[addr];
//	printf("** PIN%c = %02x\n", p->name, v);
	return v;
}

static void avr_spi_write(struct avr_t * avr, uint8_t addr, uint8_t v, void * param)
{
	avr_spi_t * p = (avr_spi_t *)param;

	if (addr == p->r_udr) {
	//	printf("UDR%c(%02x) = %02x\n", p->name, addr, v);
		avr_core_watch_write(avr, addr, v);
		avr_regbit_set(avr, p->udre);

		static char buf[128];
		static int l = 0;
		buf[l++] = v <= ' ' ? '.' : v;
		buf[l] = 0;
		if (v == '\n' || l == 127) {
			l = 0;
			printf("\e[32m%s\e[0m\n", buf);
		}
	}
}
#endif

void avr_spi_reset(avr_t * avr, struct avr_io_t *io)
{
//	avr_spi_t * p = (avr_spi_t *)io;
//	avr_regbit_set(avr, p->udre);
}

static	avr_io_t	_io = {
	.kind = "spi",
	.run = avr_spi_run,
	.reset = avr_spi_reset,
};

void avr_spi_init(avr_t * avr, avr_spi_t * p)
{
	p->io = _io;
	avr_register_io(avr, &p->io);

	printf("%s SPI%c init\n", __FUNCTION__, p->name);

//	avr_register_io_write(avr, p->r_udr, avr_spi_write, p);
//	avr_register_io_read(avr, p->r_udr, avr_spi_read, p);
}

