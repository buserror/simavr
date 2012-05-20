/*
	c3driver_object.h

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


#ifndef __C3DRIVER_OBJECT_H___
#define __C3DRIVER_OBJECT_H___

#include "c3/c3driver.h"

struct c3object_t;
struct c3geometry_array_t;
union c3mat4;

typedef struct c3driver_object_t {
	/*
	 * Delete any object related to this object, geometry etc
	 * The object will still exist, just empty
	 */
	void (*clear)(
			struct c3object_t * object,
			const struct c3driver_object_t * d);
	/*
	 * Dispose of the remaining memory for an object, detaches it
	 * and frees remaining traces of it
	 */
	void (*dispose)(
			struct c3object_t * object,
			const struct c3driver_object_t * d);
	/*
	 * Adds sub objects geometry and self geometry to array 'out'
	 */
	void (*get_geometry)(
			struct c3object_t * object,
			const struct c3driver_object_t * d,
			struct c3geometry_array_t * out);
	/*
	 * Reproject geometry along matrix 'mat', applies our own
	 * transform and call down the chain for sub-objects
	 */
	void (*project)(
			struct c3object_t * object,
			const struct c3driver_object_t * d,
			union c3mat4 * mat);
} c3driver_object_t, *c3driver_object_p;


#endif /* __C3DRIVER_OBJECT_H___ */
