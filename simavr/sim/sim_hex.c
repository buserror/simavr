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
    while (*src && maxlen--) {
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
        }
        ls++;
    }

    return dst - buffer;
}

uint8_t * read_ihex_file(const char * fname, uint32_t * dsize, uint32_t * start)
{
	if (!fname || !dsize)
		return NULL;
	FILE * f = fopen(fname, "r");
	if (!f) {
		perror(fname);
		return NULL;
	}
	uint8_t * res = NULL;
	uint32_t size = 0;
	uint32_t base = ~0;

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
		if (bline[3] != 0) {
			if (bline[3] != 1) {
				fprintf(stderr, "%s: %s, unsupported check type %02x\n", __FUNCTION__, fname, bline[3]);
				break;
			}
			continue;
		}
		uint16_t addr = (bline[1] << 8) | bline[2];
		if (base == ~0) {
			base = addr;	// stadt address
		}
		if (addr != base + size) {
			fprintf(stderr, "%s: %s, offset out of bounds %04x expected %04x\n", __FUNCTION__, fname, addr, base+size);
			break;
		}
		res = realloc(res, size + bline[0]);
		memcpy(res + size, bline + 4, bline[0]);
		size += bline[0];
	}
	*dsize = size;
	if (start)
		*start = base;
	fclose(f);
	return res;
}


int read_ihex_chunks(const char * fname, struct ihex_chunk_t * chunks, int max_chunks)
{
	if (!fname || !chunks || !max_chunks)
		return -1;
	memset((void*)chunks, 0, sizeof(chunks[0]) * max_chunks);
	FILE * f = fopen(fname, "r");
	if (!f) {
		perror(fname);
		return -1;
	}
	uint32_t segment = 0;	// segment address
	int chunk = 0;
	chunks[0].baseaddr = ~0;

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
			case 1: // end of file
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
		if (addr != chunks[chunk].baseaddr + chunks[chunk].size) {
			if (chunks[chunk].size)
				chunk++;
			chunks[chunk].baseaddr = addr;
		}
		chunks[chunk].data = realloc(chunks[chunk].data, chunks[chunk].size + bline[0]);
		memcpy(chunks[chunk].data + chunks[chunk].size, bline + 4, bline[0]);
		chunks[chunk].size += bline[0];
	}
	if (chunks[chunk].size)
		chunk++;
	fclose(f);
	return chunk;
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
