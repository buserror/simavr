/*
	c3geometry.h

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


#ifndef __C3GEOMETRY_H___
#define __C3GEOMETRY_H___

#include "c3/c3algebra.h"
#include "c_utils.h"

typedef c3vec3 c3vertex;
typedef c3vec4 c3colorf;
typedef c3vec2 c3tex;

struct c3object_t;

DECLARE_C_ARRAY(c3vertex, c3vertex_array, 16);
DECLARE_C_ARRAY(c3tex, c3tex_array, 16);
DECLARE_C_ARRAY(c3colorf, c3colorf_array, 16);

typedef struct c3material_t {
	c3colorf	color;
} c3material_t;

typedef struct c3geometry_t {
	int	type;	// GL_LINES etc
	int	dirty : 1;
	str_p name;
	c3material_t		mat;
	struct c3object_t * object;
	c3vertex_array_t 	vertice;
	c3tex_array_t		textures;
	c3colorf_array_t	colorf;

	// projected version of the vertice
	c3vertex_array_t 	projected;
} c3geometry_t, *c3geometry_p;

DECLARE_C_ARRAY(c3geometry_p, c3geometry_array, 4);

c3geometry_p
c3geometry_new(
		int type,
		struct c3object_t * o /* = NULL */);
void
c3geometry_dispose(
		c3geometry_p g);

IMPLEMENT_C_ARRAY(c3geometry_array);
IMPLEMENT_C_ARRAY(c3vertex_array);
IMPLEMENT_C_ARRAY(c3tex_array);
IMPLEMENT_C_ARRAY(c3colorf_array);

#endif /* __C3GEOMETRY_H___ */
