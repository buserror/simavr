/*
 * history_cmd.h
 *
 *  Created on: 15 Oct 2015
 *      Author: michel
 */

#ifndef HISTORY_CMD_H_
#define HISTORY_CMD_H_

#include <stdint.h>
#include <wordexp.h>

struct history_cmd_list_t;

typedef struct history_cmd_t {
	const char * names[8];
	const char * usage;
	const char * help;
	uint8_t parameter_map;	// 0 means 'any number' otherwise, bitfield of valid options
	int (*execute)(wordexp_t * l);
	int (*execute_list)(struct history_cmd_list_t *list, wordexp_t * l);
} history_cmd_t, *history_cmd_p;


typedef struct history_cmd_list_t {
	int count;
	history_cmd_p * list;
} history_cmd_list_t, *history_cmd_list_p;

history_cmd_list_p
history_cmd_list_new();

void
history_cmd_list_dispose(
		history_cmd_list_p list );
void
history_cmd_list_register(
		history_cmd_list_p list,
		const history_cmd_p cmd );

#define HISTORY_CMD_REGISTER(__cmd) \
	__attribute__((constructor)) static void _history_register_##__cmd() { \
		history_cmd_list_register(NULL, (const history_cmd_p)&(__cmd));\
	}

history_cmd_p
history_cmd_lookup(
		history_cmd_list_p list,
		const char * cmd_name,
		int * out_alias);
int
history_cmd_execute(
		history_cmd_list_p list,
		const char * cmd_line );


#endif /* HISTORY_CMD_H_ */
