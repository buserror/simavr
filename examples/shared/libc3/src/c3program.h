/*
	c3program.h

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


#ifndef __C3PROGRAM_H___
#define __C3PROGRAM_H___

#include "c3types.h"
#include "c_utils.h"

typedef struct c3shader_t {
	c3apiobject_t sid;	// shader id
	uint32_t type;
	str_p	name;
	str_p	shader;
} c3shader_t, *c3shader_p;

DECLARE_C_ARRAY(c3shader_t, c3shader_array, 4);

typedef struct c3program_param_t {
	struct c3program_t * program;
	c3apiobject_t pid;	// parameter id
	str_p	type;
	str_p	name;
} c3program_param_t, *c3program_param_p;

DECLARE_C_ARRAY(c3program_param_t, c3program_param_array, 4);

typedef struct c3program_t {
	c3apiobject_t pid;	// program id
	int						verbose : 1;
	str_p name;
	c3shader_array_t	shaders;
	c3program_param_array_t params;
	str_p					log;	// if an error occurs
} c3program_t, *c3program_p;

DECLARE_C_ARRAY(c3program_p, c3program_array, 4);

//! Allocates a new, empty program
c3program_p
c3program_new(
		const char * name);

//! disposes of a c3program memory
void
c3program_dispose(
		c3program_p p);

//! purge deletes the shader storage, but keep the program and parameters
void
c3program_purge(
		c3program_p p);

enum {
	C3_PROGRAM_LOAD_UNIFORM = (1 << 0),
};

int
c3program_load_shader(
		c3program_p p,
		uint32_t	type,
		const char * header,
		const char * filename,
		uint16_t flags);

c3program_param_p
c3program_locate_param(
		c3program_p p,
		const char * name );

IMPLEMENT_C_ARRAY(c3program_param_array);
IMPLEMENT_C_ARRAY(c3shader_array);
IMPLEMENT_C_ARRAY(c3program_array);

#endif /* __C3PROGRAM_H___ */
