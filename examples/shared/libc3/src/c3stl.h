/*
	c3stl.h

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


#ifndef __C3STL_H___
#define __C3STL_H___

/*
 * Loads an ASCII (TODO: Load STL Binary?) STL file as a c3object with
 * a set of c3geometries with the triangles
 */
struct c3object_t *
c3stl_load(
		const char * filename,
		struct c3object_t * parent);

#endif /* __C3STL_H___ */
