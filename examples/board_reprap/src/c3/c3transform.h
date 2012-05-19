/*
	c3transform.h

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


#ifndef __C3TRANSFORM_H___
#define __C3TRANSFORM_H___

#include "c3/c3algebra.h"
#include "c_utils.h"

typedef struct c3transform_t {
	str_p name;
	struct c3object_t * object;
	c3mat4	matrix;
} c3transform_t, *c3transform_p;

c3transform_p
c3transform_new(
		struct c3object_t * o );
void
c3transform_set(
		c3transform_p t,
		c3mat4p m );
void
c3transform_dispose(
		c3transform_p t );

DECLARE_C_ARRAY(c3transform_p, c3transform_array, 4);
IMPLEMENT_C_ARRAY(c3transform_array);

#endif /* __C3TRANSFORM_H___ */
