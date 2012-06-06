/*
	c3pixels.c

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

#include <stdlib.h>
#include <string.h>
#include "c3pixels.h"

c3pixels_p
c3pixels_new(
		uint32_t w,
		uint32_t h,
		int 	 psize /* in bytes */,
		size_t row,
		void * base)
{
	c3pixels_p p = malloc(sizeof(*p));
	c3pixels_init(p, w, h, psize, row, base);
	p->alloc = 1;
	return p;
}

c3pixels_p
c3pixels_init(
		c3pixels_p p,
		uint32_t w,
		uint32_t h,
		int 	 psize /* in bytes */,
		size_t row,
		void * base)
{
	memset (p, 0, sizeof(*p));
	p->w = w;
	p->h = h;
	p->row = row;
	p->psize = psize;
	p->base = base;
	c3pixels_alloc(p);
	return p;
}

void
c3pixels_dispose(
		c3pixels_p p )
{
	if (p->own && p->base)
		free(p->base);
	if (p->alloc)
		free(p);
	else
		memset(p, 0, sizeof(*p));
}

void
c3pixels_alloc(
		c3pixels_p p )
{
	if (p->base)
		return;
	p->base = malloc(p->row * p->h);
	p->own = p->base != NULL;
}

void
c3pixels_purge(
		c3pixels_p p )
{
	if (!p->base)
		return;
	if (p->own)
		free(p->base);
	p->own = 0;
	p->base = NULL;
}

void
c3pixels_zero(
		c3pixels_p p)
{
	if (!p->base)
		return;
	memset(p->base, 0, p->h * p->row);
}
