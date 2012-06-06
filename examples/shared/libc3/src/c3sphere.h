/*
	c3sphere.h

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


#ifndef __C3SPHERE_H___
#define __C3SPHERE_H___

#include "c3algebra.h"

enum {
	C3_SPHERE_TYPE = C3_TYPE('s','p','h','e'),
};

struct c3geometry_t *
c3sphere_uv(
		struct c3object_t * parent,
		c3vec3 center,
		c3f radius,
		int rings,
		int sectors );

#endif /* __C3SPHERE_H___ */
