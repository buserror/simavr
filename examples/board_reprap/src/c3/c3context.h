/*
	c3context.h

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


#ifndef __C3CONTEXT_H___
#define __C3CONTEXT_H___

#include "c3/c3algebra.h"
#include "c3/c3geometry.h"


typedef struct c3context_t {
	c3vec2	size;
	struct c3object_t * root;
	c3geometry_array_t	projected;

	const struct c3driver_context_t ** driver;
} c3context_t, *c3context_p;

c3context_p
c3context_new(
		int w,
		int h);

c3context_p
c3context_init(
		c3context_p c,
		int w,
		int h);

void
c3context_dispose(
		c3context_p c);

// Reproject geometry for dirty objects
void
c3context_prepare(
		c3context_p c);
void
c3context_draw(
		c3context_p c);

#endif /* __C3CONTEXT_H___ */
