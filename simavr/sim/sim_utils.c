/*
	sim_utils.c

	Implements a Value Change Dump file outout to generate
	traces & curves and display them in gtkwave.

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

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

#include <string.h>
#include <stdlib.h>

#include "sim_utils.h"

static argv_p
argv_realloc(
	argv_p	argv,
	uint32_t size )
{
	argv = realloc(argv,
				sizeof(argv_t) + (size * sizeof(argv->argv[0])));
	argv->size = size;
	return argv;
}

argv_p
argv_parse(
	argv_p	argv,
	char * line )
{
	if (!argv)
		argv = argv_realloc(argv, 8);
	argv->argc = 0;

	/* strip end of lines and trailing spaces */
	char *d = line + strlen(line);
	while ((d - line) > 0 && *(--d) <= ' ')
		*d = 0;
	/* stop spaces + tabs */
	char *s = line;
	while (*s && *s <= ' ')
		s++;
	argv->line = s;
	char * a = NULL;
	do {
		if (argv->argc == argv->size)
			argv = argv_realloc(argv, argv->size + 8);
		if ((a = strsep(&s, " \t")) != NULL)
			argv->argv[argv->argc++] = a;
	} while (a);
	argv->argv[argv->argc] = NULL;
	return argv;
}
