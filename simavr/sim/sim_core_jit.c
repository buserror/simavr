/*
	sim_core_jit.c

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

#define _GNU_SOURCE         /* for asprintf */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "sim_avr.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "avr_flash.h"

#define _avr_invalid_opcode(a) {}

static inline uint16_t
_avr_flash_read16le(
	avr_t * avr,
	avr_flashaddr_t addr)
{
	return(avr->flash[addr] | (avr->flash[addr + 1] << 8));
}


static inline int _avr_is_instruction_32_bits(avr_t * avr, avr_flashaddr_t pc)
{
	uint16_t o = _avr_flash_read16le(avr, pc) & 0xfc0f;
	return	o == 0x9200 || // STS ! Store Direct to Data Space
			o == 0x9000 || // LDS Load Direct from Data Space
			o == 0x940c || // JMP Long Jump
			o == 0x940d || // JMP Long Jump
			o == 0x940e ||  // CALL Long Call to sub
			o == 0x940f; // CALL Long Call to sub
}

typedef struct buf_t {
	char * buffer;
	size_t b_len, b_size;
} buf_t, *buf_p;

void
buf_add_len(
	buf_p b, const char * t, int l)
{
	size_t newsize = b->b_size;

	while (b->b_len + l >= newsize)
		newsize += 256;
	if (newsize != b->b_size) {
		b->buffer = realloc(b->buffer, newsize);
		b->b_size = newsize;
	}
	memcpy(b->buffer + b->b_len, t, l + 1);
	b->b_len += l;
}

void
buf_add(
	buf_p b, const char * t)
{
	buf_add_len(b, t, strlen(t));
}

avr_flashaddr_t
avr_translate_firmware(
	avr_t * avr)
{
	buf_t code = {0}, literal = {0};
	buf_t jump_table = {0};

	avr_flashaddr_t	pc = 0;

	void jit_generate_head(uint16_t o) {
		char * b;
		if (asprintf(&b, "f%04x: {\nconst uint16_t opcode = 0x%04x;\n", pc, o)) {};
		buf_add(&code, b); free(b);
		buf_add(&code, "cycle++;");
		if (asprintf(&b, "new_pc = 0x%04x + 2;\n", pc)) {};
		buf_add(&code, b); free(b);
	//	if (asprintf(&b, "printf(\"%04x: %04x; cycle %%d/%%d\\n\",cycle,howLong);\n", pc, o));
	//	buf_add(&code, b); free(b);
		/* this is gcc/tcc 'label as value' C extension. Handy */
		if (asprintf(&b, "[0x%04x]= &&f%04x,\n", pc/2, pc)) {};
		buf_add(&jump_table, b); free(b);
	}
	void jit_generate_tail(uint16_t o) {
		buf_add(&code, "if (*is || cycle >= howLong) goto exit;\n}\n");
	}
	void jit_generate_literal(const char * fmt, ...) {
		char * b;
		va_list ap;
		va_start(ap, fmt);
		if (vasprintf(&b, fmt, ap)) {};
		va_end(ap);
		buf_add(&literal, b);
		buf_add(&literal, ";\n");
		free(b);
	}
	void jit_literal_flush() {
		if (literal.b_len) {
			buf_add(&code, literal.buffer);
			literal.b_len = 0;
		}
	}
	void jit_generate(uint16_t o, const char * t) {
		jit_generate_head(o);
		jit_literal_flush();
		const char * l = t;
		do {
			const char * nl = index(l, '\n');
			if (strncmp(l, "STATE", 5) || avr->trace)
				buf_add_len(&code, l, nl-l+1);
			l = nl + 1;
		} while (*l);
		jit_generate_tail(o);
	}
	do {
		avr_flashaddr_t	new_pc = pc + 2;
		uint16_t opcode = _avr_flash_read16le(avr, pc);
		const int avr_rampz = avr->rampz;
		const int avr_eind = avr->eind;

		#include "sim_core_jit.h"
		pc = new_pc;
	} while (pc < avr->codeend);

	printf("Code size: %ld\n", code.b_len);

	FILE *o = fopen("jit_code.c", "w");
	fprintf(o, "static const void * jt[%d] = {\n%s};\n", pc/2, jump_table.buffer);
	/* goto current 'pc' */
	fprintf(o, "TRACE_JUMP();\n");
	fwrite(code.buffer, code.b_len, 1, o);
	fclose(o);
	free(code.buffer);
	free(literal.buffer);
	free(jump_table.buffer);
	return 0;
}


void
avr_callback_run_jit(
	avr_t * avr)
{
	avr_flashaddr_t new_pc = avr->pc;

	if (avr->state == cpu_Running) {
		int cycle = 0;
		new_pc = avr->jit.entry(
				avr->jit.jit_avr,
				new_pc,
				&avr->state,
				&avr->interrupt_state,
				&cycle, 200);
		avr->cycle += cycle;
	}

	// run the cycle timers, get the suggested sleep time
	// until the next timer is due
	avr_cycle_count_t sleep = avr_cycle_timer_process(avr);

	avr->pc = new_pc;

	if (avr->state == cpu_Sleeping) {
#ifndef SREG_BIT
		if (!avr->sreg[S_I]) {
#else
		if (!SREG_BIT(S_I)) {
#endif
			if (avr->log)
				AVR_LOG(avr, LOG_TRACE, "simavr: sleeping with interrupts off, quitting gracefully\n");
			avr->state = cpu_Done;
			return;
		}
		/*
		 * try to sleep for as long as we can (?)
		 */
		avr->sleep(avr, sleep);
		avr->cycle += 1 + sleep;
	}
	// Interrupt servicing might change the PC too, during 'sleep'
	if (avr->state == cpu_Running || avr->state == cpu_Sleeping) {
		/* Note: checking interrupt_state here is completely superfluous, however
			as interrupt_state tells us all we really need to know, here
			a simple check here may be cheaper than a call not needed. */
		if (avr->interrupt_state)
			avr_service_interrupts(avr);
	}
}


