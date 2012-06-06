/*
	c3lines.h

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


#ifndef __C3LINES_H___
#define __C3LINES_H___

#include "c3geometry.h"

enum {
	C3_LINES_TYPE = C3_TYPE('l','i','n','e'),
};
/*
 * Takes an array of points A,B and split it into 'fat' lines around
 * lineWidth, generates an array of triangles and an array of corresponding
 * texture coordinates. Can also do a projection at the same time
 * TODO: Add array indices
 */
void
c3lines_prepare(
		c3vertex_p  vertices,		// points A,B pairs
		size_t		count,
		c3vertex_array_p v,			// triangles
		c3tex_array_p tex,
		c3f lineWidth,
		c3mat4p m);

void
c3lines_init(
		c3geometry_p g,
		c3vertex_p  vertices,		// points A,B pairs
		size_t		count,
		c3f 		lineWidth);

#endif /* __C3LINES_H___ */
