/*
 * history.h
 *
 *  Created on: 15 Oct 2015
 *      Author: michel
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
