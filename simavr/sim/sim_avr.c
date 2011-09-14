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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sim_avr.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "avr_uart.h"
#include "sim_vcd_file.h"
#include "avr_mcu_section.h"


int avr_init(avr_t * avr)
{
	avr->flash = malloc(avr->flashend + 1);
	memset(avr->flash, 0xff, avr->flashend + 1);
	avr->data = malloc(avr->ramend + 1);
	memset(avr->data, 0, avr->ramend + 1);
#ifdef CONFIG_SIMAVR_TRACE
	avr->trace_data = calloc(1, sizeof(struct avr_trace_data_t));
#endif

	// cpu is in limbo before init is finished.
	avr->state = cpu_Limbo;
	avr->frequency = 1000000;	// can be overriden via avr_mcu_section
	if (avr->special_init)
		avr->special_init(avr);
	if (avr->init)
		avr->init(avr);
	// set default (non gdb) fast callbacks
	avr->run = avr_callback_run_raw;
	avr->sleep = avr_callback_sleep_raw;
	avr->state = cpu_Running;
	avr->log = 1;
	avr_reset(avr);	
	return 0;
}

void avr_terminate(avr_t * avr)
{
	if (avr->special_deinit)
		avr->special_deinit(avr);
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
	memset(avr->data, 0x0, avr->ramend + 1);
	_avr_sp_set(avr, avr->ramend);
	avr->pc = 0;
	for (int i = 0; i < 8; i++)
		avr->sreg[i] = 0;
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
	printf("%s\n", __FUNCTION__);
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
	printf("%s %02x\n", __FUNCTION__, v);
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
				printf("%s activating uart local echo IRQ src %p dst %p\n", __FUNCTION__, src, dst);
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
		printf("O:" "%s" "" "\n", buf);
		fflush(stdout);
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

void avr_loadcode(avr_t * avr, uint8_t * code, uint32_t size, uint32_t address)
{
	if (size > avr->flashend+1) {
		fprintf(stderr, "avr_loadcode(): Attempted to load code of size %d but flash size is only %d.\n",
			size, avr->flashend+1);
		abort();
	}
	memcpy(avr->flash + address, code, size);
}

void avr_callback_sleep_gdb(avr_t * avr, avr_cycle_count_t howLong)
{
	uint32_t usec = avr_cycles_to_usec(avr, howLong);
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
	
	uint16_t new_pc = avr->pc;

	if (avr->state == cpu_Running) {
		new_pc = avr_run_one(avr);
#if CONFIG_SIMAVR_TRACE
		avr_dump_state(avr);
#endif
	}

	// if we just re-enabled the interrupts...
	// double buffer the I flag, to detect that edge
	if (avr->sreg[S_I] && !avr->i_shadow)
		avr->pending_wait++;
	avr->i_shadow = avr->sreg[S_I];

	// run the cycle timers, get the suggested sleep time
	// until the next timer is due
	avr_cycle_count_t sleep = avr_cycle_timer_process(avr);

	avr->pc = new_pc;

	if (avr->state == cpu_Sleeping) {
		if (!avr->sreg[S_I]) {
			if (avr->log)
				printf("simavr: sleeping with interrupts off, quitting gracefully\n");
			avr_terminate(avr);
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
	uint32_t usec = avr_cycles_to_usec(avr, howLong);
	usleep(usec);
}

void avr_callback_run_raw(avr_t * avr)
{

	uint16_t new_pc = avr->pc;

	if (avr->state == cpu_Running) {
		new_pc = avr_run_one(avr);
#if CONFIG_SIMAVR_TRACE
		avr_dump_state(avr);
#endif
	}

	// if we just re-enabled the interrupts...
	// double buffer the I flag, to detect that edge
	if (avr->sreg[S_I] && !avr->i_shadow)
		avr->pending_wait++;
	avr->i_shadow = avr->sreg[S_I];

	// run the cycle timers, get the suggested sleeo time
	// until the next timer is due
	avr_cycle_count_t sleep = avr_cycle_timer_process(avr);

	avr->pc = new_pc;

	if (avr->state == cpu_Sleeping) {
		if (!avr->sreg[S_I]) {
			if (avr->log)
				printf("simavr: sleeping with interrupts off, quitting gracefully\n");
			avr_terminate(avr);
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


extern avr_kind_t tiny13;
extern avr_kind_t tiny2313;
extern avr_kind_t tiny25,tiny45,tiny85;
extern avr_kind_t tiny24,tiny44,tiny84;
extern avr_kind_t mega8;
extern avr_kind_t mega48,mega88,mega168,mega328;
extern avr_kind_t mega164,mega324,mega644;
extern avr_kind_t mega128;
extern avr_kind_t mega1281;
extern avr_kind_t mega16m1;

avr_kind_t * avr_kind[] = {
	&tiny13,
	&tiny2313,
	&tiny25, &tiny45, &tiny85,
	&tiny24, &tiny44, &tiny84,
	&mega8,
	&mega48, &mega88, &mega168, &mega328,
	&mega164, &mega324, &mega644,
	&mega128,
	&mega1281,
	&mega16m1,
	NULL
};

avr_t * avr_make_mcu_by_name(const char *name)
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
		fprintf(stderr, "%s: AVR '%s' now known\n", __FUNCTION__, name);
		return NULL;
	}

	avr_t * avr = maker->make();
	printf("Starting %s - flashend %04x ramend %04x e2end %04x\n", avr->mmcu, avr->flashend, avr->ramend, avr->e2end);
	return avr;	
}

