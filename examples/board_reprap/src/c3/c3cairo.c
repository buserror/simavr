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


#include "c3/c3cairo.h"

c3cairo_p
c3cairo_new(
		c3object_p parent)
{
	c3cairo_p res = malloc(sizeof(*res));
	memset(res, 0, sizeof(*res));
	return c3cairo_init(res, parent);
}

c3cairo_p
c3cairo_init(
		c3cairo_p o,
		c3object_p parent)
{
	c3object_init(&o->object, parent);
	return o;
}
