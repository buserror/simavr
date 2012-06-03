/*
	c3program.h

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


#ifndef __C3PROGRAM_H___
#define __C3PROGRAM_H___

#include "c_utils.h"

typedef struct c3shader_t {
	uint32_t sid;	// shader id
	uint32_t type;
	str_p	name;
	str_p	shader;
	str_p	log;
} c3shader_t, *c3shader_p;

DECLARE_C_ARRAY(c3shader_t, c3shader_array, 4);

typedef struct c3program_param_t {
	int32_t pid;	// parameter id
	str_p	type;
	str_p	name;
} c3program_param_t, *c3program_param_p;

DECLARE_C_ARRAY(c3program_param_t, c3program_param_array, 4);

typedef struct c3program_t {
	uint32_t pid;	// program id
	str_p name;
	c3shader_array_t	shaders;
	c3program_param_array_t params;
	str_p	log;
} c3program_t, *c3program_p;

DECLARE_C_ARRAY(c3program_p, c3program_array, 4);

c3program_p
c3program_new(
		const char * name);

void
c3program_dispose(
		const char * name);

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

IMPLEMENT_C_ARRAY(c3program_param_array);
IMPLEMENT_C_ARRAY(c3shader_array);
IMPLEMENT_C_ARRAY(c3program_array);

#endif /* __C3PROGRAM_H___ */
