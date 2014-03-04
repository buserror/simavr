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
#include <unistd.h>
#include "sim_avr.h"
#include "sim_core.h"
#include "sim_time.h"
#include "sim_gdb.h"
#include "avr_uart.h"
#include "sim_vcd_file.h"
#include "avr/avr_mcu_section.h"

#define AVR_KIND_DECL
#include "sim_core_decl.h"

static void std_logger(avr_t * avr, const int level, const char * format, va_list ap);
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


int avr_init(avr_t * avr)
{
	avr->flash = malloc(avr->flashend + 1);
	memset(avr->flash, 0xff, avr->flashend + 1);
	avr->data = malloc(avr->ramend + 1);
	memset(avr->data, 0, avr->ramend + 1);
#ifdef CONFIG_SIMAVR_TRACE
	avr->trace_data = calloc(1, sizeof(struct avr_trace_data_t));
#endif
	
	AVR_LOG(avr, LOG_TRACE, "%s init\n", avr->mmcu);

	// cpu is in limbo before init is finished.
	avr->state = cpu_Limbo;
	avr->frequency = 1000000;	// can be overridden via avr_mcu_section
	avr_interrupt_init(avr);
	if (avr->special_init)
		avr->special_init(avr, avr->special_data);
	if (avr->init)
		avr->init(avr);
	// set default (non gdb) fast callbacks
	avr->run = avr_callback_run_raw;
	avr->sleep = avr_callback_sleep_raw;
	avr->state = cpu_Running;
	// number of address bytes to push/pull on/off the stack
	avr->address_size = avr->eind ? 3 : 2;
	avr->log = 1;
	avr_reset(avr);	
	return 0;
}

void avr_terminate(avr_t * avr)
{
	if (avr->special_deinit)
		avr->special_deinit(avr, avr->special_data);
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
	avr->flash = avr->data = NULL;
}

void avr_reset(avr_t * avr)
{
	AVR_LOG(avr, LOG_TRACE, "%s reset\n", avr->mmcu);

	memset(avr->data, 0x0, avr->ramend + 1);
	_avr_sp_set(avr, avr->ramend);
	avr->pc = 0;
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
}

void avr_sadly_crashed(avr_t *avr, uint8_t signal)
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

static void _avr_io_command_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	AVR_LOG(avr, LOG_TRACE, "%s %02x\n", __FUNCTION__, v);
	switch (v) {
		case SIMAVR_CMD_VCD_START_TRACE:
			if (avr->vcd)
				avr_vcd_start(avr->vcd);
			break;
		case SIMAVR_CMD_VCD_STOP_TRACE:
			if (avr->vcd)
				avr_vcd_stop(avr->vcd);
			break;
		case SIMAVR_CMD_UART_LOOPBACK: {
			avr_irq_t * src = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_OUTPUT);
			avr_irq_t * dst = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('0'), UART_IRQ_INPUT);
			if (src && dst) {
				AVR_LOG(avr, LOG_TRACE, "%s activating uart local echo IRQ src %p dst %p\n",
						__FUNCTION__, src, dst);
				avr_connect_irq(src, dst);
			}
		}	break;

	}
}

void avr_set_command_register(avr_t * avr, avr_io_addr_t addr)
{
	if (addr)
		avr_register_io_write(avr, addr, _avr_io_command_write, NULL);
}

static void _avr_io_console_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	static char * buf = NULL;
	static int size = 0, len = 0;

	if (v == '\r' && buf) {
		buf[len] = 0;
		AVR_LOG(avr, LOG_OUTPUT, "O:" "%s" "" "\n", buf);
		len = 0;
		return;
	}
	if (len + 1 >= size) {
		size += 128;
		buf = (char*)realloc(buf, size);
	}
	if (v >= ' ')
		buf[len++] = v;
}

void avr_set_console_register(avr_t * avr, avr_io_addr_t addr)
{
	if (addr)
		avr_register_io_write(avr, addr, _avr_io_console_write, NULL);
}

void avr_loadcode(avr_t * avr, uint8_t * code, uint32_t size, avr_flashaddr_t address)
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

void avr_callback_sleep_gdb(avr_t * avr, avr_cycle_count_t howLong)
{
	uint32_t usec = avr_pending_sleep_usec(avr, howLong);
	while (avr_gdb_processor(avr, usec))
		;
}

void avr_callback_run_gdb(avr_t * avr)
{
	avr_gdb_processor(avr, avr->state == cpu_Stopped);

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

	// if we just re-enabled the interrupts...
	// double buffer the I flag, to detect that edge
	if (avr->sreg[S_I] && !avr->i_shadow)
		avr->interrupts.pending_wait++;
	avr->i_shadow = avr->sreg[S_I];

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

void avr_callback_sleep_raw(avr_t * avr, avr_cycle_count_t howLong)
{
	uint32_t usec = avr_pending_sleep_usec(avr, howLong);
	if (usec > 0) {
		usleep(usec);
	}
}

void avr_callback_run_raw(avr_t * avr)
{
	avr_flashaddr_t new_pc = avr->pc;

	if (avr->state == cpu_Running) {
		new_pc = avr_run_one(avr);
#if CONFIG_SIMAVR_TRACE
		avr_dump_state(avr);
#endif
	}

	// if we just re-enabled the interrupts...
	// double buffer the I flag, to detect that edge
	if (avr->sreg[S_I] && !avr->i_shadow)
		avr->interrupts.pending_wait++;
	avr->i_shadow = avr->sreg[S_I];

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
}


int avr_run(avr_t * avr)
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

static void std_logger(avr_t* avr, const int level, const char * format, va_list ap)
{
	if (!avr || avr->log >= level) {
		vfprintf((level > LOG_ERROR) ?  stdout : stderr , format, ap);
	}
}

