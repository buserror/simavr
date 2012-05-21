/*
	c3driver_geometry.h

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


#ifndef __C3DRIVER_GEOMETRY_H___
#define __C3DRIVER_GEOMETRY_H___

#include "c3/c3driver.h"

struct c3geometry_t;

typedef struct c3driver_geometry_t {
	void (*dispose)(
			struct c3geometry_t * geometry,
			const struct c3driver_geometry_t *d);
	void (*prepare)(
			struct c3geometry_t * geometry,
			const struct c3driver_geometry_t *d);
	void (*draw)(
			struct c3geometry_t * geometry,
			const struct c3driver_geometry_t *d);
} c3driver_geometry_t, *c3driver_geometry_p;


#endif /* __C3DRIVER_GEOMETRY_H___ */
