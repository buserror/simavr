/*
	c3cairo.h

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


#ifndef __C3CAIRO_H___
#define __C3CAIRO_H___

#include "c3/c3texture.h"
#include "c3/c3pixels.h"
#include <pango/pangocairo.h>

typedef struct c3cairo_t {
	c3texture_t	tex;
	cairo_t	*	cr;
	cairo_surface_t * surface;
} c3cairo_t, *c3cairo_p;

c3cairo_p
c3cairo_new(
		struct c3object_t * parent /* = NULL */);

c3cairo_p
c3cairo_init(
		c3cairo_p o,
		struct c3object_t * parent /* = NULL */);

c3cairo_p
c3cairo_new_offscreen(
		struct c3object_t * parent /* = NULL */,
		int w, int h);

#endif /* __C3CAIRO_H___ */
