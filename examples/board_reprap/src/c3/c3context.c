/*
	c3context.c

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


#include "c3/c3context.h"
#include "c3/c3object.h"
#include "c3/c3driver_context.h"

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
	c->size.x = w;
	c->size.y = h;
	c->root = c3object_new(NULL);
	c->root->context = c;
	return c;
}

void
c3context_prepare(
		c3context_p c)
{
	if (!c->root || !c->root->dirty)
		return;

	c3mat4 m = identity3D();
	c3object_project(c->root, &m);
	c3geometry_array_clear(&c->projected);
	c3object_get_geometry(c->root, &c->projected);
}

void
c3context_draw(
		c3context_p c)
{
	c3context_prepare(c);
	for (int gi = 0; gi < c->projected.count; gi++) {
		c3geometry_p g = c->projected.e[gi];
		C3_DRIVER(c, geometry_draw, g);
	}
}
