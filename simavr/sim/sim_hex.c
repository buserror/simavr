/*
	sim_hex.c

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
#include "sim_hex.h"
#include "sim_elf.h"

// friendly hex dump
void hdump(const char *w, uint8_t *b, size_t l)
{
	uint32_t i;
	if (l < 16) {
		printf("%s: ",w);
		for (i = 0; i < l; i++) printf("%02x",b[i]);
	} else {
		printf("%s:\n",w);
		for (i = 0; i < l; i++) {
			if (!(i & 0x1f)) printf("    ");
			printf("%02x",b[i]);
			if ((i & 0x1f) == 0x1f) {
				printf(" ");
				printf("\n");
			}
		}
	}
	printf("\n");
}

    // decode line text hex to binary
int read_hex_string(const char * src, uint8_t * buffer, int maxlen)
{
    uint8_t * dst = buffer;
    int ls = 0;
    uint8_t b = 0;
    while (*src && maxlen) {
        char c = *src++;
        switch (c) {
            case 'a' ... 'f':   b = (b << 4) | (c - 'a' + 0xa); break;
            case 'A' ... 'F':   b = (b << 4) | (c - 'A' + 0xa); break;
            case '0' ... '9':   b = (b << 4) | (c - '0'); break;
            default:
                if (c > ' ') {
                    fprintf(stderr, "%s: huh '%c' (%s)\n", __FUNCTION__, c, src);
                    return -1;
                }
                continue;
        }
        if (ls & 1) {
            *dst++ = b; b = 0;
            maxlen--;
        }
        ls++;
    }

    return dst - buffer;
}

void
free_ihex_chunks(
		ihex_chunk_p chunks)
{
	if (!chunks)
		return;
	for (int i = 0; chunks[i].size; i++)
		if (chunks[i].data)
			free(chunks[i].data);
}

int
read_ihex_chunks(
		const char * fname,
		ihex_chunk_p * chunks )
{
	if (!fname || !chunks)
		return -1;
	FILE * f = fopen(fname, "r");
	if (!f) {
		perror(fname);
		return -1;
	}
	uint32_t segment = 0;	// segment address
	int chunk = 0, max_chunks = 0;
	*chunks = NULL;

	while (!feof(f)) {
		char line[128];
		if (!fgets(line, sizeof(line)-1, f))
			continue;
		if (line[0] != ':') {
			fprintf(stderr, "AVR: '%s' invalid ihex format (%.4s)\n", fname, line);
			break;
		}
		uint8_t bline[64];

		int len = read_hex_string(line + 1, bline, sizeof(bline));
		if (len <= 0)
			continue;

		uint8_t chk = 0;
		{	// calculate checksum
			uint8_t * src = bline;
			int tlen = len-1;
			while (tlen--)
				chk += *src++;
			chk = 0x100 - chk;
		}
		if (chk != bline[len-1]) {
			fprintf(stderr, "%s: %s, invalid checksum %02x/%02x\n", __FUNCTION__, fname, chk, bline[len-1]);
			break;
		}
		uint32_t addr = 0;
		switch (bline[3]) {
			case 0: // normal data
				addr = segment | (bline[1] << 8) | bline[2];
				break;
			case 1: // end of file - reset segment
				segment = 0;
				continue;
			case 2: // extended address 2 bytes
				segment = ((bline[4] << 8) | bline[5]) << 4;
				continue;
			case 4:
				segment = ((bline[4] << 8) | bline[5]) << 16;
				continue;
			default:
				fprintf(stderr, "%s: %s, unsupported check type %02x\n", __FUNCTION__, fname, bline[3]);
				continue;
		}
		if (chunk < max_chunks && addr != ((*chunks)[chunk].baseaddr + (*chunks)[chunk].size)) {
			if ((*chunks)[chunk].size)
				chunk++;
		}
		if (chunk >= max_chunks) {
			max_chunks++;
			/* Here we allocate and zero an extra chunk, to act as terminator */
			*chunks = realloc(*chunks, (1 + max_chunks) * sizeof(ihex_chunk_t));
			memset(*chunks + chunk, 0,
					(1 + (max_chunks - chunk)) * sizeof(ihex_chunk_t));
			(*chunks)[chunk].baseaddr = addr;
		}
		(*chunks)[chunk].data = realloc((*chunks)[chunk].data,
									(*chunks)[chunk].size + bline[0]);
		memcpy((*chunks)[chunk].data + (*chunks)[chunk].size,
				bline + 4, bline[0]);
		(*chunks)[chunk].size += bline[0];
	}
	fclose(f);
	return max_chunks;
}


uint8_t *
read_ihex_file(
		const char * fname, uint32_t * dsize, uint32_t * start)
{
	ihex_chunk_p chunks = NULL;
	int count = read_ihex_chunks(fname, &chunks);
	uint8_t * res = NULL;

	if (count > 0) {
		*dsize = chunks[0].size;
		*start = chunks[0].baseaddr;
		res = chunks[0].data;
		chunks[0].data = NULL;
	}
	if (count > 1) {
		fprintf(stderr, "AVR: '%s' ihex contains more chunks than loaded (%d)\n",
				fname, count);
	}
	free_ihex_chunks(chunks);
	return res;
}

/* Load a firmware file, ELF or HEX format, from filename, based at
 * loadBase, returning the data in *fp ready for loading into
 * the simulated MCU.  Progname is the current program name for error messages.
 *
 * Included here as it mostly specific to HEX files.
 */

void
sim_setup_firmware(const char * filename, uint32_t loadBase,
                   elf_firmware_t * fp, const char * progname)
{
	char * suffix = strrchr(filename, '.');

	if (suffix && !strcasecmp(suffix, ".hex")) {
		if (!(fp->mmcu[0] && fp->frequency > 0)) {
			printf("MCU type and frequency are not set "
					"when loading .hex file\n");
		}
		ihex_chunk_p chunk = NULL;
		int cnt = read_ihex_chunks(filename, &chunk);
		if (cnt <= 0) {
			fprintf(stderr,
					"%s: Unable to load IHEX file %s\n", progname, filename);
			exit(1);
		}
		printf("Loaded %d section(s) of ihex\n", cnt);

		for (int ci = 0; ci < cnt; ci++) {
			if (chunk[ci].baseaddr < (1*1024*1024)) {
				if (fp->flash) {
					printf("Ignoring chunk %d, "
						   "possible flash redefinition %08x, %d\n",
						   ci, chunk[ci].baseaddr, chunk[ci].size);
					free(chunk[ci].data);
					chunk[ci].data = NULL;
					continue;
				}
				fp->flash = chunk[ci].data;
				fp->flashsize = chunk[ci].size;
				fp->flashbase = chunk[ci].baseaddr;
				printf("Load HEX flash %08x, %d at %08x\n",
					   fp->flashbase, fp->flashsize, fp->flashbase);
			} else if (chunk[ci].baseaddr >= AVR_SEGMENT_OFFSET_EEPROM ||
					   (chunk[ci].baseaddr + loadBase) >=
							AVR_SEGMENT_OFFSET_EEPROM) {
				// eeprom!

				if (fp->eeprom) {

					// Converting ELF with .mmcu section will do this.

					printf("Ignoring chunk %d, "
						   "possible eeprom redefinition %08x, %d\n",
						   ci, chunk[ci].baseaddr, chunk[ci].size);
					free(chunk[ci].data);
					chunk[ci].data = NULL;
					continue;
				}
				fp->eeprom = chunk[ci].data;
				fp->eesize = chunk[ci].size;
				printf("Load HEX eeprom %08x, %d\n",
					   chunk[ci].baseaddr, fp->eesize);
			}
		}
                free(chunk);
	} else {
		if (elf_read_firmware(filename, fp) == -1) {
			fprintf(stderr, "%s: Unable to load firmware from file %s\n",
					progname, filename);
			exit(1);
		}
	}
}

#ifdef IHEX_TEST
// gcc -std=gnu99 -Isimavr/sim simavr/sim/sim_hex.c -o sim_hex -DIHEX_TEST -Dtest_main=main
int test_main(int argc, char * argv[])
{
	struct ihex_chunk_t chunk[4];

	for (int fi = 1; fi < argc; fi++) {
		int c = read_ihex_chunks(argv[fi], chunk, 4);
		if (c == -1) {
			perror(argv[fi]);
			continue;
		}
		for (int ci = 0; ci < c; ci++) {
			char n[96];
			sprintf(n, "%s[%d] = %08x", argv[fi], ci, chunk[ci].baseaddr);
			hdump(n, chunk[ci].data, chunk[ci].size);
		}
	}
}
#endif
