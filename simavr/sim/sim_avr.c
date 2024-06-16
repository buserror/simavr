/*
	sim_avr.c

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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include "sim_avr.h"
#include "sim_core.h"
#include "sim_time.h"
#include "sim_gdb.h"
#include "avr_uart.h"
#include "sim_vcd_file.h"
#include "avr/avr_mcu_section.h"

#define AVR_KIND_DECL
#include "sim_core_decl.h"

static void
std_logger(
		avr_t * avr,
		const int level,
		const char * format,
		va_list ap);
static avr_logger_p _avr_global_logger = std_logger;

void
avr_global_logger(
		struct avr_t* avr,
		const int level,
		const char * format,
		... )
{
	va_list args;
	va_start(args, format);
	if (_avr_global_logger)
		_avr_global_logger(avr, level, format, args);
	va_end(args);
}

void
avr_global_logger_set(
		avr_logger_p logger)
{
	_avr_global_logger = logger ? logger : std_logger;
}

avr_logger_p
avr_global_logger_get(void)
{
	return _avr_global_logger;
}

uint64_t
avr_get_time_stamp(
		avr_t * avr )
{
	uint64_t stamp;
#ifndef CLOCK_MONOTONIC_RAW
	/* CLOCK_MONOTONIC_RAW isn't portable, here is the POSIX alternative.
	 * Only downside is that it will drift if the system clock changes */
	struct timeval tv;
	gettimeofday(&tv, NULL);
	stamp = (((uint64_t)tv.tv_sec) * 1E9) + (tv.tv_usec * 1000);
#else
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
	stamp = (tp.tv_sec * 1E9) + tp.tv_nsec;
#endif
	if (!avr->time_base)
		avr->time_base = stamp;
	return stamp - avr->time_base;
}

int
avr_init(
		avr_t * avr)
{
	avr->flash = malloc(avr->flashend + 4);
	memset(avr->flash, 0xff, avr->flashend + 1);
	*((uint16_t*)&avr->flash[avr->flashend + 1]) = AVR_OVERFLOW_OPCODE;
	avr->codeend = avr->flashend;
	avr->data = malloc(avr->ramend + 1);
	memset(avr->data, 0, avr->ramend + 1);
#ifdef CONFIG_SIMAVR_TRACE
	avr->trace_data = calloc(1, sizeof(struct avr_trace_data_t));
        avr->trace_data->data_names_size = avr->ioend + 1;
#endif
	avr->data_names = calloc(avr->ioend + 1, sizeof (char *));
	/* put "something" in the serial number */
#ifdef _WIN32
	uint32_t r = getpid() + (uint32_t) rand();
#else
	uint32_t r = getpid() + random();
#endif
	for (int i = 0; i < ARRAY_SIZE(avr->serial); i++)
		avr->serial[i] = r >> (i * 3);
	AVR_LOG(avr, LOG_TRACE, "%s init\n", avr->mmcu);
	AVR_LOG(avr, LOG_TRACE, "   serial %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			avr->serial[0], avr->serial[1], avr->serial[2], avr->serial[3],
			avr->serial[4], avr->serial[5], avr->serial[6], avr->serial[7],
			avr->serial[8]);

	// cpu is in limbo before init is finished.
	avr->state = cpu_Limbo;
	avr->frequency = 1000000;	// can be overridden via avr_mcu_section
	avr_cmd_init(avr);
	avr_interrupt_init(avr);
	if (avr->custom.init)
		avr->custom.init(avr, avr->custom.data);
	if (avr->init)
		avr->init(avr);
	// set default (non gdb) fast callbacks
	avr->run = avr_callback_run_raw;
	avr->sleep = avr_callback_sleep_raw;
	// number of address bytes to push/pull on/off the stack
	avr->address_size = avr->eind ? 3 : 2;
	avr->log = LOG_ERROR;
	avr_reset(avr);
	avr_regbit_set(avr, avr->reset_flags.porf);		// by  default set to power-on reset
	return 0;
}

void
avr_terminate(
		avr_t * avr)
{
	if (avr->custom.deinit)
		avr->custom.deinit(avr, avr->custom.data);
	if (avr->gdb) {
		avr_deinit_gdb(avr);
		avr->gdb = NULL;
	}
	if (avr->vcd) {
		avr_vcd_close(avr->vcd);
		avr->vcd = NULL;
	}
	avr_deallocate_ios(avr);

	if (avr->flash) free(avr->flash);
	if (avr->data) free(avr->data);
	if (avr->io_console_buffer.buf) {
		avr->io_console_buffer.len = 0;
		avr->io_console_buffer.size = 0;
		free(avr->io_console_buffer.buf);
		avr->io_console_buffer.buf = NULL;
	}
	avr->flash = avr->data = NULL;
}

void
avr_reset(
		avr_t * avr)
{
	AVR_LOG(avr, LOG_TRACE, "%s reset\n", avr->mmcu);

	avr->state = cpu_Running;
	for(int i = 0x20; i <= avr->ioend; i++)
		avr->data[i] = 0;
	_avr_sp_set(avr, avr->ramend);
	avr->pc = avr->reset_pc;	// Likely to be zero
	for (int i = 0; i < 8; i++)
		avr->sreg[i] = 0;
	avr_interrupt_reset(avr);
	avr_cycle_timer_reset(avr);
	if (avr->reset)
		avr->reset(avr);
	avr_io_t * port = avr->io_port;
	while (port) {
		if (port->reset)
			port->reset(port);
		port = port->next;
	}
	avr->cycle = 0; // Prevent crash
}

void
avr_sadly_crashed(
		avr_t *avr,
		uint8_t signal)
{
	AVR_LOG(avr, LOG_ERROR, "%s\n", __FUNCTION__);
	avr->state = cpu_Stopped;
	if (avr->gdb_port) {
		// enable gdb server, and wait
		if (!avr->gdb)
			avr_gdb_init(avr);
	}
	if (!avr->gdb)
		avr->state = cpu_Crashed;
}

void
avr_set_command_register(
		avr_t * avr,
		avr_io_addr_t addr)
{
	avr_cmd_set_register(avr, addr);
}

static void
_avr_io_console_write(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	if (v == '\r' && avr->io_console_buffer.buf) {
		avr->io_console_buffer.buf[avr->io_console_buffer.len] = 0;
		AVR_LOG(avr, LOG_OUTPUT, "O:" "%s" "" "\n",
			avr->io_console_buffer.buf);
		avr->io_console_buffer.len = 0;
		return;
	}
	if (avr->io_console_buffer.len + 1 >= avr->io_console_buffer.size) {
		avr->io_console_buffer.size += 128;
		avr->io_console_buffer.buf = (char*)realloc(
			avr->io_console_buffer.buf,
			avr->io_console_buffer.size);
	}
	if (v >= ' ')
		avr->io_console_buffer.buf[avr->io_console_buffer.len++] = v;
}

void
avr_set_console_register(
		avr_t * avr,
		avr_io_addr_t addr)
{
	if (addr)
		avr_register_io_write(avr, addr, _avr_io_console_write, NULL);
}

void
avr_loadcode(
		avr_t * avr,
		uint8_t * code,
		uint32_t size,
		avr_flashaddr_t address)
{
	if ((address + size) > avr->flashend+1) {
		AVR_LOG(avr, LOG_ERROR, "avr_loadcode(): Attempted to load code of size %d but flash size is only %d.\n",
			size, avr->flashend + 1);
		abort();
	}
	memcpy(avr->flash + address, code, size);
}

/**
 * Accumulates sleep requests (and returns a sleep time of 0) until
 * a minimum count of requested sleep microseconds are reached
 * (low amounts cannot be handled accurately).
 */
uint32_t
avr_pending_sleep_usec(
		avr_t * avr,
		avr_cycle_count_t howLong)
{
	avr->sleep_usec += avr_cycles_to_usec(avr, howLong);
	uint32_t usec = avr->sleep_usec;
	if (usec > 200) {
		avr->sleep_usec = 0;
		return usec;
	}
	return 0;
}

void
avr_callback_sleep_gdb(
		avr_t * avr,
		avr_cycle_count_t howLong)
{
	uint32_t usec = avr_pending_sleep_usec(avr, howLong);
	while (avr_gdb_processor(avr, usec))
		;
}

void
avr_callback_run_gdb(
		avr_t * avr)
{
	avr_gdb_processor(avr, avr->state == cpu_Stopped ? 50000 : 0);

	if (avr->state == cpu_Stopped)
		return ;

	// if we are stepping one instruction, we "run" for one..
	int step = avr->state == cpu_Step;
	if (step)
		avr->state = cpu_Running;

	avr_flashaddr_t new_pc = avr->pc;

	if (avr->state == cpu_Running) {
		new_pc = avr_run_one(avr);
#if CONFIG_SIMAVR_TRACE
		avr_dump_state(avr);
#endif
	}

	// run the cycle timers, get the suggested sleep time
	// until the next timer is due
	avr_cycle_count_t sleep = avr_cycle_timer_process(avr);

	avr->pc = new_pc;

	if (avr->state == cpu_Sleeping) {
		if (!avr->sreg[S_I]) {
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
	if (avr->state == cpu_Running || avr->state == cpu_Sleeping)
		avr_service_interrupts(avr);

	// if we were stepping, use this state to inform remote gdb
	if (step)
		avr->state = cpu_StepDone;
}

/*
To avoid simulated time and wall clock time to diverge over time
this function tries to keep them in sync (roughly) by sleeping
for the time required to match the expected sleep deadline
in wall clock time.
*/
void
avr_callback_sleep_raw(
		avr_t *avr,
		avr_cycle_count_t how_long)
{
	/* figure out how long we should wait to match the sleep deadline */
	uint64_t deadline_ns = avr_cycles_to_nsec(avr, avr->cycle + how_long);
	uint64_t runtime_ns = avr_get_time_stamp(avr);
	if (runtime_ns >= deadline_ns)
		return;
	uint64_t sleep_us = (deadline_ns - runtime_ns) / 1000;
	usleep(sleep_us);
	return;
}

void
avr_callback_run_raw(
		avr_t * avr)
{
	avr_flashaddr_t new_pc = avr->pc;

	if (avr->state == cpu_Running) {
		new_pc = avr_run_one(avr);
#if CONFIG_SIMAVR_TRACE
		avr_dump_state(avr);
#endif
	}

	// run the cycle timers, get the suggested sleep time
	// until the next timer is due
	avr_cycle_count_t sleep = avr_cycle_timer_process(avr);

	avr->pc = new_pc;

	if (avr->state == cpu_Sleeping) {
		if (!avr->sreg[S_I]) {
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


int
avr_run(
		avr_t * avr)
{
	avr->run(avr);
	return avr->state;
}

avr_t *
avr_core_allocate(
		const avr_t * core,
		uint32_t coreLen)
{
	uint8_t * b = malloc(coreLen);
	memcpy(b, core, coreLen);
	return (avr_t *)b;
}

avr_t *
avr_make_mcu_by_name(
		const char *name)
{
	avr_kind_t * maker = NULL;
	for (int i = 0; avr_kind[i] && !maker; i++) {
		for (int j = 0; avr_kind[i]->names[j]; j++)
			if (!strcmp(avr_kind[i]->names[j], name)) {
				maker = avr_kind[i];
				break;
			}
	}
	if (!maker) {
		AVR_LOG(((avr_t*)0), LOG_ERROR, "%s: AVR '%s' not known\n", __FUNCTION__, name);
		return NULL;
	}

	avr_t * avr = maker->make();
	AVR_LOG(avr, LOG_TRACE, "Starting %s - flashend %04x ramend %04x e2end %04x\n",
			avr->mmcu, avr->flashend, avr->ramend, avr->e2end);
	return avr;
}

static void
std_logger(
		avr_t * avr,
		const int level,
		const char * format,
		va_list ap)
{
	if (!avr || avr->log >= level) {
		vfprintf((level < LOG_ERROR) ?  stdout : stderr, format, ap);
	}
}

