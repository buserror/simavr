/*
	c3context.h

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


#ifndef __C3CONTEXT_H___
#define __C3CONTEXT_H___

#include "c3algebra.h"
#include "c3geometry.h"
#include "c3pixels.h"
#include "c3program.h"
#include "c3camera.h"

enum {
	C3_CONTEXT_VIEW_EYE = 0,
	C3_CONTEXT_VIEW_LIGHT
};

typedef struct c3context_view_t {
	int			type : 4,	// C3_CONTEXT_VIEW_EYE...
				dirty : 1;
	c3vec2		size;					// in pixels. for fbo/textures/window
	c3cam_t 	cam;

	c3geometry_array_t	projected;
	struct {
		c3f min, max;
	} z;
} c3context_view_t, *c3context_view_p;

DECLARE_C_ARRAY(c3context_view_t, c3context_view_array, 4);

//! c3context_t is a container for a 'scene' to be drawn
/*!
 * A c3context_t holds a root object, a list of already cached projected
 * version of the geometry, and a driver that can be customized to draw it.
 *
 * This is a wrapper around a "top level object", the list of projected
 * geometries is kept, purged and resorted if the root object becomes
 * dirty
 * TODO: Add the camera/eye/arcball control there
 */
typedef struct c3context_t {
	int	current;
	c3context_view_array_t	views;

	struct c3object_t * root;	// root object

	c3pixels_array_t 	pixels;	// pixels, textures...
	c3program_array_t	programs;	// fragment, vertex, geometry shaders

	const struct c3driver_context_t ** driver;
} c3context_t, *c3context_p;

//! Allocates a new context of size w=width, h=height
c3context_p
c3context_new(
		int w,
		int h);

//! Initializes a new context 'c' of size w=width, h=height
c3context_p
c3context_init(
		c3context_p c,
		int w,
		int h);

//! Disposes the context, and everything underneath
void
c3context_dispose(
		c3context_p c);

//! Reproject geometry for dirty objects
void
c3context_project(
		c3context_p c);
//! Draws the context
void
c3context_draw(
		c3context_p c);

IMPLEMENT_C_ARRAY(c3context_view_array);

/*
 * Set and get the current view, this is done
 * before projecting and drawing
 */
static inline c3context_view_p
c3context_view_get(
		c3context_p c )
{
	return &c->views.e[c->current];
}

static inline c3context_view_p
c3context_view_get_at(
		c3context_p c,
		int view)
{
	if (view < c->views.count)
		return &c->views.e[view];
	return NULL;
}

static inline void
c3context_view_set(
		c3context_p c,
		int view)
{
	if (view < c->views.count)
		c->current = view;
}


#endif /* __C3CONTEXT_H___ */
