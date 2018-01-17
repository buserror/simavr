/* vim: ts=4
	history.h

	Copyright 2017 Michel Pollet <buserror@gmail.com>

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

#ifndef HISTORY_H_
#define HISTORY_H_

#include <sys/queue.h>

struct history_t;

typedef struct history_params_t {
	char prompt[32];
	int (*process_line)(
			struct history_params_t *p,
			const char *cmd_line);
} history_params_t;

struct history_t *
history_new(
		int ttyin, int ttyout,
		struct history_params_t * params,
		void * private_context);
int
history_idle(
		struct history_t * h);
void
history_display(
		struct history_t * h);

#endif /* HISTORY_H_ */
