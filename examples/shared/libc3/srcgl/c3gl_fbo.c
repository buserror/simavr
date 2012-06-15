/*
	c3gl_fbo.c

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

#if __APPLE__
#define GL_GLEXT_PROTOTYPES
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glut.h>
#include <GL/glext.h>
#endif
#include <stdio.h>
#include <string.h>

#include "c3gl_fbo.h"


#define GLCHECK(_w) {_w; dumpError(#_w);}
static int dumpError(const char * what)
{
	GLenum e;
	int count = 0;
	while ((e = glGetError()) != GL_NO_ERROR) {
		printf("%s: %s\n", what, gluErrorString(e));
		count++;
	}
	return count;
}

int
c3gl_fbo_create(
		c3gl_fbo_p b,
		c3vec2 size,
		uint32_t flags )
{
	memset(b, 0, sizeof(*b));
	b->size = size;
	b->flags = flags;

	/* Texture */
	GLCHECK(glActiveTexture(GL_TEXTURE0));

	if (b->flags & (1 << C3GL_FBO_COLOR)) {
		GLuint tex;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
				b->size.x, b->size.y, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);
		b->buffers[C3GL_FBO_COLOR].bid = (c3apiobject_t)tex;
	}

	/* Depth buffer */
	if (b->flags & (1 << C3GL_FBO_DEPTH)) {
		GLuint rbo_depth;
		GLCHECK(glGenRenderbuffers(1, &rbo_depth));
		glBindRenderbuffer(GL_RENDERBUFFER, rbo_depth);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
				b->size.x, b->size.y);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		b->buffers[C3GL_FBO_DEPTH].bid = (c3apiobject_t)rbo_depth;
	}

	/* Framebuffer to link everything together */
	GLuint fbo;
	GLCHECK(glGenFramebuffers(1, &fbo));
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	if (b->flags & (1 << C3GL_FBO_COLOR)) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_2D, (GLuint)b->buffers[C3GL_FBO_COLOR].bid, 0);
		// Set the list of draw buffers.
		GLenum DrawBuffers[2] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers
	} else
		glDrawBuffers(0, NULL); // "1" is the size of DrawBuffers

	if (b->flags & (1 << C3GL_FBO_DEPTH))
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
	        GL_RENDERBUFFER, (GLuint)b->buffers[C3GL_FBO_DEPTH].bid);
	b->fbo = (c3apiobject_t)fbo;

	GLenum status;
	if ((status = glCheckFramebufferStatus(GL_FRAMEBUFFER))
	        != GL_FRAMEBUFFER_COMPLETE) {
		fprintf(stderr, "%s: glCheckFramebufferStatus: error %d", __func__, (int)status);
		return -1 ;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return 0;
}

void
c3gl_fbo_resize(
		c3gl_fbo_p b,
		c3vec2 size)
{
	b->size = size;
// Rescale FBO and RBO as well
	if (b->flags & (1 << C3GL_FBO_COLOR)) {
		glBindTexture(GL_TEXTURE_2D, (GLuint)b->buffers[C3GL_FBO_COLOR].bid);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
				b->size.x, b->size.y, 0,
				GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	if (b->flags & (1 << C3GL_FBO_DEPTH)) {
		glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)b->buffers[C3GL_FBO_DEPTH].bid);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
				b->size.x, b->size.y);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
}

void
c3gl_fbo_dispose(
		c3gl_fbo_p b )
{
	/* free_resources */
	if (b->flags & (1 << C3GL_FBO_DEPTH)) {
		GLuint bid = (GLuint)b->buffers[C3GL_FBO_DEPTH].bid;
		glDeleteRenderbuffers(1, &bid);
	}
	if (b->flags & (1 << C3GL_FBO_COLOR)) {
		GLuint bid = (GLuint)b->buffers[C3GL_FBO_COLOR].bid;
		glDeleteTextures(1, &bid);
	}
	GLuint fbo = (GLuint)b->fbo;
	glDeleteFramebuffers(1, &fbo);
	memset(b, 0, sizeof(*b));
}

