/*
	c3quaternion.h

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


#ifndef __C3QUATERNION_H___
#define __C3QUATERNION_H___

#include <stdbool.h>
#include "c3algebra.h"

typedef struct c3quat {
  c3vec3  v;  	/* vector component */
  c3f s;  		/* scalar component */
} c3quat, *c3quatp;

c3quat
c3quat_new();
c3quat
c3quat_identity();

c3quat
c3quatf(
		const c3f x,
		const c3f y,
		const c3f z,
		const c3f w);
c3quat
c3quat_vec3(
		const c3vec3 v,
		const c3f s);
c3quat
c3quat_vec4(
		const c3vec4 v);

c3quat
c3quat_double(
		const double *d);

c3quat
c3quat_add(
		const c3quat a,
		const c3quat b);
c3quat
c3quat_sub(
		const c3quat a,
		const c3quat b);
c3quat
c3quat_minus(
		const c3quat a );

c3quat
c3quat_mul(
		const c3quat a,
		const c3quat b);

c3mat4
c3quat_to_mat4(
		const c3quat a );

c3quatp
c3quat_set_angle(
		c3quatp a,
		c3f f);
c3quatp
c3quat_scale_angle(
		c3quatp a,
		c3f f);
c3f
c3quat_get_angle(
		const c3quatp a);
c3vec3
c3quat_get_axis(
		c3quatp a);

#endif /* __C3QUATERNION_H___ */
