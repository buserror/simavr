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


typedef struct line_t {
	struct line_t * next;
	struct line_t * prev;

	int size, len, pos;
	char * line;
} line_t, *line_p;


typedef struct history_t {
	int ttyin;
	int ttyout;

	line_p head, tail;
	line_p current;
	char prompt[32];
	void * state;
	int temp;

	void *param;
	int (*process_line)(struct history_t *h, line_p l);

	//struct history_cmd_list_t * cmd_list;
} history_t, *history_p;


void
history_init(
		history_p h);

int
history_idle(
		history_p h);

void
history_display(
		history_p h);

#endif /* HISTORY_H_ */
