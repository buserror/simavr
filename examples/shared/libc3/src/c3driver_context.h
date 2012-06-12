/*
	c3driver_context.h

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


#ifndef __C3DRIVER_CONTEXT_H___
#define __C3DRIVER_CONTEXT_H___

#include "c3driver.h"

struct c3context_t;
struct c3driver_context_t;
struct c3geometry_t;
struct c3context_view_t;

typedef struct c3driver_context_t {
	/*
	 * Called when a geometry projection had changed in world view
	 * can also be used to prepare resources like textures and so on
	 */
	void (*geometry_project)(
			struct c3context_t * c,
			const struct c3driver_context_t *d,
			struct c3geometry_t * g,
			union c3mat4 * mat);
	/*
	 * Called to draw a geometry
	 */
	void (*geometry_draw)(
			struct c3context_t * c,
			const struct c3driver_context_t *d,
			struct c3geometry_t * g);

	/*
	 * Called when starting to draw a context view(point)
	 */
	void (*context_view_draw)(
			struct c3context_t * c,
			const struct c3driver_context_t *d,
			struct c3context_view_t * ctx);

	/*
	 * called when a geometry is disposed of, let the application
	 * delete resources like textures etc
	 */
	void (*geometry_dispose)(
		struct c3context_t * c,
		const struct c3driver_context_t *d,
		struct c3geometry_t * g);
} c3driver_context_t, *c3driver_context_p;

#endif /* __C3DRIVER_CONTEXT_H___ */
