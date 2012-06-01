/*
	c3geometry.c

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
	str_free(g->name);
	c3vertex_array_free(&g->vertice);
	c3vertex_array_free(&g->projected);
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
		c3vertex_array_realloc(&g->projected, g->vertice.count);
		g->projected.count = g->vertice.count;
		for (int vi = 0; vi < g->vertice.count; vi++) {
			g->projected.e[vi] = c3mat4_mulv3(m, g->vertice.e[vi]);
			if (vi == 0)
				g->bbox.min = g->bbox.max = g->projected.e[vi];
			else {
				g->bbox.max = c3vec3_min(g->bbox.min, g->projected.e[vi]);
				g->bbox.max = c3vec3_max(g->bbox.max, g->projected.e[vi]);
			}
		}
	}

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


