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

static avr_cycle_count_t avr_timer8_compa(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_timer8_t * p = (avr_timer8_t *)param;
	avr_raise_interrupt(avr, &p->compa);
	return p->compa_cycles ? when + p->compa_cycles : 0;
}

static avr_cycle_count_t avr_timer8_compb(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_timer8_t * p = (avr_timer8_t *)param;
	avr_raise_interrupt(avr, &p->compb);
	return p->compb_cycles ? when + p->compb_cycles : 0;
}

static uint8_t avr_timer8_tcnt_read(struct avr_t * avr, uint8_t addr, void * param)
{
	//avr_timer8_t * p = (avr_timer8_t *)param;
	// made to trigger potential watchpoints
	return avr_core_watch_read(avr, addr);
}

static void avr_timer8_write(struct avr_t * avr, uint8_t addr, uint8_t v, void * param)
{
	avr_timer8_t * p = (avr_timer8_t *)param;

	p->compa_cycles = 0;
	p->compb_cycles = 0;

	avr_core_watch_write(avr, addr, v);
	long clock = avr->frequency;
	if (avr_regbit_get(avr, p->as2))
		clock = 32768;
	uint8_t cs = avr_regbit_get_array(avr, p->cs, ARRAY_SIZE(p->cs));
	if (cs == 0) {
		printf("%s-%c clock turned off\n", __FUNCTION__, p->name);		
		avr_cycle_timer_cancel(avr, avr_timer8_compa, p);
		avr_cycle_timer_cancel(avr, avr_timer8_compb, p);
		return;
	}
	uint8_t mode = avr_regbit_get_array(avr, p->wgm, ARRAY_SIZE(p->wgm));
	uint8_t cs_div = p->cs_div[cs];
	uint16_t ocra = avr->data[p->r_ocra];
	uint16_t ocrb = avr->data[p->r_ocrb];
	long f = clock >> cs_div;
	long fa = f / (ocra+1), fb = f / (ocrb+1);

//	printf("%s-%c clock f=%ld cs=%02x (div %d) = %ldhz\n", __FUNCTION__, p->name, clock, cs, 1 << cs_div, f);
	if (ocra) printf("%s-%c wgm %d OCRA=%3d = %ldhz\n", __FUNCTION__, p->name, mode, ocra, fa);
	if (ocrb) printf("%s-%c wgm %d OCRB=%3d = %ldhz\n", __FUNCTION__, p->name, mode, ocrb, fb);

	p->compa_cycles = avr_hz_to_cycles(avr, fa);
	p->compb_cycles = avr_hz_to_cycles(avr, fb);
	if (p->compa_cycles)
		avr_cycle_timer_register(avr, p->compa_cycles, avr_timer8_compa, p);
	if (p->compb_cycles)
		avr_cycle_timer_register(avr, p->compb_cycles, avr_timer8_compb, p);
//	printf("%s-%c A %ld/%ld = cycles = %d\n", __FUNCTION__, p->name, (long)avr->frequency, fa, (int)p->compa_cycles);
//	printf("%s-%c B %ld/%ld = cycles = %d\n", __FUNCTION__, p->name, (long)avr->frequency, fb, (int)p->compb_cycles);
}

static void avr_timer8_reset(avr_io_t * port)
{
	avr_timer8_t * p = (avr_timer8_t *)port;
	avr_cycle_timer_cancel(p->io.avr, avr_timer8_compa, p);
	avr_cycle_timer_cancel(p->io.avr, avr_timer8_compb, p);
	p->compa_cycles = 0;
	p->compb_cycles = 0;
}

static	avr_io_t	_io = {
	.kind = "timer8",
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

	avr_register_io_read(avr, p->r_tcnt, avr_timer8_tcnt_read, p);
}
