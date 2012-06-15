/*
	c3object.h

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


#ifndef __C3OBJECT_H___
#define __C3OBJECT_H___

#include <stdbool.h>
#include "c3transform.h"
#include "c3geometry.h"

struct c3object_t;

DECLARE_C_ARRAY(struct c3object_t*, c3object_array, 4);

//! c3object is a container for child object, and geometry
/*!
 * a c3object is a node in a c3object tree, it contains sub-objects and/or
 * geometry. It also contains it's own list of transform matrices, so can
 * be seen as a "anchor" that can be moved around and where you can
 * attach other objects or geometry.
 *
 * An object has a notion of 'dirty bit' -- something that signals that
 * something has changed and deserved reprojection. the dirty bit
 * is propagated upward when 1 (up to the root object) and downward when 0
 * (to allow clearing the bit on a subtree)
 */
typedef struct c3object_t {
	str_p 				name;	//! optional name
	int					dirty : 1, 
						hidden : 8 /* hidden bit mask, related to c3context's views */;
	struct c3context_t * context; //! context this object is attached to
	struct c3object_t * parent;		//! Parent object
	const struct c3driver_object_t ** driver;	//! Driver stack

	c3mat4				world;		// calculated world coordinates
	c3transform_array_t	transform;
	c3object_array_t	objects;	//! child object list
	c3geometry_array_t	geometry;	//! Object geometri(es)
} c3object_t, *c3object_p;

//! Allocates and initialize an emty object, attaches it to parent 'o'
c3object_p
c3object_new(
		c3object_p o /* = NULL */);
//! Disposes of everything under this object
void
c3object_dispose(
		c3object_p o);
//! Clears every sub-object, geometry, and transform, but do not dispose of o
void
c3object_clear(
		c3object_p o);
//! Initializes 'o' as a new object, attaches it to parent (optional)
c3object_p
c3object_init(
		c3object_p o,
		c3object_p parent /* = NULL */);
//! sets the dirty bit for 'o' and related tree
/*!
 * When dirty is 1, sets the dirty bit of this object and all the parent
 * objects up to the root object.
 * When dirty is 0, clear the dirty bit of this object, and all the
 * sub objects.
 */
void
c3object_set_dirty(
		c3object_p o,
		bool dirty);
//! Adds a new geometry g to object o
void
c3object_add_geometry(
		c3object_p o,
		c3geometry_p g);
//! Adds a new sub-object sub to object o
void
c3object_add_object(
		c3object_p o,
		c3object_p sub);
//! Adds a new transform matrix, initialized as identity
c3transform_p
c3object_add_transform(
		c3object_p o );
//! Iterates all the sub-objects and collects all the geometries
/*!
 * This call iterates the sub-objects and collects all their 'projected'
 * geometry, and add them to the array
 */
void
c3object_get_geometry(
		c3object_p o,
		c3geometry_array_p array );
//! Project object 'o' using it's own transformations, relative to matrix 'm'
/*!
 * Multiply this objects transformation(s) to matrix 'm' and calls
 * reprojects the geometries using that matrix as an anchor. also call
 * recursively to sub-objects to follow the projection down.
 */
void
c3object_project(
		c3object_p o,
		const c3mat4p m);

IMPLEMENT_C_ARRAY(c3object_array);

#endif /* __C3OBJECT_H___ */
