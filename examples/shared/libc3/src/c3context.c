/*
	c3context.c

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
#include "c3context.h"
#include "c3object.h"
#include "c3light.h"
#include "c3driver_context.h"

c3context_p
c3context_new(
		int w,
		int h)
{
	c3context_p res = malloc(sizeof(*res));
	return c3context_init(res, w, h);
}

c3context_p
c3context_init(
		c3context_p c,
		int w,
		int h)
{
	memset(c, 0, sizeof(*c));

	c3context_view_t v = {
			.type = C3_CONTEXT_VIEW_EYE,
			.size = c3vec2f(w, h),
			.dirty = 1,
			.index = c->views.count,
	};
	c3cam_init(&v.cam);
	c3context_view_array_add(&c->views, v);
	c->root = c3object_new(NULL);
	c->root->context = c;

	return c;
}

void
c3context_dispose(
		c3context_p c)
{
	c3object_dispose(c->root);
	for (int i = 0; i < c->views.count; i++)
		c3geometry_array_free(&c->views.e[i].projected);
	free(c);
}

static c3context_view_p qsort_view;

/*
 * Computes the distance from the 'eye' of the camera, sort by this value
 */
static int
_c3_z_sorter(
		const void *_p1,
		const void *_p2)
{
	c3geometry_p g1 = *(c3geometry_p*)_p1;
	c3geometry_p g2 = *(c3geometry_p*)_p2;
	// get center of bboxes
	c3vec3 c1 = c3vec3_add(g1->bbox.min, c3vec3_divf(c3vec3_sub(g1->bbox.max, g1->bbox.min), 2));
	c3vec3 c2 = c3vec3_add(g2->bbox.min, c3vec3_divf(c3vec3_sub(g2->bbox.max, g2->bbox.min), 2));

	c3cam_p cam = &qsort_view->cam;
	c3f d1 = c3vec3_length2(c3vec3_sub(c1, cam->eye));
	c3f d2 = c3vec3_length2(c3vec3_sub(c2, cam->eye));

	if (d1 > qsort_view->z.max) qsort_view->z.max = d1;
	if (d1 < qsort_view->z.min) qsort_view->z.min = d1;
	if (d2 > qsort_view->z.max) qsort_view->z.max = d2;
	if (d2 < qsort_view->z.min) qsort_view->z.min = d2;
	/*
	 * make sure transparent items are drawn after everyone else
	 */
	if (g1->mat.color.n[3] < 1)
		d1 -= 100000.0;
	if (g2->mat.color.n[3] < 1)
		d2 -= 100000.0;
	if (g1->type.type == C3_LIGHT_TYPE)
		d1 = -200000 + (int)(((c3light_p)g1)->light_id);
	if (g2->type.type == C3_LIGHT_TYPE)
		d2 = -200000 + (int)(((c3light_p)g2)->light_id);

	return d1 < d2 ? 1 : d1 > d2 ? -1 : 0;
}

void
c3context_project(
		c3context_p c)
{
	if (!c->root)
		return;

	/*
	 * if the root object is dirty, all the views are also
	 * dirty since the geometry has changed
	 */
	if (c->root->dirty) {
		for (int ci = 0; ci < c->views.count; ci++)
			c->views.e[ci].dirty = 1;
		c3mat4 m = identity3D();
		c3object_project(c->root, &m);
	}

	/*
	 * if the current view is dirty, gather all the geometry
	 * and Z sort it in a basic way
	 */
	c3context_view_p v = qsort_view = c3context_view_get(c);
	if (v->dirty) {
	    c3cam_update_matrix(&v->cam);

		c3geometry_array_p  array = &c3context_view_get(c)->projected;
		c3geometry_array_clear(array);
		c3object_get_geometry(c->root, array);

		v->z.min = 1000000000;
		v->z.max = -1000000000;

		qsort(v->projected.e,
				v->projected.count, sizeof(v->projected.e[0]),
		        _c3_z_sorter);
		v->z.min = sqrt(v->z.min);
		v->z.max = sqrt(v->z.max);

		v->dirty = 0;
	}
}

void
c3context_draw(
		c3context_p c)
{
	c3context_project(c);

	c3context_view_p v = c3context_view_get(c);

	C3_DRIVER(c, context_view_draw, v);

	c3geometry_array_p  array = &v->projected;
	for (int gi = 0; gi < array->count; gi++) {
		c3geometry_p g = array->e[gi];
		c3geometry_draw(g);
	}
}

