/*
	c_utils.h

	Copyright 2008-11 Michel Pollet <buserror@gmail.com>

	This program cross examines a root filesystem, loads all the elf
	files it can find, see what other library they load and then
	find the orphans. In then remove the orphans as "user" for it's
	dependencies and continues removing until everything has at least
	one user, OR is a program itself (ie, not a shared library)

	cross_linker is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cross_linker is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cross_linker.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __C_UTILS_H__
#define __C_UTILS_H__

#ifndef NO_ALLOCA
#include <alloca.h>
#endif
#include "c_array.h"

/********************************************************************
 * CRC16
 ********************************************************************/

static uint8_t _crc16_lh[16] = { 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60,
        0x70, 0x81, 0x91, 0xA1, 0xB1, 0xC1, 0xD1, 0xE1, 0xF1 };
static uint8_t _crc16_ll[16] = { 0x00, 0x21, 0x42, 0x63, 0x84, 0xA5, 0xC6,
        0xE7, 0x08, 0x29, 0x4A, 0x6B, 0x8C, 0xAD, 0xCE, 0xEF };

static uint16_t crc16_u4(uint16_t crc, uint8_t val)
{
	uint8_t h = crc >> 8, l = crc & 0xff;
	uint8_t t = (h >> 4) ^ val;

	// Shift the CRC Register left 4 bits
	h = (h << 4) | (l >> 4);
	l = l << 4;
	// Do the table lookups and XOR the result into the CRC Tables
	h = h ^ _crc16_lh[t];
	l = l ^ _crc16_ll[t];
	return (h << 8) | l;
}

static uint16_t crc16_update(uint16_t crc, uint8_t val)
{
	crc = crc16_u4(crc, val >> 4); // High nibble first
	crc = crc16_u4(crc, val & 0x0F); // Low nibble
	return crc;
}

static uint16_t crc16_string(char * str)
{
	uint16_t crc = 0xffff;
	while (*str)
		crc = crc16_update(crc, *str++);
	return crc;
}

/********************************************************************
 * Hashed strings
 ********************************************************************/

#include <string.h>
typedef struct str_t {
	uint32_t hash : 16, rom : 1,  len : 15;
	char str[0];
} str_t, *str_p;

static inline str_p str_new_i(const char *s, void * (*_alloc)(size_t))
{
	int l = s ? strlen(s) : 0;
	str_p r = (str_p)_alloc(sizeof(*r) + l + 1);
	r->hash = 0; r->len = l;
	if (s)
		strcpy(r->str, s);
	return r;
}
static inline void str_free(str_p s)
{
	if (s && !s->rom)
		free(s);
}
static inline str_p str_new(const char *s)
{
	return str_new_i(s, malloc);
}
static inline str_p str_anew(const char *s)
{
	str_p r = str_new_i(s, alloca);
	r->rom = 1;
	return r;
}
static inline str_p str_dup(const str_p s)
{
	size_t l = sizeof(*s) + s->len + 1;
	str_p r = (str_p)malloc(l);
	memcpy(r, s, l);
	return r;
}
#ifndef NO_ALLOCA
static inline str_p str_adup(const str_p s)
{
	size_t l = sizeof(*s) + s->len + 1;
	str_p r = (str_p)alloca(l);
	memcpy(r, s, l);
	r->rom = 1;
	return r;
}
#endif
static inline uint16_t str_hash(str_p s)
{
	if (!s->hash) s->hash = crc16_string(s->str);
	return s->hash;
}
static inline int str_cmp(str_p s1, str_p s2)
{
	if (s1 == s2) return 1;
	if (s1->len != s2->len) return 1;
	str_hash(s1);
	str_hash(s2);
	return s1->hash == s2->hash ? strcmp(s1->str, s2->str) : 1;
}

/********************************************************************
 * Hash table of strings. Key/value pair
 ********************************************************************/

typedef struct str_hashval_t {
	str_p key;
	void * val;
} str_hashval_t;

DECLARE_C_ARRAY(str_hashval_t, hashval_array, 16);
IMPLEMENT_C_ARRAY(hashval_array);

#ifndef STR_HASH_SIZE
#define STR_HASH_SIZE	512	// use 9 bits of the 16 of the CRC
#endif
/* uses bins to store the strings as per their hash values */
typedef struct str_hash_t {
	hashval_array_t bin[STR_HASH_SIZE];
} str_hash_t, *str_hash_p;

void
str_hash_init(
		str_hash_p h);
void
str_hash_add(
	str_hash_p h, 
	str_p k, 
	void * v);

void *
str_hash_lookup(
	str_hash_p h, 
	str_p k );

#endif /* __C_UTILS_H__ */
