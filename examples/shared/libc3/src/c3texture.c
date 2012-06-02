/*
	c3texture.c

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


#include <stdio.h>
#include "c3texture.h"
#include "c3driver_geometry.h"

void
_c3texture_dispose(
		c3geometry_p g,
		const c3driver_geometry_t * d)
{
//	c3texture_p t = (c3texture_p)g;
	C3_DRIVER_INHERITED(g, d, dispose);
}

void
_c3texture_project(
		c3geometry_p g,
		const c3driver_geometry_t * d,
		c3mat4p m)
{
	c3texture_p t = (c3texture_p)g;
	c3pixels_p p = t->geometry.mat.texture;
	if (!p) {
		C3_DRIVER_INHERITED(g, d, project, m);
		return;
	}
	c3vec2 qs = c3vec2f(
			t->size.x > 0 ? t->size.x : p->w,
			t->size.y > 0 ? t->size.y : p->h);
	c3vec3 v[4] = {
			c3vec3f(0, 0, 0), c3vec3f(qs.x, 0, 0),
			c3vec3f(qs.x, qs.y, 0), c3vec3f(0, qs.y, 0)
	};
	c3vertex_array_clear(&g->vertice);
	c3vertex_array_realloc(&g->vertice, 4);
	c3vertex_array_insert(&g->vertice, 0, v, 4);

	c3f tw = p->normalize ? 1.0 : p->w,
		th = p->normalize ? 1.0 : p->h;
	c3vec2 ti[4] = {
			c3vec2f(0, th), c3vec2f(tw, th),
			c3vec2f(tw, 0), c3vec2f(0, 0)
	};
	if (p->trace)
		printf("%s size %.0fx%.0f tex %.0fx%.0f\n", __func__, qs.x, qs.y, tw, th);
	c3tex_array_clear(&t->geometry.textures);
	c3tex_array_realloc(&t->geometry.textures, 4);
	c3tex_array_insert(&t->geometry.textures, 0, ti, 4);

	C3_DRIVER_INHERITED(g, d, project, m);
}

const c3driver_geometry_t c3texture_driver = {
		.dispose = _c3texture_dispose,
		.project = _c3texture_project,
};
extern const c3driver_geometry_t c3geometry_driver;

c3texture_p
c3texture_new(
		struct c3object_t * o /* = NULL */)
{
	c3texture_p res = malloc(sizeof(*res));
	return c3texture_init(res, o);
}

c3texture_p
c3texture_init(
		c3texture_p t,
		struct c3object_t * o /* = NULL */)
{
	memset(t, 0, sizeof(*t));
	c3geometry_init(&t->geometry,
			c3geometry_type(C3_TEXTURE_TYPE, 0 /* GL_TRIANGLE_FAN */),
			o);
	static const c3driver_geometry_t * list[] = {
			&c3texture_driver, &c3geometry_driver, NULL,
	};
	t->geometry.driver = list;

	return t;
}

void
c3texture_setpixels(
		)
{

}
