/*
	c3light.c

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

 	This file is part of libc3.

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
#include <string.h>
#include "c3light.h"

c3light_p
c3light_new(
		struct c3object_t * o /* = NULL */)
{
	c3light_p res = malloc(sizeof(*res));
	return c3light_init(res, o);
}

c3light_p
c3light_init(
		c3light_p l,
		struct c3object_t * o /* = NULL */)
{
	memset(l, 0, sizeof(*l));
	c3geometry_init(&l->geometry,
			c3geometry_type(C3_LIGHT_TYPE, 0),
			o);

	return l;
}
