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

#ifndef __SIM_ELF_H__
#define __SIM_ELF_H__

#include "avr/avr_mcu_section.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ELF_SYMBOLS
#define ELF_SYMBOLS 1
#endif

/* these are the addresses the gnu linker uses to
 * "fake" a non-Harvard addressing space for the AVR
 */
#define AVR_SEGMENT_OFFSET_FLASH 0
#define AVR_SEGMENT_OFFSET_DATA   0x00800000
#define AVR_SEGMENT_OFFSET_EEPROM 0x00810000

#include "sim_avr.h"

typedef struct elf_firmware_t {
	char  mmcu[64];
	uint32_t	frequency;
	uint32_t	vcc,avcc,aref;

	char		tracename[128];	// trace filename
	uint32_t	traceperiod;
	int			tracecount;
	struct {
		uint8_t kind;
		uint8_t mask;
		uint16_t addr;
		char	name[64];
	} trace[32];

	struct {
		char port;
		uint8_t mask, value;
	} external_state[8];

	// register to listen to for commands from the firmware
	uint16_t	command_register_addr;
	uint16_t	console_register_addr;

	uint32_t	flashbase;	// base address
	uint8_t * 	flash;
	uint32_t	flashsize;
	uint32_t 	datasize;
	uint32_t 	bsssize;
	// read the .eeprom section of the elf, too
	uint8_t * 	eeprom;
	uint32_t 	eesize;
	uint8_t *	fuse;
	uint32_t	fusesize;
	uint8_t *	lockbits;

#if ELF_SYMBOLS
	avr_symbol_t **  symbol;
	uint32_t	symbolcount;
	uint32_t	highest_data_symbol;
	char *		dwarf_file;	// Must be dynamically allocated.
#endif
} elf_firmware_t ;

/* The structure *firmware must be pre-initialised to zero, then optionally
 * with tracing and VCD information.
 */

int
elf_read_firmware(
	const char * file,
	elf_firmware_t * firmware);

void
avr_load_firmware(
	avr_t * avr,
	elf_firmware_t * firmware);

// DWARF, not ELF, but a separate header for this would be silly.

int avr_read_dwarf(avr_t *avr, const char *filename);

#ifdef __cplusplus
};
#endif

#endif /*__SIM_ELF_H__*/
