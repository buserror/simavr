/*
	c3program.c

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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>

#include "c3program.h"

c3program_p
c3program_new(
		const char * name)
{
	c3program_p p = malloc(sizeof(*p));
	memset(p, 0, sizeof(*p));
	p->name = str_new(name);
	return p;
}

void
c3program_dispose(
		c3program_p p)
{
	c3program_purge(p);
	for (int pi = 0; pi < p->params.count; pi++) {
		c3program_param_p pa = &p->params.e[pi];
		str_free(pa->name);
	}
	c3program_param_array_free(&p->params);
	str_free(p->name);
	str_free(p->log);
	free(p);
}

void
c3program_purge(
		c3program_p p)
{
	for (int si = 0; si < p->shaders.count; si++) {
		c3shader_p s = &p->shaders.e[si];
		str_free(s->name);
		str_free(s->shader);
	}
	c3shader_array_free(&p->shaders);
}

int
c3program_load_shader(
		c3program_p p,
		uint32_t	type,
		const char * header,
		const char * filename,
		uint16_t flags)
{
	struct stat st;
	str_p pgm = NULL;

	if (stat(filename, &st))
		goto error;
	int fd = open(filename, O_RDONLY);
	if (fd == -1)
		goto error;

	int hlen = header ? strlen(header) : 0;
	pgm = str_alloc(st.st_size + hlen);
	if (header)
		strcpy(pgm->str, header);

	if (read(fd, pgm->str + hlen, st.st_size) != st.st_size)
		goto error;
	close(fd);
	pgm->str[pgm->len] = 0;	// zero terminate it

	c3shader_t s = {
		.type = type,
		.name = str_new(filename),
		.shader = pgm,
	};
	c3shader_array_add(&p->shaders, s);

	if (flags & C3_PROGRAM_LOAD_UNIFORM) {
		char * cur = pgm->str;
		char * l;

		while ((l = strsep(&cur, "\r\n")) != NULL) {
			while (*l && *l <= ' ')
				l++;
			str_p line = str_new(l);
			if (cur) // fix the endline after strsep
				*(cur-1) = '\n';
			if (strncmp(line->str, "uniform", 7))
				continue;
			// printf("UNI: %s\n", line->str);

			char * sep = line->str;
			char * uniform = strsep(&sep, " \t");
			char * unitype = strsep(&sep, " \t");
			char * uniname = strsep(&sep, " \t");
			/*
			 * found a parameter, extract it's type & name
			 */
			if (uniform && unitype && uniname) {
				// trim semicolons etc
				char *cl = uniname;
				while (isalpha(*cl) || *cl == '_')
					cl++;
				*cl = 0;
				str_p name = str_new(uniname);
				for (int pi = 0; pi < p->params.count && uniform; pi++)
					if (!str_cmp(name, p->params.e[pi].name))
						uniform = NULL;	// already there
				if (uniform) {
					c3program_param_t pa = {
							.type = str_new(unitype),
							.name = name,
							.program = p,
					};
					c3program_param_array_add(&p->params, pa);
					printf("%s %s: new parameter '%s' '%s'\n", __func__,
							p->name->str, unitype, uniname);
				} else
					str_free(name);
			}
			str_free(line);
		}
	}
	return p->shaders.count - 1;

error:
	if (fd != -1)
		close(fd);
	if (pgm)
		str_free(pgm);
	fprintf(stderr, "%s: %s: %s\n", __func__, filename, strerror(errno));
	return -1;

}
