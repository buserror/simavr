/*
	c3cairo.c

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


#include "c3cairo.h"
#include "c3driver_geometry.h"

#if CONFIG_C3_CAIRO

void
_c3cairo_dispose(
		c3geometry_p g,
		const c3driver_geometry_t * d)
{
	c3cairo_p c = (c3cairo_p)g;

	if (c->cr)
		cairo_destroy(c->cr);
	if (c->surface)
		cairo_surface_destroy(c->surface);
	C3_DRIVER_INHERITED(g, d, dispose);
}

static void
_c3cairo_project(
		c3geometry_p g,
		const struct c3driver_geometry_t *d,
		c3mat4p m)
{
	C3_DRIVER_INHERITED(g, d, project, m);
}

const c3driver_geometry_t c3cairo_base_driver = {
	.dispose = _c3cairo_dispose,
	.project = _c3cairo_project,
};
const c3driver_geometry_t c3texture_driver;
const c3driver_geometry_t c3geometry_driver;

c3cairo_p
c3cairo_new(
		struct c3object_t * parent)
{
	c3cairo_p res = malloc(sizeof(*res));
	return c3cairo_init(res, parent);
}

c3cairo_p
c3cairo_init(
		c3cairo_p o,
		struct c3object_t * parent)
{
	memset(o, 0, sizeof(*o));
	c3texture_init(&o->tex, parent);

	static const c3driver_geometry_t * list[] = {
			&c3cairo_base_driver, &c3texture_driver, &c3geometry_driver, NULL,
	};
	((c3geometry_p)o)->driver = list;

	return o;
}

c3cairo_p
c3cairo_new_offscreen(
		struct c3object_t * parent,
		int w, int h)
{
	c3cairo_p o = c3cairo_new(parent);

	o->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	o->cr = cairo_create(o->surface);

	c3pixels_p dst = c3pixels_new(w, h, 4,
			cairo_image_surface_get_stride(o->surface),
			cairo_image_surface_get_data(o->surface));
	o->tex.geometry.mat.texture = dst;

	return o;
}

#if 0
cairo_surface_destroy(_surface);
else
cairo_surface_finish(_surface);
#endif
#endif // CONFIG_C3_CAIRO
