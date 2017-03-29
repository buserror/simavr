/*
 * hcmd_misc.c
 *
 *  Created on: 15 Oct 2015
 *      Author: michel
 */


#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <ctype.h>
#include <wordexp.h>
#include <stdarg.h>
#include <poll.h>
#include <unistd.h>

#include "history_avr.h"

int history_redisplay = 0;


static int
_history_process_line(
		struct history_t *h,
		line_p l )
{
	printf("\r\n");
	history_cmd_execute(NULL, l->line);

	return 1;
}

static void
raw_std_logger(
		avr_t* avr,
		const int level,
		const char * format,
		va_list args)
{
	if (!avr || avr->log >= level) {
		FILE * d = (level > LOG_ERROR) ?  stdout : stderr;
		fprintf(d, "\r");
		vfprintf(d , format, args);
		fprintf(d, "\r");
		history_redisplay++;
	}
}

static int
_cmd_quit(
		wordexp_t * l)
{
	avr_terminate(avr);
	exit(0);
	return 0;
}

const history_cmd_t cmd_quit = {
	.names = { "quit", "q", "exit", },
	.usage = "quit simavr",
	.help = "exits the program",
	.parameter_map = 0,
	.execute = _cmd_quit,
};
HISTORY_CMD_REGISTER(cmd_quit);

int prompt_fd = -1;
int prompt_event = 0;

static void
callback_sleep_prompt(
		avr_t * avr,
		avr_cycle_count_t howLong)
{
	uint32_t usec = avr_pending_sleep_usec(avr, howLong);
	if (prompt_fd == -1) {
		usleep(usec);
		return;
	}
#if 1
	usleep(usec / 2);
#else
	struct pollfd ev = { .fd = prompt_fd, .events = POLLIN };
	int r = poll(&ev, 1, usec / 1000 / 2);
//	printf("poll %d %x\r\n", r, ev.revents);
	if (r > 0 && ev.revents)
		prompt_event++;
#endif
}

history_t history = {
	.ttyin = 0,
	.ttyout = 1,
	.prompt = "avr: ",
	.process_line = _history_process_line,
};

void history_avr_init()
{
	history_init(&history);
	prompt_fd = history.ttyin;
//	avr->sleep = callback_sleep_prompt;
    avr_global_logger_set(raw_std_logger);
}

void history_avr_idle()
{
	int prompt_event = 0;
	struct pollfd ev = { .fd = prompt_fd, .events = POLLIN };
	int r = poll(&ev, 1, 1000 / 2);
//	printf("poll %d %x\r\n", r, ev.revents);
	if (r > 0 && ev.revents)
		prompt_event++;
	if (history_redisplay) {
		history_redisplay = 0;
		history_display(&history);
		prompt_event++;
	}
	if (prompt_event)
		history_idle(&history);
}
