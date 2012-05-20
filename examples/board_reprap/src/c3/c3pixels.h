/*
	c3pixels.h

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

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


#ifndef __C3PIXELS_H___
#define __C3PIXELS_H___

#include <stdint.h>

typedef struct c3pixels_t {
	uint32_t w, h;	// width & height in pixels
	size_t row;		// size of one row in bytes
	void * base;	// base address

	union {
		struct {
			uint32_t	own : 1,	// is the base our own to delete
				alloc : 1,			// is the c3pixelsp our own to delete
				dirty : 1,			// pixels have been changed
				psize : 4,			// pixel size in byte
				format : 8;			// not used internally
		};
		uint32_t flags;
	};
	int		refCount;	// TODO: Implement reference counting ?
} c3pixels_t, *c3pixelsp;

//! Allocates a new c3pixels, also allocates the pixels if row == NULL
c3pixelsp
c3pixels_new(
		uint32_t w,
		uint32_t h,
		int 	 psize /* in bytes */,
		size_t row,
		void * base);

//! Initializes p, also allocates the pixels if row == NULL
c3pixelsp
c3pixels_init(
		c3pixelsp p,
		uint32_t w,
		uint32_t h,
		int 	 psize /* in bytes */,
		size_t row,
		void * base);

//! Dispose of the pixels, and potentially p if it was allocated with c3pixels_new
void
c3pixels_dispose(
		c3pixelsp p );

//! Disposes of the pixels, only
void
c3pixels_purge(
		c3pixelsp p );

//! (Re)allocate pixels if pixels had been purged
void
c3pixels_alloc(
		c3pixelsp p );

//! Get a pixel address
static inline void *
c3pixels_get(
		c3pixelsp p,
		int x, int y)
{
	return ((uint8_t*)p->base) + (y * p->row) + (x * p->psize);
}

//! Zeroes the pixels
void
c3pixels_zero(
		c3pixelsp p);

#endif /* __C3PIXELS_H___ */
