/*
	c3geometry.c

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


#include <stdio.h>
#include <math.h>
#include "c3object.h"
#include "c3context.h"
#include "c3driver_geometry.h"
#include "c3driver_context.h"

static void
_c3geometry_dispose(
		c3geometry_p  g,
		const struct c3driver_geometry_t *d)
{
	/*
	 * If we're still attached to an object, detach
	 */
	if (g->object) {
		for (int oi = 0; oi < g->object->geometry.count; oi++)
			if (g->object->geometry.e[oi] == g) {
				c3geometry_array_delete(&g->object->geometry, oi, 1);
				c3object_set_dirty(g->object, true);
				break;
			}
		g->object = NULL;
	}
	/* let the context driver have a chance to clear it's own stuff */
	if (g->object && g->object->context)
		C3_DRIVER(g->object->context, geometry_dispose, g);
	str_free(g->name);
	c3vertex_array_free(&g->vertice);
	c3tex_array_free(&g->textures);
	c3colorf_array_free(&g->colorf);
	free(g);
//	C3_DRIVER_INHERITED(g, d, dispose);
}

static void
_c3geometry_project(
		c3geometry_p g,
		const struct c3driver_geometry_t *d,
		c3mat4p m)
{
	if (g->vertice.count) {
		for (int vi = 0; vi < g->vertice.count; vi++) {
			c3vec3 v = c3mat4_mulv3(m, g->vertice.e[vi]);
			if (vi == 0)
				g->bbox.min = g->bbox.max = v;
			else {
				g->bbox.max = c3vec3_min(g->bbox.min, v);
				g->bbox.max = c3vec3_max(g->bbox.max, v);
			}
		}
	} else
		g->bbox.min = g->bbox.max = c3vec3f(0,0,0);

	if (g->object && g->object->context)
		C3_DRIVER(g->object->context, geometry_project, g, m);
	g->dirty = 0;
//	C3_DRIVER_INHERITED(g, d, project);
}

static void
_c3geometry_draw(
		c3geometry_p g,
		const struct c3driver_geometry_t *d)
{
	if (g->object && g->object->context)
		C3_DRIVER(g->object->context, geometry_draw, g);
//	C3_DRIVER_INHERITED(g, d, draw);
}

const  c3driver_geometry_t c3geometry_driver = {
	.dispose = _c3geometry_dispose,
	.project = _c3geometry_project,
	.draw = _c3geometry_draw,
};

c3geometry_p
c3geometry_new(
		c3geometry_type_t type,
		c3object_p o /* = NULL */)
{
	c3geometry_p res = malloc(sizeof(c3geometry_t));
	return c3geometry_init(res, type, o);
}

c3geometry_p
c3geometry_init(
		c3geometry_p g,
		c3geometry_type_t type,
		struct c3object_t * o /* = NULL */)
{
	memset(g, 0, sizeof(*g));
	static const c3driver_geometry_t * list[] = {
			&c3geometry_driver, NULL,
	};
	g->driver = list;
	g->type = type;
	g->dirty = 1;
	if (o)
		c3object_add_geometry(o, g);
	return g;
}

c3driver_geometry_p
c3geometry_get_custom(
		c3geometry_p g )
{
	if (g->custom)
		return (c3driver_geometry_p)g->driver[0];
	int cnt = 0;
	for (int di = 0; g->driver[di]; di++)
		cnt++;
	c3driver_geometry_p * newd = malloc(sizeof(c3driver_geometry_p) * (cnt + 2));
	memcpy(&newd[1], g->driver, (cnt + 1) * sizeof(c3driver_geometry_p));
	newd[0] = malloc(sizeof(c3driver_geometry_t));
	memset(newd[0], 0, sizeof(c3driver_geometry_t));
	g->custom = 1;
	g->driver = (typeof(g->driver))newd;
	return newd[0];
}

void
c3geometry_dispose(
		c3geometry_p g)
{
	C3_DRIVER(g, dispose);
}

void
c3geometry_project(
		c3geometry_p g,
		c3mat4p m)
{
	if (!g->dirty)
		return;
	C3_DRIVER(g, project, m);
}

void
c3geometry_draw(
		c3geometry_p g )
{
	C3_DRIVER(g, draw);
}

void
c3geometry_factor(
		c3geometry_p g,
		c3f tolerance,
		c3f normaltolerance)
{
	printf("%s Start geometry has %d vertices and %d indexes\n", __func__,
			g->vertice.count, g->indices.count);
	printf("%s Start geometry has %d normals and %d tex\n", __func__,
			g->normals.count, g->textures.count);

	c3f tolerance2 = tolerance * tolerance;

	int in_index = g->indices.count;
	int vcount = in_index ? in_index : g->vertice.count;
	int input = 0;
	int output = 0;
	g->indices.count = 0;
	while (input < vcount) {
		int current = in_index ? g->indices.e[input] : input;
		c3vec3 v = g->vertice.e[current];
		c3vec3 n = g->normals.count ? g->normals.e[current] : c3vec3f(0,0,0);
		c3vec3 np = c3vec3_polar(n);	// normal in polar coord

		int oi = -1;
		for (int ci = 0; ci < output && oi == -1; ci++)
			if (c3vec3_length2(c3vec3_sub(g->vertice.e[ci], v)) < tolerance2) {
				if (g->normals.count) {
					c3vec3 nc = g->normals.e[ci];
					c3vec3 pc = c3vec3_polar(nc);

					c3vec3 d = c3vec3_sub(np, pc);
					while (d.n[0] <= -M_PI) d.n[0] += (2*M_PI);
					while (d.n[1] <= -M_PI) d.n[1] += (2*M_PI);

					if (fabs(d.n[0]) < normaltolerance &&
							fabs(d.n[1]) < normaltolerance) {
						oi = ci;
						// replace the compared normal with the 'merged' one
						// that should hopefully trim it to the right direction
						// somehow. Not perfect obviously
						g->normals.e[ci] = c3vec3_add(n, nc);
					}
				} else
					oi = ci;
			}
		if (oi == -1) {
			oi = output;
			g->vertice.e[output] = g->vertice.e[current];
			if (g->textures.count)
				g->textures.e[output] = g->textures.e[current];
			if (g->normals.count)
				g->normals.e[output] = n;
			if (g->colorf.count)
				g->colorf.e[output] = g->colorf.e[current];
			output++;
		}
		c3indices_array_add(&g->indices, oi);
		input++;
	}
	g->vertice.count = output;
	c3vertex_array_realloc(&g->vertice, output);
	if (g->textures.count) {
		g->textures.count = output;
		c3tex_array_realloc(&g->textures, output);
	}
	if (g->normals.count) {
		g->normals.count = output;
		c3vertex_array_realloc(&g->normals, output);
		for (int ni = 0; ni < output; ni++)
			g->normals.e[ni] = c3vec3_normalize(g->normals.e[ni]);
	}
	if (g->colorf.count) {
		g->colorf.count = output;
		c3colorf_array_realloc(&g->colorf, output);
	}
	g->dirty = 1;

	printf("%s end geometry has %d vertices and %d indexes\n",  __func__,
			g->vertice.count, g->indices.count);
}
