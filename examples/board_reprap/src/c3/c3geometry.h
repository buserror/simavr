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
	uint32_t	texture;
} c3material_t;

typedef struct c3bbox_t {
	c3vec3	min, max;
} c3bbox_t;

enum {
	C3_RAW_TYPE = 0,
	C3_TRIANGLE_TYPE,
	C3_TEXTURE_TYPE,
};

typedef union {
	struct  { uint32_t type : 16, subtype : 16; };
	uint32_t value;
} c3geometry_type_t;

typedef struct c3geometry_t {
	c3geometry_type_t	type;	// GL_LINES etc
	int					dirty : 1,
						texture : 1,	// has a valid material.texture
						custom : 1;		// has a custom driver
	str_p 				name;	// optional
	c3material_t		mat;
	struct c3object_t * object;
	const struct c3driver_geometry_t ** driver;

	c3vertex_array_t 	vertice;
	c3tex_array_t		textures;
	c3colorf_array_t	colorf;
	c3vertex_array_t 	normals;

	// projected version of the vertice
	c3vertex_array_t 	projected;
	c3bbox_t			bbox;
} c3geometry_t, *c3geometry_p;

DECLARE_C_ARRAY(c3geometry_p, c3geometry_array, 4);

c3geometry_p
c3geometry_new(
		c3geometry_type_t type,
		struct c3object_t * o /* = NULL */);
c3geometry_p
c3geometry_init(
		c3geometry_p g,
		c3geometry_type_t type,
		struct c3object_t * o /* = NULL */);
void
c3geometry_dispose(
		c3geometry_p g);

void
c3geometry_prepare(
		c3geometry_p g );
void
c3geometry_draw(
		c3geometry_p g );

//! allocate (if not there) and return a custom driver for this geometry
struct c3driver_geometry_t *
c3geometry_get_custom(
		c3geometry_p g );

IMPLEMENT_C_ARRAY(c3geometry_array);
IMPLEMENT_C_ARRAY(c3vertex_array);
IMPLEMENT_C_ARRAY(c3tex_array);
IMPLEMENT_C_ARRAY(c3colorf_array);

static inline c3geometry_type_t
c3geometry_type(int type, int subtype)
{
	c3geometry_type_t r = { .type = type, . subtype = subtype };
	return r;
}

#endif /* __C3GEOMETRY_H___ */
