/*
	c3lines.c

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


#include "c3object.h"
#include "c3context.h"
#include "c3driver_geometry.h"
#include "c3lines.h"


void
c3lines_prepare(
		c3vertex_p  vertices,		// points A,B pairs
		size_t		count,
		c3vertex_array_p v,			// triangles
		c3tex_array_p tex,
		c3f lineWidth,
		c3mat4p m)
{
	c3tex_array_clear(tex);
	c3vertex_array_clear(v);
	for (int l = 0; l < count; l += 2) {
		c3vec3 a = c3mat4_mulv3(m, vertices[l]);
		c3vec3 b = c3mat4_mulv3(m, vertices[l+1]);

		c3vec3 e = c3vec3_mulf(c3vec3_normalize(c3vec3_sub(b, a)), lineWidth);

		c3vec3 N = c3vec3f(-e.y, e.x, 0);
		c3vec3 S = c3vec3_minus(N);
		c3vec3 NE = c3vec3_add(N, e);
		c3vec3 NW = c3vec3_sub(N, e);
		c3vec3 SW = c3vec3_minus(NE);
		c3vec3 SE = c3vec3_minus(NW);
#if 0
		c3vertex_array_add(v, c3vec3_add(a, SW));
		c3vertex_array_add(v, c3vec3_add(a, NW));
		c3vertex_array_add(v, c3vec3_add(a, S));
		c3vertex_array_add(v, c3vec3_add(a, N));
		c3vertex_array_add(v, c3vec3_add(b, S));
		c3vertex_array_add(v, c3vec3_add(b, N));
		c3vertex_array_add(v, c3vec3_add(b, SE));
		c3vertex_array_add(v, c3vec3_add(b, NE));
#endif

		const float ts = 1;

		c3vertex_array_add(v, c3vec3_add(a, SW));
		c3vertex_array_add(v, c3vec3_add(a, S));
		c3vertex_array_add(v, c3vec3_add(a, NW));
		c3tex_array_add(tex, c3vec2f(ts * 0  , ts * 0  ));
		c3tex_array_add(tex, c3vec2f(ts * 0.5, ts * 0  ));
		c3tex_array_add(tex, c3vec2f(ts * 0  , ts * 1  ));

		c3vertex_array_add(v, c3vec3_add(a, S));
		c3vertex_array_add(v, c3vec3_add(a, N));
		c3vertex_array_add(v, c3vec3_add(a, NW));
		c3tex_array_add(tex, c3vec2f(ts * 0.5, ts * 0  ));
		c3tex_array_add(tex, c3vec2f(ts * 0.5, ts * 1  ));
		c3tex_array_add(tex, c3vec2f(ts * 0  , ts * 1  ));

		c3vertex_array_add(v, c3vec3_add(a, N));
		c3vertex_array_add(v, c3vec3_add(b, S));
		c3vertex_array_add(v, c3vec3_add(b, N));
		c3tex_array_add(tex, c3vec2f(ts * 0.5, ts * 1  ));
		c3tex_array_add(tex, c3vec2f(ts * 0.5, ts * 0  ));
		c3tex_array_add(tex, c3vec2f(ts * 0.5, ts * 1  ));

		c3vertex_array_add(v, c3vec3_add(a, N));
		c3vertex_array_add(v, c3vec3_add(a, S));
		c3vertex_array_add(v, c3vec3_add(b, S));
		c3tex_array_add(tex, c3vec2f(ts * 0.5, ts * 1  ));
		c3tex_array_add(tex, c3vec2f(ts * 0.5, ts * 0  ));
		c3tex_array_add(tex, c3vec2f(ts * 0.5, ts * 0  ));

		c3vertex_array_add(v, c3vec3_add(b, N));
		c3vertex_array_add(v, c3vec3_add(b, S));
		c3vertex_array_add(v, c3vec3_add(b, SE));
		c3tex_array_add(tex, c3vec2f(ts * 0.5, ts * 1  ));
		c3tex_array_add(tex, c3vec2f(ts * 0.5, ts * 0  ));
		c3tex_array_add(tex, c3vec2f(ts * 1  , ts * 0  ));

		c3vertex_array_add(v, c3vec3_add(b, N));
		c3vertex_array_add(v, c3vec3_add(b, SE));
		c3vertex_array_add(v, c3vec3_add(b, NE));
		c3tex_array_add(tex, c3vec2f(ts * 0.5, ts * 1  ));
		c3tex_array_add(tex, c3vec2f(ts * 1  , ts * 0  ));
		c3tex_array_add(tex, c3vec2f(ts * 1  , ts * 1  ));

	}
}

void
c3lines_init(
		c3geometry_p g,
		c3vertex_p  vertices,		// points A,B pairs
		size_t		count,
		c3f 		lineWidth)
{
	c3mat4 i = identity3D();
	c3lines_prepare(vertices, count, &g->vertice, &g->textures, lineWidth, &i);
	g->type.type = C3_LINES_TYPE;
}

#if 0
static void
_c3lines_project(
		c3geometry_p g,
		const struct c3driver_geometry_t *d,
		c3mat4p m)

const  c3driver_geometry_t c3lines_driver = {
	.project = _c3lines_project,
};

const  c3driver_geometry_t c3geometry_driver;

c3geometry_set_lines(
		c3f lineWidth)
{
	static const c3driver_geometry_t * list[] = {
			&c3lines_driver, &c3geometry_driver, NULL,
	};
}
#endif
