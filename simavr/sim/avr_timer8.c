/*
	avr_timer8.c

	Handles the just one mode of the 8 bit AVR timer.
	Still need to handle all the others!

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
#include "avr_timer8.h"

static void avr_timer8_run(avr_t * avr, avr_io_t * port)
{
	avr_timer8_t * p = (avr_timer8_t *)port;
	//printf("%s\n", __FUNCTION__);

	if (p->compa_cycles) {
		if (p->compa_next == 0) {
			p->compa_next = avr->cycle + p->compa_cycles;
		}
		if (avr->cycle >= p->compa_next) {
		//	printf("timer a firea %d\n", p->compa_next);
			fflush(stdout);
			p->compa_next += p->compa_cycles;						
			avr_raise_interrupt(avr, &p->compa);
		} 
	}
}


static void avr_timer8_write(struct avr_t * avr, uint8_t addr, uint8_t v, void * param)
{
	avr_timer8_t * p = (avr_timer8_t *)param;
//	uint8_t oldv = avr->data[addr];

	p->compa_cycles = 0;
	p->compa_next = 0;

	avr_core_watch_write(avr, addr, v);
	long clock = avr->frequency;
	if (avr_regbit_get(avr, p->as2))
		clock = 32768;
	uint8_t cs = avr_regbit_get_array(avr, p->cs, ARRAY_SIZE(p->cs));
	if (cs == 0) {
		printf("%s-%c clock turned off\n", __FUNCTION__, p->name);		
		p->compa_cycles = 0;
		return;
	}
	uint8_t mode = avr_regbit_get_array(avr, p->wgm, ARRAY_SIZE(p->wgm));
	uint8_t cs_div = p->cs_div[cs];
	uint16_t ocra = avr->data[p->r_ocra];
	uint16_t ocrb = avr->data[p->r_ocrb];
	long f = clock >> cs_div;
	long fa = f / 2 / (ocra+1), fb = f / 2 / (ocrb+1);

	printf("%s-%c clock f=%ld cs=%02x (div %d) = %ldhz\n", __FUNCTION__, p->name, clock, cs, 1 << cs_div, f);
	printf("%s-%c wgm %d OCRA=%3d = %ldhz\n", __FUNCTION__, p->name, mode, ocra, fa);
	printf("%s-%c wgm %d OCRB=%3d = %ldhz\n", __FUNCTION__, p->name, mode, ocrb, fb);	

	long cocra = ocra ? avr->frequency / fa : 0;
	p->compa_cycles = cocra;
	printf("%s-%c A %ld/%ld = cycles = %ld\n", __FUNCTION__, p->name, (long)avr->frequency, fa, cocra);
	
}

static void avr_timer8_reset(avr_t * avr, avr_io_t * port)
{
}

static	avr_io_t	_io = {
	.kind = "timer8",
	.run = avr_timer8_run,
	.reset = avr_timer8_reset,
};

void avr_timer8_init(avr_t * avr, avr_timer8_t * p)
{
	p->io = _io;
//	printf("%s timer%c created\n", __FUNCTION__, p->name);

	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->compa);

	avr_register_io_write(avr, p->cs[0].reg, avr_timer8_write, p);
	avr_register_io_write(avr, p->r_ocra, avr_timer8_write, p);
	avr_register_io_write(avr, p->r_ocrb, avr_timer8_write, p);
}
