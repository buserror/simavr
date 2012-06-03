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

/*
 * c3geometry is a structure containing one set of vertices and various
 * bits related to it. Ultimately it contains a pre-cached projected
 * version of the vertices that the drawing code can use directly.
 * c3geometry is aways attached to a c3object as a parent.
 */

#ifndef __C3GEOMETRY_H___
#define __C3GEOMETRY_H___

#include "c3algebra.h"
#include "c_utils.h"

typedef c3vec3 c3vertex, *c3vertex_p;
typedef c3vec4 c3colorf, *c3colorf_p;
typedef c3vec2 c3tex, *c3tex_p;

struct c3object_t;
struct c3pixels_t;
struct c3program_t;

DECLARE_C_ARRAY(c3vertex, c3vertex_array, 16, uint32_t bid);
DECLARE_C_ARRAY(c3tex, c3tex_array, 16, uint32_t bid);
DECLARE_C_ARRAY(c3colorf, c3colorf_array, 16, uint32_t bid);

//! Geometry material. TODO: Beef up. Add vertex/fragment programs..
typedef struct c3material_t {
	c3colorf	color;
	struct c3pixels_t * texture;
	struct c3program_t * program;
	struct {
		uint32_t src, dst;
	} blend;
} c3material_t;

//! Bounding box. TODO: Move to a separate file?
typedef struct c3bbox_t {
	c3vec3	min, max;
} c3bbox_t;

//! Generic geometry type
enum {
	C3_RAW_TYPE = 0,
	C3_LINES_TYPE,
	C3_TRIANGLE_TYPE,
	C3_TEXTURE_TYPE,
};

/*!
 * geometry type.
 * The type is used as non-opengl description of what the geometry
 * contains, like "texture", and the subtype can be used to store the
 * real format of the vertices. like GL_LINES etc
 */
typedef union c3geometry_type_t {
	struct  { uint32_t type : 16, subtype : 16; };
	uint32_t value;
} c3geometry_type_t;

/*!
 * Geometry object. Describes a set of vertices, texture coordinates,
 * normals, colors and material
 * The projection is not set here, a geometry is always attached to a
 * c3object that has the projection
 */
typedef struct c3geometry_t {
	c3geometry_type_t	type;	// C3_TRIANGLE_TYPE, GL_LINES etc
	int					dirty : 1,
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

	/*
	 * Some shared attributes
	 */
	union {
		struct {
			float width;
		} line;
	};
} c3geometry_t, *c3geometry_p;

DECLARE_C_ARRAY(c3geometry_p, c3geometry_array, 4);

//! Allocates a new geometry, init it, and attached it to parent 'o' (optional)
c3geometry_p
c3geometry_new(
		c3geometry_type_t type,
		struct c3object_t * o /* = NULL */);
//! Init an existing new geometry, and attached it to parent 'o' (optional)
c3geometry_p
c3geometry_init(
		c3geometry_p g,
		c3geometry_type_t type,
		struct c3object_t * o /* = NULL */);
//! Disposes (via the driver interface) the geometry
void
c3geometry_dispose(
		c3geometry_p g);

//! Prepares a geometry. 
/*!
 * The project phase is called only when the container object is 'dirty'
 * for example if it's projection has changed.
 * The project call is responsible for reprojecting the geometry and that
 * sort of things
 */
void
c3geometry_project(
		c3geometry_p g,
		c3mat4p m);

//! Draw the geometry
/*
 * Called when drawing the context. Typicaly this calls the geometry 
 * driver, which in turn will call the 'context' draw method, and the 
 * application to draw this particular geometry
 */
void
c3geometry_draw(
		c3geometry_p g );


//! allocate (if not there) and return a custom driver for this geometry
/*!
 * Geometries come with a default, read only driver stack.. It is a constant
 * global to save memory for each of the 'generic' object.
 * This call will duplicate that stack and allocate (if not there) a read/write
 * empty driver that the application can use to put their own, per object,
 * callback. For example you can add your own project() or draw() function
 * and have it called first
 */
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
	c3geometry_type_t r;// = { .type = type, .subtype = subtype }; // older gcc <4.6 doesn't like this
	r.type = type; r.subtype = subtype;
	return r;
}

#endif /* __C3GEOMETRY_H___ */
