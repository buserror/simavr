/*
 * history_cmd.c
 *
 *  Created on: 15 Oct 2015
 *      Author: michel
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "history_cmd.h"
#include "history.h"

history_cmd_list_p
history_cmd_list_new()
{
	history_cmd_list_p res = (history_cmd_list_p)malloc(sizeof(*res));
	memset(res, 0, sizeof(*res));

	return res;
}

void
history_cmd_list_dispose(
		history_cmd_list_p list )
{
	if (list->list)
		free(list->list);
	list->list = NULL;
	free(list);
}


static history_cmd_list_p global = NULL;

void
history_cmd_list_register(
		history_cmd_list_p list,
		const history_cmd_p cmd )
{
	if (!list) {
		if (!global)
			global = history_cmd_list_new();
		list = global;
	}

	if (!list || !cmd)
		return;
	if ((list->count % 8) == 0)
		list->list = realloc(list->list, sizeof(list->list[0]) * (list->count + 8));

	int ins = -1;
	for (int i = 0; i < list->count && ins == -1; i++) {
		history_cmd_p c = list->list[i];
		if (c == cmd || strcmp(cmd->names[0], c->names[0]) <= 0)
			ins = i;
	}
	if (ins == -1)
		ins = list->count;
	else
		memmove(list->list + ins + 1, list->list + ins, (list->count - ins) * sizeof(list->list[0]));
	list->list[ins] = cmd;
	list->count++;
}

history_cmd_p
history_cmd_lookup(
		history_cmd_list_p list,
		const char * cmd_name,
		int * out_alias)
{
	if (!list)
		list = global;
	if (!list || !cmd_name)
		return NULL;
	for (int i = 0; i < list->count; i++) {
		const history_cmd_p cmd = list->list[i];
		int alias = -1;
		for (int ai = 0; ai < 8 && cmd->names[ai] && alias == -1; ai++)
			if (!strcmp(cmd->names[ai], cmd_name)) {
				if (out_alias)
					*out_alias = ai;
				return cmd;
			}
	}
	return NULL;
}


int
history_cmd_execute(
		history_cmd_list_p list,
		const char * cmd_line )
{
	if (!list)
		list = global;
	if (!list || !cmd_line)
		return -1;

	int res = -1;
	wordexp_t words = {0};
	while (*cmd_line == ' ' || *cmd_line == '\t')
		cmd_line++;

	if (wordexp(cmd_line, &words, WRDE_NOCMD)) {
		wordfree(&words);
		fprintf(stderr, "Syntax error\r\n");
		goto out;
	}
	if (words.we_wordc == 0) {
		res = 0;
		goto out;
	}
	int alias = 0;
	const history_cmd_p cmd = history_cmd_lookup(list, words.we_wordv[0], &alias);
	if (!cmd) {
		fprintf(stderr, "Unknown command '%s'\r\n", words.we_wordv[0]);
		goto out;
	}
	if (cmd->parameter_map && !(cmd->parameter_map & (1 << (words.we_wordc-1)))) {
		fprintf(stderr, "%s: %s\r\n", words.we_wordv[0], cmd->usage);
		goto out;
	}
	if (cmd->execute_list)
		res = cmd->execute_list(list, &words);
	else if (cmd->execute)
		res = cmd->execute(&words);
	else {
		fprintf(stderr, "%s: Internal error, no execute function", words.we_wordv[0]);
	}
out:
	wordfree(&words);

	return res;
}

static int
history_cmd_help(
		struct history_cmd_list_t *list,
		wordexp_t * l)
{
	if (l->we_wordc == 1) {
		for (int i = 0; i < list->count; i++) {
			const history_cmd_p cmd = list->list[i];
			fprintf(stdout, "%-12.12s %s\r\n", cmd->names[0], cmd->usage);
		}
	} else {
		for (int i = 1; i < l->we_wordc; i++) {
			const history_cmd_p cmd = history_cmd_lookup(list, l->we_wordv[i], NULL);
			if (cmd) {
				fprintf(stderr, "%s: %s\r\n", l->we_wordv[i], cmd->usage);
				fprintf(stderr, "\t%s\r\n", cmd->help);
			} else {
				fprintf(stderr, "help: Unknown command '%s'\r\n", l->we_wordv[i]);
			}
		}
	}
	return 0;
}

const history_cmd_t cmd_help = {
	.names[0] = "help",
	.names[1] = "h",
	.usage = "[cmd]: Display help for commands",
	.help = "This helps",
	.parameter_map = (1 << 0) | (1 << 1),
	.execute_list = history_cmd_help,
};
HISTORY_CMD_REGISTER(cmd_help);

