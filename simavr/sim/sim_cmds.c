/*
	sim_cmds.c

	Copyright 2014 Florian Albrechtskirchinger <falbrechtskirchinger@gmail.com>

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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "sim_avr.h"
#include "sim_cycle_counters.h"
#include "sim_cmds.h"
#include "sim_vcd_file.h"
#include "avr_uart.h"
#include "avr/avr_mcu_section.h"

#define LOG_PREFIX		"CMDS: "

// Internal struct for simple multi-byte commands with no further state
typedef struct avr_cmd_multibyte_ctx_t {
	int byte;
	avr_cycle_count_t overhead;
} avr_cmd_multibyte_ctx_t;

static void
_avr_cmd_io_write(
		avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_cmd_table_t * commands = &avr->commands;
	avr_cmd_t * command = commands->pending;

	AVR_LOG(avr, LOG_TRACE, LOG_PREFIX "%s: 0x%02x\n", __FUNCTION__, v);

	if(!command) {
		if(v > MAX_AVR_COMMANDS) {
			AVR_LOG(avr, LOG_ERROR, LOG_PREFIX "%s: code 0x%02x outside permissible range (>0x%02x)\n", __FUNCTION__, v, MAX_AVR_COMMANDS - 1);
			return;
		}

		command = &commands->table[v];
	}

	if(command) {
		if(command->handler(avr, v, command->param))
			commands->pending = command;
		else
			commands->pending = NULL;
	}
	else
		AVR_LOG(avr, LOG_TRACE, LOG_PREFIX "%s: unknown command 0x%02x\n", __FUNCTION__, v);
}

void
avr_cmd_set_register(
		avr_t * avr,
		avr_io_addr_t addr)
{
	if(addr)
		avr_register_io_write(avr, addr, &_avr_cmd_io_write, NULL);
}

void
avr_cmd_register(
		avr_t * avr,
		uint8_t code,
		avr_cmd_handler_t handler,
		void * param)
{
	avr_cmd_table_t * commands = &avr->commands;
	avr_cmd_t * command;

	if(!handler)
		return;

	if(code > MAX_AVR_COMMANDS) {
		AVR_LOG(avr, LOG_ERROR, LOG_PREFIX "%s: code 0x%02x outside permissible range (>0x%02x)\n", __FUNCTION__, code, MAX_AVR_COMMANDS - 1);
		return;
	}

	command = &commands->table[code];
	if(command->handler) {
		AVR_LOG(avr, LOG_ERROR, LOG_PREFIX "%s: code 0x%02x is already registered\n", __FUNCTION__, code);
		return;
	}

	command->handler = handler;
	command->param = param;
}

void
avr_cmd_unregister(
		avr_t * avr,
		uint8_t code)
{
	avr_cmd_table_t * commands = &avr->commands;
	avr_cmd_t * command;

	if(code > MAX_AVR_COMMANDS) {
		AVR_LOG(avr, LOG_ERROR, LOG_PREFIX "%s: code 0x%02x outside permissible range (>0x%02x)\n", __FUNCTION__, code, MAX_AVR_COMMANDS - 1);
		return;
	}

	command = &commands->table[code];
	if(command->handler) {
		if(command->param)
			free(command->param);

		command->handler = NULL;
		command->param = NULL;
	}
	else
		AVR_LOG(avr, LOG_ERROR, LOG_PREFIX "%s: no command registered for code 0x%02x\n", __FUNCTION__, code);
}

static int
_simavr_cmd_vcd_start_trace(
		avr_t * avr,
		uint8_t v,
		void * param)
{
	if(avr->vcd)
		avr_vcd_start(avr->vcd);

	return 0;
}

static int
_simavr_cmd_vcd_stop_trace(
		avr_t * avr,
		uint8_t v,
		void * param)
{
	if(avr->vcd)
		avr_vcd_stop(avr->vcd);

	return 0;
}

static int
_simavr_cmd_uart_loopback(
		avr_t * avr,
		uint8_t v,
		void * param)
{
	avr_irq_t * src = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT);
	avr_irq_t * dst = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_INPUT);

	if(src && dst) {
		AVR_LOG(avr, LOG_TRACE, LOG_PREFIX "%s: activating uart local echo; IRQ src %p dst %p\n",
				__FUNCTION__, src, dst);
		avr_connect_irq(src, dst);
	}

	return 0;
}

static avr_cycle_count_t
_avr_cmd_calculate_overhead(
		avr_t * avr)
{
	/*
	 * The overhead is calculated by examining the current and previous
	 * instructions, making a few assumptions:
	 *
	 * - The current instruction has to be 'out' and consumes one clock cycle.
	 *   Because of this, overhead is initialized to 1.
	 * - To write to an IO address, the value has to be first loaded into a
	 *   register using 'ldi' or by clearing the register using 'eor'.
	 * - If the value isn't loaded immediately in the instruction before, the
	 *   value has already been placed into the register as part of the regular
	 *   program flow and is not considered overhead.
	 *
	 * Any compiler or hand-written code can of course easily violate these
	 * assumptions and break the overhead accounting. You have been warned.
	 */
	uint8_t out_r;
	uint32_t opcode;
	avr_cycle_count_t overhead = 1;

	opcode = (avr->flash[avr->pc + 1] << 8) | avr->flash[avr->pc];
	assert((opcode & 0xf800) == 0xb800);
	out_r = (opcode >> 4) & 0x1f;

	opcode = (avr->flash[avr->old_pc + 1] << 8) | avr->flash[avr->old_pc];
	// 'ldi'
	if((opcode & 0xf000) == 0xe000 && (16 + ((opcode >> 4) & 0xf) == out_r))
		++overhead;
	// 'eor'
	else if((opcode & 0xfc00) == 0x2400
			&& (((opcode >> 5) & 0x10) | (opcode & 0xf)) == out_r
			&& ((opcode >> 4) & 0x1f) == out_r)
		++overhead;

	return overhead;
}

static int
_simavr_cmd_start_cycle_counter(
		avr_t * avr,
		uint8_t v,
		void * param)
{
	avr_cmd_multibyte_ctx_t * ctx = (avr_cmd_multibyte_ctx_t *)param;

	if(ctx->byte == 0) {
		ctx->overhead += _avr_cmd_calculate_overhead(avr);
		++ctx->byte;
	}
	else if(ctx->byte == 1) {
		ctx->overhead += _avr_cmd_calculate_overhead(avr);

		avr_cycle_counter_start_with_overhead(avr, v, ctx->overhead);

		ctx->overhead = 0;
		ctx->byte = 0;
	}

	return ctx->byte;
}

static int
_simavr_cmd_stop_cycle_counter(
		avr_t * avr,
		uint8_t v,
		void * param)
{
	avr_cycle_count_t overhead = _avr_cmd_calculate_overhead(avr);

	avr_cycle_counter_stop_with_overhead(avr, overhead);

	return 0;
}

void
avr_cmd_init(
		avr_t * avr)
{
	avr_cmd_multibyte_ctx_t * cycle_counter_ctx;

	memset(&avr->commands, 0, sizeof(avr->commands));

	cycle_counter_ctx = malloc(sizeof(*cycle_counter_ctx));
	memset(cycle_counter_ctx, 0, sizeof(*cycle_counter_ctx));

	// Register builtin commands
	avr_cmd_register(avr, SIMAVR_CMD_VCD_START_TRACE, &_simavr_cmd_vcd_start_trace, NULL);
	avr_cmd_register(avr, SIMAVR_CMD_VCD_STOP_TRACE, &_simavr_cmd_vcd_stop_trace, NULL);
	avr_cmd_register(avr, SIMAVR_CMD_UART_LOOPBACK, &_simavr_cmd_uart_loopback, NULL);
	avr_cmd_register(avr, SIMAVR_CMD_START_CYCLE_COUNTER, &_simavr_cmd_start_cycle_counter, cycle_counter_ctx);
	avr_cmd_register(avr, SIMAVR_CMD_STOP_CYCLE_COUNTER, &_simavr_cmd_stop_cycle_counter, NULL);
}
