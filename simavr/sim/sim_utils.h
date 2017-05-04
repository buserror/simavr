/*
	sim_utils.h

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

#ifndef __SIM_UTILS_H__
#define __SIM_UTILS_H__

#include <stdint.h>

typedef struct argv_t {
	uint32_t size, argc;
	char * line;
	char * argv[];
} argv_t, *argv_p;

/*
 * Allocate a argv_t structure, split 'line' into words (destructively)
 * and fills up argc, and argv fields with pointers to the individual
 * words. The line is stripped of any \r\n as well
 * You can pass an already allocated argv_t for it to be reused (and
 * grown to fit).
 *
 * You are still responsible, as the caller, to (free) the resulting
 * pointer, and the 'line' text, if appropriate, no duplication is made
 */
argv_p
argv_parse(
	argv_p	argv,
	char * line );

#endif /* __SIM_UTILS_H__ */
