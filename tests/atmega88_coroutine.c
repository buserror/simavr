/*
	atmega88_coroutine.c

	Copyright 2008-2013 Michel Pollet <buserror@gmail.com>

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

#ifndef __AVR_CR_H__
#define __AVR_CR_H__
/*
 * Smallest coroutine implementation for AVR. Takes
 * 23 + (24 * tasks) bytes of SRAM to run.
 *
 * Use it like:
 *
 * AVR_TASK(mytask1, 32);
 * AVR_TASK(mytask2, 48);
 * ...
 * void my_task_function() {
 *    do {
 *        AVR_YIELD(mytask1, 1);
 *    } while (1);
 * }
 * ...
 * main() {
 *     AVR_TASK_START(mytask1, my_task_function);
 *     AVR_TASK_START(mytask2, my_other_task_function);
 *     do {
 *          AVR_RESUME(mytask1);
 *          AVR_RESUME(mytask2);
 *     } while (1);
 * }
 * NOTE: Do *not* use "static" on the function prototype, otherwise it
 * will fail to link (compiler doesn't realize the "jmp" is referencing)
 */
#include <setjmp.h>
#include <stdint.h>
static inline void _set_stack(register void * stack)
{
	asm volatile (
			"in r0, __SREG__" "\n\t"
			"cli" "\n\t"
			"out __SP_H__, %B0" "\n\t"
			"out __SREG__, r0" "\n\t"
			"out __SP_L__, %A0" "\n\t"
			: : "e" (stack) /* : */
	);
}

jmp_buf	g_caller;
#define AVR_TASK(_name, _stack_size) \
	struct { \
		jmp_buf jmp; \
		uint8_t running : 1; \
		uint8_t stack[_stack_size]; \
	} _name
#define AVR_TASK_START(_name, _entry) \
	if (!setjmp(g_caller)) { \
		_set_stack(_name.stack+sizeof(_name.stack));\
		asm volatile ("rjmp "#_entry); \
	}
#define AVR_YIELD(_name, _sleep) \
	_name.running = !_sleep; \
	if (!setjmp(_name.jmp)) \
		longjmp(g_caller, 1)
#define AVR_RESUME(_name) \
	if (!setjmp(g_caller)) \
		longjmp(_name.jmp, 1)
#endif /* __AVR_CR_H__ */



#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdio.h>

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega88");

static int uart_putchar(char c, FILE *stream)
{
	if (c == '\n')
		uart_putchar('\r', stream);
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);


AVR_TASK(mytask1, 32);
AVR_TASK(mytask2, 48);

uint16_t t = 0;

void task1_function() {
	uint16_t c = 0;
	do {
		c++;
		if (c == 1000) {
			c = 0;
			printf("task1\n");
			t++;
		}
		AVR_YIELD(mytask1, 1);
	} while (1);
}
void task2_function() {
	uint16_t c = 0;
	do {
		c++;
		if (c == 2000) {
			c = 0;
			printf("task2\n");
		}
		AVR_YIELD(mytask2, 1);
	} while (1);
}


int main(void)
{
	stdout = &mystdout;

	sei();                                      // Enable global interrupts
	printf("Starting\n");
	AVR_TASK_START(mytask1, task1_function);
	AVR_TASK_START(mytask2, task2_function);
	do {
		AVR_RESUME(mytask1);
		AVR_RESUME(mytask2);
	} while (t < 500);

	// exit simavr
	cli();
	sleep_mode();
}

