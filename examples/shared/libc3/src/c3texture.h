/*
	c3texture.h

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


#ifndef __C3TEXTURE_H___
#define __C3TEXTURE_H___

#include "c3geometry.h"
#include "c3pixels.h"

enum {
	C3_TEXTURE_TYPE = C3_TYPE('t','e','x','u'),
};

typedef struct c3texture_t {
	c3geometry_t	geometry;
	c3vec2 size;	// quad size
} c3texture_t, *c3texture_p;

c3texture_p
c3texture_new(
		struct c3object_t * parent /* = NULL */);
c3texture_p
c3texture_init(
		c3texture_p t,
		struct c3object_t * parent /* = NULL */);
void
c3texture_resize(
		c3texture_p t,
		c3vec2 size );

#endif /* __C3TEXTURE_H___ */
