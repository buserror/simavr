/*
	c3sphere.c

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


#include <math.h>
#include "c3geometry.h"
#include "c3sphere.h"

c3geometry_p
c3sphere_uv(
		struct c3object_t * parent,
		c3vec3 center,
		c3f radius,
		int rings,
		int sectors )
{
	float const R = 1. / (float) (rings - 1);
	float const S = 1. / (float) (sectors - 1);

	c3geometry_p g = c3geometry_new(c3geometry_type(C3_SPHERE_TYPE, 0), parent);

	c3vertex_array_realloc(&g->vertice, rings * sectors);
	c3vertex_array_realloc(&g->normals, rings * sectors);
	c3tex_array_realloc(&g->textures, rings * sectors);
	c3indices_array_realloc(&g->indices, rings * sectors * 6);

	for (int r = 0; r < rings; r++)
		for (int s = 0; s < sectors; s++) {
			float const y = sin(-M_PI_2 + M_PI * r * R);
			float const x = cos(2 * M_PI * s * S) * sin(M_PI * r * R);
			float const z = sin(2 * M_PI * s * S) * sin(M_PI * r * R);

			c3tex_array_add(&g->textures, c3vec2f(s * S, r * R));
			c3vertex_array_add(&g->vertice,
					c3vec3_add(center, c3vec3f(x * radius, y * radius, z * radius)));
			c3vertex_array_add(&g->normals, c3vec3_normalize(c3vec3f(x, y, z)));
		}

	for (int r = 0; r < rings - 1; r++)
		for (int s = 0; s < sectors - 1; s++) {
			uint16_t i[6] = {
				r * sectors + (s + 1), r * sectors + s, (r + 1) * sectors + (s + 1),
				(r + 1) * sectors + (s + 1), r * sectors + s, (r + 1) * sectors + s,
			};
			c3indices_array_insert(&g->indices, g->indices.count, i, 6);
		}
	return g;
}

