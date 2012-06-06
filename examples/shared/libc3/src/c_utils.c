/*
	c_utils.c

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

 	This file is part of libc3.

	libc3 is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	libc3 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with libc3.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "c_utils.h"

void
str_hash_init(str_hash_p h)
{
	memset(h, 0, sizeof(*h));
}

void
str_hash_add(
	str_hash_p h,
	str_p k,
	void * v)
{
	uint16_t hv = str_hash(k);
	hashval_array_p bin = &h->bin[hv & (STR_HASH_SIZE-1)];
	int inserti = bin->count;

	for (int i = 0; i < bin->count; i++)
		if (bin->e[i].key->hash >= hv) {
			inserti = i;
			break;
		}
	str_hashval_t n = { .key = str_dup(k), .val = v };
	hashval_array_insert(bin, inserti, &n, 1);
	return;
}

void *
str_hash_lookup(
	str_hash_p h,
	str_p k )
{
	uint16_t hv = str_hash(k);
	hashval_array_p bin = &h->bin[hv & (STR_HASH_SIZE-1)];

	for (int i = 0; i < bin->count; i++) {
		uint16_t h = bin->e[i].key->hash;
		if (h == hv && !str_cmp(k, bin->e[i].key))
			return bin->e[i].val;
		else if (h > hv)
			break;
	}
	return NULL;
}
