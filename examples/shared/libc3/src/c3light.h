/*
	c3light.h

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

 	This file is part of libc3.

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


#ifndef __C3LIGHT_H___
#define __C3LIGHT_H___

#include "c3geometry.h"

enum {
	C3_LIGHT_TYPE = C3_TYPE('l','i','g','h'),
};

typedef struct c3light_t {
	c3geometry_t	geometry;
	c3apiobject_t	light_id;
	int				context_view_index;
	c3vec4			position;
	c3vec3			direction;
	c3f				fov;
	struct {
		c3colorf_t	ambiant;
		c3colorf_t	specular;
	} color;
} c3light_t, *c3light_p;

c3light_p
c3light_new(
		struct c3object_t * o /* = NULL */);

c3light_p
c3light_init(
		c3light_p l,
		struct c3object_t * o /* = NULL */);

#endif /* __C3LIGHT_H___ */
