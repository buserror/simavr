/*
	sim_elf.h

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

#ifndef ELF_H_
#define ELF_H_

#include "avr_mcu_section.h"

#ifndef ELF_SYMBOLS
#define ELF_SYMBOLS 1
#endif

#if ELF_SYMBOLS
#include "sim_avr.h"
#endif

typedef struct elf_firmware_t {
	struct avr_mcu_t mmcu;
	uint8_t * flash;
	uint32_t flashsize;
	uint32_t datasize;
	uint32_t bsssize;
	// read the .eeprom section of the elf, too
	uint8_t * eeprom;
	uint32_t eesize;

#if ELF_SYMBOLS
	avr_symbol_t **  codeline;
	uint32_t		codesize;	// in elements
#endif
} elf_firmware_t ;

int elf_read_firmware(const char * file, elf_firmware_t * firmware);

#endif /* ELF_H_ */
