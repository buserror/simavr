/*
 * hcmd_symbols.c
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

#include "history_avr.h"

static const char *
demangle(
		const char *symbol)
{
	if (symbol[0] == '_' && symbol[1] == 'Z') {
		symbol += 3;
		while (isdigit(*symbol))
			symbol++;
	}
	return symbol;
}

int
_locate_sym(
		const char *name,
		avr_symbol_t ** out,
		int * outsize)
{
	int size = 1;
	avr_symbol_t * sym = NULL;
	for (int i = 0; i < code.symbolcount && !sym; i++) {
		if (!strcmp(demangle(code.symbol[i]->symbol), name)) {
			sym = code.symbol[i];
			if (i < code.symbolcount-1) {
				if ((code.symbol[i+1]->addr >> 16) == (sym->addr >> 16)) {
					size = code.symbol[i+1]->addr - sym->addr;
				//	printf("deduced size %d\r\n", size);
				}
			}
		}
	}
	*out = sym;
	*outsize = size;

	return sym != NULL;
}

static int
_cmd_print(
		wordexp_t * l)
{
	char *cmd = l->we_wordv[0];
	char * nt = l->we_wordv[1];
	char * st = l->we_wordv[2];

	int size = 1;
	avr_symbol_t * sym = NULL;
	_locate_sym(nt, &sym, &size);

	if (st) {
		if (!strncmp(st, "0x", 2)) {
			st += 2;
			sscanf(st, "%x", &size);
		} else if (st[0] == 'x') {
			st++;
			sscanf(st, "%x", &size);
		} else
			sscanf(st, "%d", &size);
	}

	if (!sym) {
		fprintf(stderr, "%s: invalid symbol name '%s'\r\n", cmd, nt);
	} else if ((sym->addr >> 16) != 0x80) {
		fprintf(stderr, "%s: '%s' is not in sram\r\n", cmd, nt);
	} else {
		int offset = sym->addr & 0xffff;
		printf("%s (%04x): ", demangle(sym->symbol), offset);
		if (size > 23)
			printf("\r\n  ");
		for (int i = 0; i < size; i++, offset++)
			printf("%s%02x%s",
					i > 1 && (i % 32) == 1 ? "  " : "",
					avr->data[offset],
					i > 1 && (i % 32) == 0 ? "\r\n" : " ");
		printf("\r\n");
	}
	return 0;
}


const history_cmd_t cmd_print = {
	.names = { "print", "p" },
	.usage = "<name> [size]: print SRAM variable data",
	.help = "Prints SRAM variable",
	.parameter_map = (1 << 1) | (1 << 2),
	.execute = _cmd_print,
};
HISTORY_CMD_REGISTER(cmd_print);


static int
_cmd_set(
		wordexp_t * l)
{
	if (l->we_wordc < 3) {
		fprintf(stderr, "%s: invalid syntax\r\n", l->we_wordv[0]);
		return 0;
	}
	char *cmd = l->we_wordv[0];
	char * nt = l->we_wordv[1];

	int size = 1;
	avr_symbol_t * sym = NULL;
	_locate_sym(nt, &sym, &size);

	if (!sym) {
		fprintf(stderr, "%s: invalid symbol name '%s'\r\n", cmd, nt);
	} else if ((sym->addr >> 16) != 0x80) {
		fprintf(stderr, "%s: '%s' is not in sram\r\n", cmd, nt);
	} else {
		int offset = sym->addr & 0xffff;
		printf("%s (%04x): ", demangle(sym->symbol), offset);

		for (int i = 2; i < l->we_wordc; i++) {
			char * ht = l->we_wordv[i];

			while (isxdigit(ht[0]) && isxdigit(ht[1])) {
				const char *hex = "0123456789abcdef";
				uint8_t b = ((index(hex, tolower(ht[0])) - hex) << 4) |
						(index(hex, tolower(ht[1])) - hex);
				avr->data[offset++] = b;
				printf("%02x", b);
				ht += 2;
			}
		}
		printf("\r\n");
	}
	return 0;
}


const history_cmd_t cmd_set = {
	.names = { "set" },
	.usage = "<name> <hex values...>: set SRAM variable data",
	.help = "Sets SRAM variable",
	.parameter_map = 0, // any numbers
	.execute = _cmd_set,
};
HISTORY_CMD_REGISTER(cmd_set);

static int
_cmd_dump(
		wordexp_t * l)
{
	int size = 1;
	avr_symbol_t * sym = NULL;
	for (int i = 0; i < code.symbolcount; i++) {
		sym = code.symbol[i];
		size = 0;
		if (i < code.symbolcount-1) {
			if ((code.symbol[i+1]->addr >> 16) == (sym->addr >> 16))
				size = code.symbol[i+1]->addr - sym->addr;
		}
		if ((sym->addr >> 16) == 0x80 &&
					strncmp(sym->symbol, "__", 2) &&
					sym->symbol[0] != ' ' &&
					strcmp(sym->symbol, "_edata") &&
					strcmp(sym->symbol, "_end"))
			printf("%04x %s (%d bytes)\n",
					sym->addr, demangle(sym->symbol), size);
	}

	return 0;
}

const history_cmd_t cmd_dump = {
	.names = { "dump", "du" },
	.usage = "dump SRAM ELF symbols and matching size",
	.help = "dump SRAM ELF symbols and matching size",
	.parameter_map = 0, // any numbers
	.execute = _cmd_dump,
};
HISTORY_CMD_REGISTER(cmd_dump);
