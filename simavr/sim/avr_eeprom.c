/*
	avr_eeprom.c

	IO module that simulates the AVR EEProm

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
#include <stdlib.h>
#include <string.h>
#include "avr_eeprom.h"

static void avr_eeprom_run(avr_t * avr, avr_io_t * port)
{
	avr_eeprom_t * p = (avr_eeprom_t *)port;
	//printf("%s\n", __FUNCTION__);
	if (p->eempe_clear_timer) {
		p->eempe_clear_timer--;
		if (p->eempe_clear_timer == 0) {
			avr_regbit_clear(avr, p->eempe);
		}
	}
	if (p->ready_raise_timer) {
		p->ready_raise_timer--;
		if (p->ready_raise_timer == 0) {
			avr_raise_interrupt(avr, &p->ready);
		}
	}
}

static void avr_eeprom_write(struct avr_t * avr, uint8_t addr, uint8_t v, void * param)
{
	avr_eeprom_t * p = (avr_eeprom_t *)param;
	uint8_t eempe = avr_regbit_get(avr, p->eempe);

	avr_core_watch_write(avr, addr, v);

	if (!eempe && avr_regbit_get(avr, p->eempe)) {
		p->eempe_clear_timer = 4;	// auto clear, later
	}
	
	if (eempe && avr_regbit_get(avr, p->eepe)) {	// write operation
		uint16_t addr = avr->data[p->r_eearl] | (avr->data[p->r_eearh] << 8);
	//	printf("eeprom write %04x <- %02x\n", addr, avr->data[p->r_eedr]);
		p->eeprom[addr] = avr->data[p->r_eedr];	
		// Automatically clears that bit (?)
		p->eempe_clear_timer = 0;
		avr_regbit_clear(avr, p->eempe);

		p->ready_raise_timer = 1024; // make a avr_milliseconds_to_cycle(...) 3.4ms here
	}
	if (avr_regbit_get(avr, p->eere)) {	// read operation
		uint16_t addr = avr->data[p->r_eearl] | (avr->data[p->r_eearh] << 8);
		avr->data[p->r_eedr] = p->eeprom[addr];
	//	printf("eeprom read %04x : %02x\n", addr, p->eeprom[addr]);
	}

	// autocleared
	avr_regbit_clear(avr, p->eepe);
	avr_regbit_clear(avr, p->eere);
}

static int avr_eeprom_ioctl(avr_t * avr, avr_io_t * port, uint32_t ctl, void * io_param)
{
	avr_eeprom_t * p = (avr_eeprom_t *)port;
	int res = -1;

	switch(ctl) {
		case AVR_IOCTL_EEPROM_SET: {
			avr_eeprom_desc_t * desc = (avr_eeprom_desc_t*)io_param;
			if (!desc || !desc->size || !desc->ee || (desc->offset + desc->size) >= p->size) {
				printf("%s: AVR_IOCTL_EEPROM_SET Invalid argument\n",
						__FUNCTION__);
				return -2;
			}
			memcpy(p->eeprom + desc->offset, desc->ee, desc->size);
			printf("%s: AVR_IOCTL_EEPROM_SET Loaded %d at offset %d\n",
					__FUNCTION__, desc->size, desc->offset);
		}	break;
		case AVR_IOCTL_EEPROM_GET: {
			avr_eeprom_desc_t * desc = (avr_eeprom_desc_t*)io_param;
			if (!desc || (desc->offset + desc->size) >= p->size) {
				printf("%s: AVR_IOCTL_EEPROM_GET Invalid argument\n",
						__FUNCTION__);
				return -2;
			}
			if (desc->ee)
				memcpy(desc->ee, p->eeprom + desc->offset, desc->size);
			else	// allow to get access to the read data, for gdb support
				desc->ee = p->eeprom + desc->offset;
		}	break;
	}
	
	return res;
}

static	avr_io_t	_io = {
	.kind = "eeprom",
	.run = avr_eeprom_run,
	.ioctl = avr_eeprom_ioctl,
};

void avr_eeprom_init(avr_t * avr, avr_eeprom_t * p)
{
	p->io = _io;
//	printf("%s init (%d bytes) EEL/H:%02x/%02x EED=%02x EEC=%02x\n",
//			__FUNCTION__, p->size, p->r_eearl, p->r_eearh, p->r_eedr, p->r_eecr);

	p->eeprom = malloc(p->size);
	memset(p->eeprom, 0xff, p->size);
	
	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->ready);

	avr_register_io_write(avr, p->r_eecr, avr_eeprom_write, p);
}

