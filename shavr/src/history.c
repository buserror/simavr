/*
 * history.c
 *
 *  Created on: 15 Oct 2015
 *      Author: michel
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <termios.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "history.h"

static struct termios orig_termios;  /* TERMinal I/O Structure */

void fatal(char *message) {
	fprintf(stderr, "fatal error: %s\n", message);
	exit(1);
}


/* put terminal in raw mode - see termio(7I) for modes */
static void
tty_raw(void)
{
	struct termios raw;

	raw = orig_termios; /* copy original and then modify below */

	/* input modes - clear indicated ones giving: no break, no CR to NL,
	 no parity check, no strip char, no start/stop output (sic) control */
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

	/* Contrary to normal raw mode, we want to keep the post processing on output
	 * otherwise the printf() all over fail due to their lack of \r
	 */
//	raw.c_oflag &= ~(OPOST);

	/* control modes - set 8 bit chars */
	raw.c_cflag |= (CS8);

	/* local modes - clear giving: echoing off, canonical off (no erase with
	 backspace, ^U,...),  no extended functions, no signal chars (^Z,^C) */
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

	/* control chars - set return condition: min number of bytes and timer */
#if 0
	raw.c_cc[VMIN] = 5;
	raw.c_cc[VTIME] = 8; /* after 5 bytes or .8 seconds
	 after first byte seen      */
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 0; /* immediate - anything       */
	raw.c_cc[VMIN] = 2;
	raw.c_cc[VTIME] = 0; /* after two bytes, no timer  */
#endif
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 8; /* after a byte or .8 seconds */

	/* put terminal in raw mode after flushing */
	if (tcsetattr(1, TCSAFLUSH, &raw) < 0)
		fatal("can't set raw mode");
}

/* reset tty - useful also for restoring the terminal when this process
   wishes to temporarily relinquish the tty
*/
static int
tty_reset(void)
{
	/* flush and reset */
	if (tcsetattr(1, TCSAFLUSH, &orig_termios) < 0)
		return -1;
	return 0;
}

/* exit handler for tty reset */
static void
tty_atexit(void) /* NOTE: If the program terminates due to a signal   */
{ /* this code will not run.  This is for exit()'s     */
	tty_reset(); /* only.  For resetting the terminal after a signal, */
} /* a signal handler which calls tty_reset is needed. */

static line_p
line_new(
		history_p h)
{
	line_p res = malloc(sizeof(line_t));
	memset(res, 0, sizeof(*res));
	res->prev = h->tail;
	if (!h->tail)
		h->head = res;
	else
		h->tail->next = res;
	h->tail = res;
	h->current = res;
	return res;
}


line_p
line_dup(
		history_p h,
		line_p l)
{
	line_p n = line_new(h);
	n->pos = l->pos;
	n->size = l->size;
	n->len = l->len;
	n->line = malloc(n->size);
	memcpy(n->line, l->line, n->size);
	h->current = n;
	return n;
}

void
history_display(
		history_p h)
{
	char s[16];
	int pl = strlen(h->prompt);
	write(h->ttyout, "\033[0m\r", 5);
	write(h->ttyout, h->prompt, pl);
	if (h->current) {
		write(h->ttyout, h->current->line, h->current->len);
		pl += h->current->pos;
	}
	// kill rest of line, return to 0, move back to X
	sprintf(s, "\033[K\r\033[%dC", pl);
	write(h->ttyout, s, strlen(s));
}

/*
 * this is a redone skeleton protothread bunch of macros
 * this will only work with gcc and clang. Use the old, bigger
 * protothread if porting to another compiler
 */
// these are to force preprocessor to evaluate
#define PP_SUB_CONCAT(p1, p2) p1##p2
#define PP_CONCAT(p1, p2) PP_SUB_CONCAT(p1, p2)

#define PT_START(__s) do { if (__s) goto *__s;	} while (0)
#define PT_END(__s)  PP_CONCAT(_e, __FUNCTION__):
#define PT_YIELD(__s) \
	(__s) = &&PP_CONCAT(pt_, __LINE__); \
	goto PP_CONCAT(_e, __FUNCTION__); \
	PP_CONCAT(pt_, __LINE__):\
	(__s) = NULL;

int
line_key(
		history_p h,
		uint8_t key)
{
	int insert = 0;
	int delete = 0;
	int cursor_move = 0;
	int res = 0;

	if (!h->current)
		line_new(h);

	PT_START(h->state);

	switch (key) {
		case 0x7f: goto del_char;
		case 0 ... ' '-1: {
			switch(key) {
				case 3: printf("\r\n"); exit(0); break;
				case 8:	// control h delete
					del_char:
					delete++;
					cursor_move = -1;
					break;
				case 2: // control b : cursor -1
					previous_char:
					cursor_move = -1;
					break;
				case 6: // control f : cursor + 1
					next_char:
					cursor_move = 1;
					break;
				case 5: // control e : end of line
					if (h->current)
						h->current->pos = h->current->len;
					break;
				case 1: // control a : start of line
					if (h->current)
						h->current->pos = 0;
					break;
				case 23: {// control-W : delete word
					if (!h->current->pos)
						break;
					char *d = h->current->line + h->current->pos - 1;
					// first skip any trailing spaces
					while (*d == ' ' && h->current->pos - delete > 0) {
						delete++;
						cursor_move -= 1;
						d--;
					}
					// then delete any word before the cursor
					while (*d != ' ' && h->current->pos - delete > 0) {
						delete++;
						cursor_move -= 1;
						d--;
					}
				}	break;
				case 11: { // control-K : kill remains of line
					delete = h->current->len - h->current->pos;
				}	break;
				case 13: { // return!!
					if (h->current->len) {
						if (!h->process_line || h->process_line(h, h->current)) {
							res++;
							if (h->current != h->tail)
								line_dup(h, h->current);
							line_new(h);
						}
					} else
						write(1, "\012", 1);	// next line
				}	break;
				case 16: { // control-P -- previous line
					previous_line:
					if (h->current->prev)
						h->current = h->current->prev;
				}	break;
				case 14: {	// control-N -- next line
					next_line:
					if (h->current->next)
						h->current = h->current->next;
				}	break;
				case 27: {
					PT_YIELD(h->state);
					switch (key) {
						case '[': {
							h->temp = 0;
							do {
								PT_YIELD(h->state);
								if (!isdigit(key))
									break;
								h->temp = (h->temp*10) + (key - '0');
							} while (1);
							// now handle last key of command
							switch (key) {
								// arrow keys
								case 'A': goto previous_line;
								case 'B': goto next_line;
								case 'C': goto next_char;
								case 'D': goto previous_char;
								default: printf("Unknown Key '%d%c'\r\n", h->temp, key);	break;
							}
						}	break;
						default:
							printf("Unknown ESC key '%c'\r\n", key);	break;
							break;
					}
				}	break;
			}
		}	break;
		default: {
			insert = 1;
			cursor_move = 1;
		}	break;
	}

	if ((insert || delete) && h->current != h->tail) {
		line_dup(h, h->current);
	}
	line_p l = h->current;
	if (insert) {
		if (l->len + insert >= l->size) {
			while (l->len + insert >= l->size)
				l->size += 8;
			l->line = realloc(l->line, l->size);
		}
		if (l->pos < l->len)
			memmove(l->line + l->pos + insert, l->line + l->pos,
				l->len - l->pos + insert);
		l->line[l->pos] = key;
		l->len += insert;
		l->line[l->len] = 0;
	}
	if (cursor_move) {
		if (l->pos + cursor_move < 0) {
			cursor_move = 0;
			delete = 0;
			l->pos = 0;
		}
		if (l->pos + cursor_move > l->len) {
			cursor_move = 0;
			l->pos = l->len;
		}
		l->pos += cursor_move;
	}
	if (delete) {
		if (l->len - l->pos) {
			memmove(l->line + l->pos, l->line + l->pos + delete, l->len - l->pos - delete);
			l->len -= delete;
		}
	}
	history_display(h);
	PT_END(h->state);
	return res;
}


void
history_init(
		history_p h)
{
	atexit(tty_atexit);

	/* store current tty settings in orig_termios */
	if (tcgetattr(h->ttyout, &orig_termios) < 0)
		fatal("can't get tty settings");
	atexit(tty_atexit);
	tty_raw();

	int fl = fcntl(h->ttyin, F_GETFL);
	fcntl(0, F_SETFL, fl | O_ASYNC);
	history_display(h);
}

int
history_idle(
		history_p h)
{
	uint8_t c;
	ssize_t r = read(h->ttyin, &c, 1);
	if (r <= 0)
		return 0;
	return line_key(h, c);
}
