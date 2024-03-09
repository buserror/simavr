/*
	test_atmega88_pullups_test.c

	Copyright 2017 Al Popov

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
#include "tests.h"
#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_core.h"
#include "avr_uart.h"
#include "avr_ioport.h"
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

enum {
	IRQ_UART_TERM_BYTE_IN = 0,
	IRQ_UART_TERM_BYTE_OUT,
	IRQ_UART_TERM_COUNT
};

typedef struct uart_TERM_t {
	avr_irq_t *	irq;		// irq list
} uart_TERM_t;

static const char * irq_names[IRQ_UART_TERM_COUNT] = {
	[IRQ_UART_TERM_BYTE_IN] = "8<uart_TERM.in",
	[IRQ_UART_TERM_BYTE_OUT] = "8>uart_TERM.out",
};

uart_TERM_t uart_term;

struct output_buffer {
	char *str;
	int currlen;
	int alloclen;
	int maxlen;
	int head;
	avr_t *avr;
};

static void
buf_output_cb(struct avr_irq_t *irq, uint32_t value, void *param) {
	struct output_buffer *buf = param;
	if (!buf)
		fail("Internal error: buf == NULL in buf_output_cb()");
	if (buf->currlen > buf->alloclen-1)
		fail("Internal error");
	if (buf->alloclen == 0)
		fail("Internal error");
	if (buf->currlen == buf->alloclen-1) {
		buf->alloclen *= 2;
		buf->str = realloc(buf->str, buf->alloclen);
	}
	buf->str[buf->currlen++] = value;
	buf->str[buf->currlen] = 0;
	if (value == '\n') {
		buf->head = buf->currlen;
		avr_irq_t * dst = avr_io_getirq(buf->avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_INPUT);
		avr_raise_irq(dst, '1');
		avr_raise_irq(dst, '\n');
	} else if (value == '\r') {
		if ((buf->str[buf->head] == '~')
			&& (buf->head < (buf->currlen-2))) {
			// simulate the button press or release on the specified pin
			int m_pinN = buf->str[buf->head + 2] - '0';
			avr_irq_t *irq = avr_io_getirq( buf->avr, AVR_IOCTL_IOPORT_GETIRQ('D'), m_pinN );
			if (buf->str[buf->head + 1] == 'p') {
#ifdef AVR_IOPORT_INTRN_PULLUP_IMP
				// If we wish to take a control over internally pulled-up input pin,
				// that is, we have a "low output impedance source" connected to the pin,
				// we must explicitly inform simavr about it.
				avr_irq_t *src_imp_irq = avr_io_getirq(buf->avr, AVR_IOCTL_IOPORT_GETIRQ('D'), m_pinN + IOPORT_IRQ_PIN0_SRC_IMP);
				avr_raise_irq_float(src_imp_irq, 0, 1);
				// Otherwise simavr internall pull-ups handling is active and will "override" the pin state in some situations.
#endif //AVR_IOPORT_INTRN_PULLUP_IMP
				avr_raise_irq(irq, 0);
			} else if (buf->str[buf->head + 1] == 'r') {
				avr_raise_irq(irq, 1);
			}
		}
	}
}

static void init_output_buffer(struct output_buffer *buf, avr_t *avr) {
	buf->str = malloc(128);
	buf->str[0] = 0;
	buf->currlen = 0;
	buf->alloclen = 128;
	buf->maxlen = 4096;
	buf->head = 0;
	buf->avr = avr;
}

static void
test_assert_uart_receive_avr(avr_t *avr,
				   unsigned long run_usec,
				   const char *expected
				   ) {
	struct output_buffer buf;
	init_output_buffer(&buf, avr);

	memset(&uart_term, 0, sizeof(uart_term));

	uart_term.irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_UART_TERM_COUNT, irq_names);
	avr_irq_register_notify(uart_term.irq + IRQ_UART_TERM_BYTE_IN, buf_output_cb, &buf);
	//avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT), buf_output_cb, &buf);
	avr_irq_t * src = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT);
	avr_connect_irq(src, uart_term.irq + IRQ_UART_TERM_BYTE_IN);

	enum tests_finish_reason reason = tests_run_test(avr, run_usec);
	if (reason == LJR_CYCLE_TIMER) {
		if (strcmp(buf.str, expected) == 0) {
			_fail(NULL, 0, "Simulation did not finish within %lu simulated usec. "
				 "UART output is correct and complete.", run_usec);
		}
		_fail(NULL, 0, "Simulation did not finish within %lu simulated usec. "
			 "UART output so far: \"%s\"", run_usec, buf.str);
	}
	if (strcmp(buf.str, expected) != 0)
		_fail(NULL, 0, "UART outputs differ: expected \"%s\", got \"%s\"", expected, buf.str);
}

int main(int argc, char **argv) {
	tests_init(argc, argv);

	static const char *expected =
		"Read internally pulled-up input pins on PIND2:4 and mirror its state to PORTD5:7 outputs\r\n"
		"PIND2 expected=HIGH, actual=HIGH\r\n"
		"PIND3 expected=HIGH, actual=HIGH\r\n"
		"PIND4 expected=HIGH, actual=HIGH\r\n"
		"~p2\r\n"
		"PIND2 expected=LOW, actual=LOW\r\n"
		"PIND3 expected=HIGH, actual=HIGH\r\n"
		"PIND4 expected=HIGH, actual=HIGH\r\n"
		"~r2\r\n"
		"PIND2 expected=HIGH, actual=HIGH\r\n"
		"PIND3 expected=HIGH, actual=HIGH\r\n"
		"PIND4 expected=HIGH, actual=HIGH\r\n"
		"~p3\r\n"
		"PIND2 expected=HIGH, actual=HIGH\r\n"
		"PIND3 expected=LOW, actual=LOW\r\n"
		"PIND4 expected=HIGH, actual=HIGH\r\n"
		"~r3\r\n"
		"PIND2 expected=HIGH, actual=HIGH\r\n"
		"PIND3 expected=HIGH, actual=HIGH\r\n"
		"PIND4 expected=HIGH, actual=HIGH\r\n"
		"~p4\r\n"
		"PIND2 expected=HIGH, actual=HIGH\r\n"
		"PIND3 expected=HIGH, actual=HIGH\r\n"
		"PIND4 expected=LOW, actual=LOW\r\n"
		"~r4\r\n"
		"PIND2 expected=HIGH, actual=HIGH\r\n"
		"PIND3 expected=HIGH, actual=HIGH\r\n"
		"PIND4 expected=HIGH, actual=HIGH\r\n"
			;
	avr_t *avr = tests_init_avr("atmega88_pullups_test.axf");

	test_assert_uart_receive_avr(avr, 5000000L,
				  expected);

	tests_success();
	return 0;
}
