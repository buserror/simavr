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


#include "c3/c3object.h"


c3geometry_p
c3geometry_new(
		int type,
		c3object_p o /* = NULL */)
{
	c3geometry_p res = malloc(sizeof(c3geometry_t));
	memset(res, 0, sizeof(*res));
	res->type = type;
	res->dirty = 1;
	c3object_add_geometry(o, res);
	return res;
}

void
c3geometry_dispose(
		c3geometry_p g)
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
}
