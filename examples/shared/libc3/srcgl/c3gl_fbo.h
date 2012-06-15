/*
	c3gl_fbo.h

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


#ifndef __C3GL_FBO_H___
#define __C3GL_FBO_H___

#include "c3types.h"

enum {
	C3GL_FBO_COLOR	= 0,
	C3GL_FBO_DEPTH,
	C3GL_FBO_MAX,
};

typedef struct c3gl_fbo_t {
	c3vec2			size;
	uint32_t		flags;
	c3apiobject_t	fbo;
	struct {
		c3apiobject_t	bid;
		// ... ?
	} buffers[8];
} c3gl_fbo_t, *c3gl_fbo_p;


int
c3gl_fbo_create(
		c3gl_fbo_p b,
		c3vec2 size,
		uint32_t flags );
void
c3gl_fbo_resize(
		c3gl_fbo_p b,
		c3vec2 size);
void
c3gl_fbo_dispose(
		c3gl_fbo_p b );

#endif /* __C3GL_FBO_H___ */
