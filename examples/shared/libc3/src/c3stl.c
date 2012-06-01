/*
	c3stl.c

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

#include <stdio.h>
#include "c3algebra.h"
#include "c3geometry.h"
#include "c3object.h"
#include "c3stl.h"

enum {
	vertex_None = -1,
	vertex_Vertex,
	vertex_Normal,
};

static int
_c3stl_read_vertex(
		char * vt,
		c3vec3 * out )
{
	int res = 1;
	char *l = vt;
	/*char * key =*/ strsep(&l, " \t");
	char * x = strsep(&l, " \t");
	char * y = strsep(&l, " \t");
	char * z = strsep(&l, " \t");

	if (x) sscanf(x, "%f", out->n);
	if (y) sscanf(y, "%f", out->n + 1);
	if (z) sscanf(z, "%f", out->n + 2);
//	printf("'%s' '%s' '%s' '%s' = %.2f %.2f %.2f\n",
//			key, x, y, z, out->n[0], out->n[1], out->n[2]);
	return res;
}

struct c3object_t *
c3stl_load(
		const char * filename,
		c3object_p parent)
{
	FILE *f = fopen(filename, "r");
	if (!f) {
		perror(filename);
		return NULL;
	}

	c3object_p		o = c3object_new(parent);
	c3geometry_p	current_g = NULL;
	o->name = str_new(filename);

	int state = 0;
	while (!feof(f)) {
		char line[256];

		fgets(line, sizeof(line), f);

		int l = strlen(line);
		while (l && line[l-1] < ' ')
			line[--l] = 0;
		if (!l)
			continue;
		char * keyword = line;
		while (*keyword && *keyword <= ' ')
			keyword++;
		l = strlen(keyword);
	//	printf("%d>'%s'\n", state, keyword);

		switch (state) {
			case 0:	//
				if (!strncmp(keyword, "solid ", 6)) {
					char * n = keyword + 6;
					current_g = c3geometry_new(c3geometry_type(C3_TRIANGLE_TYPE, 0), o);
					current_g->name = str_new(n);

					state = 1;
				}
				break;
			case 1: //
				if (!strncmp(keyword, "facet ", 6)) {
					c3vec3 normal;
					_c3stl_read_vertex(keyword + 6, &normal);
					c3vertex_array_add(&current_g->normals, normal);
					c3vertex_array_add(&current_g->normals, normal);
					c3vertex_array_add(&current_g->normals, normal);
					state = 2;
				} else if (!strncmp(keyword, "endsolid ", 9))
					state = 0;
				break;
			case 2:
				if (!strncmp(keyword, "outer loop", 10))
					state = 3;
				else if (!strncmp(keyword, "endfacet", 8))
					state = 1;
				break;
			case 3:
				if (!strncmp(keyword, "vertex ", 7)) {
					c3vec3 v;
					_c3stl_read_vertex(keyword, &v);
					c3vertex_array_add(&current_g->vertice, v);
					state = 3;
				} else if (!strncmp(keyword, "endloop", 7))
					state = 2;
				break;
		}
	}

	fclose(f);
	return o;
}
