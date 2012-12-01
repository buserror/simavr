/*
	sim_core.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
	Copyright 2013 Michel Hughes <squirmyworms@embarqmail.com>

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

#ifndef _SIM_CORE_V2_MEM_FUNCS_H
#define _SIM_CORE_V2_MEM_FUNCS_H

static inline uint8_t _avr_get_r(avr_t* avr, uint8_t reg) {
	return(avr->data[reg]);
}

static inline void _avr_data_write(avr_t* avr, uint16_t addr, uint8_t data) {
	avr->data[addr]=data;
}

static inline void _avr_set_r(avr_t* avr, uint8_t reg, uint8_t v) {
	avr->data[reg]=v;
}

static inline uint16_t _avr_bswap16_le(uint16_t v) { return(v); }
static inline uint16_t _avr_bswap16_be(uint16_t v) { return(((v&0xff00)>>8)|((v&0x00ff)<<8)); }
//static inline uint16_t _avr_bswap16_be(uint16_t v) { return(__builtin_bswap16(v)); }

static inline uint16_t _avr_fetch16(void* p, uint16_t addr) {
	
	return(*((uint16_t*)&((uint8_t *)p)[addr]));
}

static inline void _avr_store16(void*p, uint16_t addr, uint16_t data) {
	*((uint16_t*)&((uint8_t *)p)[addr])=data;
}

static inline uint16_t _avr_flash_read16le(avr_t* avr, uint16_t addr) {
	return(_avr_bswap16_le(_avr_fetch16(avr->flash, addr)));
}

static inline uint16_t _avr_flash_read16be(avr_t* avr, uint16_t addr) {
	return(_avr_bswap16_be(_avr_fetch16(avr->flash, addr)));
}


static inline uint16_t _avr_data_read16(avr_t* avr, uint16_t addr) {
	return(_avr_fetch16(avr->data, addr));
}

static inline uint16_t _avr_get_r16(avr_t* avr, uint8_t addr) {
	return(_avr_data_read16(avr, addr));
}

static inline uint16_t _avr_data_read16le(avr_t* avr, uint16_t addr) {
	return(_avr_bswap16_le(_avr_fetch16(avr->data, addr)));
}

static inline uint16_t _avr_get_r16le(avr_t* avr, uint8_t addr) {
	return(_avr_data_read16le(avr, addr));
}

static inline void _avr_data_write16(avr_t* avr, uint16_t addr, uint16_t data) {
	_avr_store16(avr->data, addr, data);
}

static inline void _avr_set_r16(avr_t* avr, uint8_t addr, uint16_t data) {
	_avr_data_write16(avr, addr, data);
}

static inline void _avr_data_write16le(avr_t* avr, uint16_t addr, uint16_t data) {
	_avr_store16(avr->data, addr, _avr_bswap16_le(data));
}

static inline void _avr_set_r16le(avr_t* avr, uint8_t addr, uint16_t data) {
	_avr_data_write16(avr, addr, data);
}

static inline uint16_t _avr_data_read16be(avr_t* avr, uint16_t addr) __attribute__ ((unused));
static inline uint16_t _avr_data_read16be(avr_t* avr, uint16_t addr) {
	return(_avr_bswap16_be(_avr_fetch16(avr->data, addr)));
}
#endif

