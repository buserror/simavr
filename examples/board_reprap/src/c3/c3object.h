/*
	c3object.h

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


#ifndef __C3OBJECT_H___
#define __C3OBJECT_H___

#include <stdbool.h>
#include "c3/c3transform.h"
#include "c3/c3geometry.h"
#include "c3/c3object_driver.h"

struct c3object_t;

DECLARE_C_ARRAY(struct c3object_t*, c3object_array, 4);

typedef struct c3object_t {
	str_p name;
	int	dirty : 1;
	struct c3object_t * parent;
	c3object_driver_p	driver;
	c3transform_array_t	transform;
	c3object_array_t	objects;
	c3geometry_array_t	geometry;
} c3object_t, *c3object_p;

c3object_p
c3object_new(
		c3object_p o /* = NULL */);
void
c3object_dispose(
		c3object_p o);
void
c3object_clear(
		c3object_p o);

c3object_p
c3object_init(
		c3object_p o /* = NULL */,
		c3object_p parent);
void
c3object_set_dirty(
		c3object_p o,
		bool dirty);
void
c3object_add_geometry(
		c3object_p o,
		c3geometry_p g);
void
c3object_add_object(
		c3object_p o,
		c3object_p sub);
c3transform_p
c3object_add_transform(
		c3object_p o );
void
c3object_get_geometry(
		c3object_p o,
		c3geometry_array_p array );
void
c3object_project(
		c3object_p o,
		const c3mat4p m);

IMPLEMENT_C_ARRAY(c3object_array);

#endif /* __C3OBJECT_H___ */
