/*
	sim_hex.h

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


#ifndef __SIM_HEX_H___
#define __SIM_HEX_H___

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Load a firmware file, ELF or HEX format, ready for use.

struct elf_firmware_t;                          // Predeclaration ...

void
sim_setup_firmware(
				   const char * filename,       // Firmware file
				   uint32_t loadBase,           // Base of load region
				   struct elf_firmware_t * fp,  // Data returned here
				   const char * progname);      // For error messages.

// parses a hex text string 'src' of at max 'maxlen' characters, decodes it into 'buffer'
int
read_hex_string(
		const char * src,
		uint8_t * buffer,
		int maxlen);

// a .hex file chunk (base address + size)
typedef struct ihex_chunk_t {
	uint32_t baseaddr;	// offset it started at in the .hex file
	uint8_t * data;		// read data
	uint32_t size;		// read data size
} ihex_chunk_t, *ihex_chunk_p;

/*
 * Read a .hex file, detects the various different chunks in it from their starting
 * addresses and allocate an array of ihex_chunk_t returned in 'chunks'.
 * Returns the number of chunks found, or -1 if an error occurs.
 */
int
read_ihex_chunks(
		const char * fname,
		ihex_chunk_p * chunks );
/* Frees previously allocated chunks */
void
free_ihex_chunks(
		ihex_chunk_p chunks);

// reads IHEX file 'fname', puts it's decoded size in *'dsize' and returns
// a newly allocated buffer with the binary data (or NULL, if error)
uint8_t *
read_ihex_file(
		const char * fname,
		uint32_t * dsize,
		uint32_t * start);

// hex dump from pointer 'b' for 'l' bytes with string prefix 'w'
void
hdump(
		const char *w,
		uint8_t *b, size_t l);

#ifdef __cplusplus
};
#endif

#endif /* __SIM_HEX_H___ */
