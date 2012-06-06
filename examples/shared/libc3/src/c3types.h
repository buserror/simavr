/*
	c3types.h

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


#ifndef __C3TYPES_H___
#define __C3TYPES_H___

#include <stdint.h>
#include "c3algebra.h"

typedef c3vec3 c3vertex_t, *c3vertex_p;
typedef c3vec4 c3colorf_t, *c3colorf_p;
typedef c3vec2 c3tex_t, *c3tex_p;
typedef uint16_t	c3index_t, *c3index_p;

/* this type is used to store an API object (texture id etc
 * it is made to force a cast in most cases as OpenGL uses integers
 * for object ids
 */
typedef void * c3apiobject_t;

//! Bounding box
typedef struct c3bbox_t {
	c3vec3	min, max;
} c3bbox_t;

#endif /* __C3TYPES_H___ */
