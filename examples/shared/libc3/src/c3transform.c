/*
	c3transform.c

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

c3transform_p
c3transform_new(
		c3object_p o )
{
	c3transform_p res = malloc(sizeof(*res));
	res->matrix = identity3D();
	res->object = o;
	res->name = NULL;
	c3transform_array_add(&o->transform, res);
	return res;
}

void
c3transform_dispose(
		c3transform_p t )
{
	if (t->object) {
		for (int oi = 0; oi < t->object->transform.count; oi++)
			if (t->object->transform.e[oi] == t) {
				c3transform_array_delete(&t->object->transform, oi, 1);
				c3object_set_dirty(t->object, true);
				break;
			}
		t->object = NULL;
	}
	str_free(t->name);
	free(t);
}

void
c3transform_set(
		c3transform_p t,
		const c3mat4p m )
{
	if (c3mat4_equal(m, &t->matrix))
		return;
	t->matrix = *m;
	c3object_set_dirty(t->object, true);
}
