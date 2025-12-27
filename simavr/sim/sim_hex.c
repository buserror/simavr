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
    int       ls = 0;
    uint8_t   b = 0;

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

#define ALLOCATION 512
#define INCREMENT  512

static void
read_ihex_chunks(
		const char  * fname,
		fw_chunk_t ** chunks_p )
{
	fw_chunk_t *chunk = *chunks_p;
	fw_chunk_t *backlink_p = chunk;
	int         len, allocation = 0;
	uint8_t     chk = 0;
	uint8_t     bline[272];

	if (!fname || !chunks_p)
		return;
	FILE * f = fopen(fname, "r");
	if (!f) {
		perror(fname);
		return;
	}
	uint32_t segment = 0;	// segment address

	while (!feof(f)) {
		uint32_t addr = 0;
		char     line[544];

		if (!fgets(line, sizeof(line)-1, f))
			break;
		if (line[0] != ':') {
			fprintf(stderr, "AVR: '%s' invalid ihex format (%.4s)\n",
					fname, line);
			break;
		}

		len = read_hex_string(line + 1, bline, sizeof(bline));
		if (len <= 0)
			continue;

		{	// calculate checksum
			uint8_t * src = bline;
			int tlen = len-1;

			chk = 0;
			while (tlen--)
				chk += *src++;
			chk = 0x100 - chk;
		}
		if (chk != bline[len-1]) {
			fprintf(stderr, "%s: %s, invalid checksum %02x/%02x\n",
					__FUNCTION__, fname, chk, bline[len-1]);
			break;
		}

		switch (bline[3]) {
			case 0: // normal data
				addr = segment | (bline[1] << 8) | bline[2];
				if (bline[0] == 0)
					continue; // No data!
				break;
			case 1: // "End of file" - reset segment as could be multi-part.
				segment = 0;
				continue;
			case 2: // extended address 2 bytes
				segment = ((bline[4] << 8) | bline[5]) << 4;
				continue;
			case 4:
				segment = ((bline[4] << 8) | bline[5]) << 16;
				continue;
			default:
				fprintf(stderr, "%s: %s, unsupported check type %02x\n",
						__FUNCTION__, fname, bline[3]);
				continue;
		}
		if (!chunk || (chunk->size && addr != chunk->addr + chunk->size)) {
			/* New chunk. */
			backlink_p = chunk;
			allocation = ALLOCATION - sizeof *chunk + 1;
			chunk = (fw_chunk_t *)malloc(ALLOCATION);
			*chunks_p = chunk;
			chunks_p = &chunk->next;
			chunk->type = UNKNOWN;
			chunk->addr = addr;
			chunk->fill_size = chunk->size = bline[0];
			chunk->next = NULL;
			memcpy(chunk->data, bline + 4, bline[0]);
			continue;
		}

		/* Continuation of chunk. */

		if (bline[0] > allocation - chunk->size) {
			/* Expand chunk. */

			allocation += INCREMENT;
			chunk = realloc(chunk, allocation + (sizeof *chunk - 1));

			/* Update the pointer in the previous list element or root */
			if ( backlink_p ) {
				backlink_p->next = chunk;
			} else {
				*chunks_p = chunk;
			}

			/* Refresh the pointer to the future chunk */
			chunks_p = &chunk->next;
		}
		memcpy(chunk->data + chunk->size, bline + 4, bline[0]);
		chunk->size += bline[0];
		chunk->fill_size = chunk->size;
	}
	fclose(f);
}

uint8_t *
read_ihex_file(
		const char * fname, uint32_t * dsize, uint32_t * start)
{
	fw_chunk_t *chunk = NULL, *next_chunk;
	uint8_t    *res = NULL;

	read_ihex_chunks(fname, &chunk);
	if (chunk) {
		*dsize = chunk->size;
		*start = chunk->addr;
		res = malloc((size_t)chunk->size);
		memcpy(res, chunk->data,  chunk->size);
	}
	if (chunk->next)
		fprintf(stderr, "%s: Additional data blocks were ignored.\n", fname);
	while(chunk) {
		next_chunk = chunk->next;
		free(chunk);
		chunk = next_chunk;
	}
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
	fw_chunk_t * chunks = NULL, *cp;
	char       * suffix = strrchr(filename, '.');

	if (!suffix || strcasecmp(suffix, ".hex")) {
		/* Not suffix .hex, try reading as an ELF file. */

		if (elf_read_firmware(filename, fp) == -1) {
			fprintf(stderr, "%s: Unable to load firmware from file %s\n",
					progname, filename);
			exit(1);
		}
		return;
	}

	if (!(fp->mmcu[0] && fp->frequency > 0))
		printf("MCU type and frequency are not set when loading .hex file\n");

	read_ihex_chunks(filename, &chunks);
	if (!chunks) {
		fprintf(stderr, "%s: Unable to load IHEX file %s\n",
				progname, filename);
		exit(1);
	}

	for (cp = chunks; cp; cp = cp->next) {
		if (cp->addr + loadBase < (1*1024*1024)) {
			cp->type = FLASH;
		} else if (cp->addr >= AVR_SEGMENT_OFFSET_EEPROM ||
				   (cp->addr + loadBase) >= AVR_SEGMENT_OFFSET_EEPROM) {
			// eeprom!
			if (cp->addr >= AVR_SEGMENT_OFFSET_EEPROM)
				cp->addr -= AVR_SEGMENT_OFFSET_EEPROM;
			cp->type = EEPROM;
		} else {
			cp->type = UNKNOWN;
		}
	}
	fp->chunks = chunks;
}

#ifdef IHEX_TEST
// gcc -std=gnu99 -Isimavr/sim simavr/sim/sim_hex.c -o sim_hex -DIHEX_TEST -Dtest_main=main -fsanitize=address -fno-omit-frame-pointer -O1 -g
int test_main(int argc, char * argv[])
{
	fw_chunk_t *chunks, *next_chunk;
	int         fi;

	for (fi = 1; fi < argc; fi++) {
		chunks = NULL;
		read_ihex_chunks(argv[fi], &chunks);
		if (!chunks) {
			perror(argv[fi]);
			continue;
		}
		for (int ci = 0; chunks; ++ci) {
			char n[96];

			snprintf(n, sizeof n, "%s[%d] = %08x",
					  argv[fi], ci, chunks->addr);
			hdump(n, chunks->data, chunks->size);
			next_chunk = chunks->next;
			free(chunks);
			chunks = next_chunk;
		}
	}
	return 0;
}
#endif
