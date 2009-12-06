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


int avr_init(avr_t * avr)
{
	avr->flash = malloc(avr->flashend + 1);
	memset(avr->flash, 0xff, avr->flashend + 1);
	avr->data = malloc(avr->ramend + 1);
	memset(avr->data, 0, avr->ramend + 1);

	// cpu is in limbo before init is finished.
	avr->state = cpu_Limbo;
	avr->frequency = 1000000;	// can be overriden via avr_mcu_section
	if (avr->init)
		avr->init(avr);
	avr->state = cpu_Running;
	avr_reset(avr);	
	return 0;
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
	avr->state = cpu_Stopped;
	if (avr->gdb_port) {
		// enable gdb server, and wait
		if (!avr->gdb)
			avr_gdb_init(avr);
	} 
	if (!avr->gdb)
		exit(1); // no gdb ?
}

void avr_loadcode(avr_t * avr, uint8_t * code, uint32_t size, uint32_t address)
{
	memcpy(avr->flash + address, code, size);
}

void avr_core_watch_write(avr_t *avr, uint16_t addr, uint8_t v)
{
	if (addr > avr->ramend) {
		printf("*** Invalid write address PC=%04x SP=%04x O=%04x Address %04x=%02x out of ram\n",
				avr->pc, _avr_sp_get(avr), avr->flash[avr->pc] | (avr->flash[avr->pc]<<8), addr, v);
		CRASH();
	}
	if (addr < 32) {
		printf("*** Invalid write address PC=%04x SP=%04x O=%04x Address %04x=%02x low registers\n",
				avr->pc, _avr_sp_get(avr), avr->flash[avr->pc] | (avr->flash[avr->pc]<<8), addr, v);
		CRASH();
	}
#if AVR_STACK_WATCH
	/*
	 * this checks that the current "function" is not doctoring the stack frame that is located
	 * higher on the stack than it should be. It's a sign of code that has overrun it's stack
	 * frame and is munching on it's own return address.
	 */
	if (avr->stack_frame_index > 1 && addr > avr->stack_frame[avr->stack_frame_index-2].sp) {
		printf("\e[31m%04x : munching stack SP %04x, A=%04x <= %02x\e[0m\n", avr->pc, _avr_sp_get(avr), addr, v);
	}
#endif
	avr->data[addr] = v;
}

uint8_t avr_core_watch_read(avr_t *avr, uint16_t addr)
{
	if (addr > avr->ramend) {
		printf("*** Invalid read address PC=%04x SP=%04x O=%04x Address %04x out of ram (%04x)\n",
				avr->pc, _avr_sp_get(avr), avr->flash[avr->pc] | (avr->flash[avr->pc]<<8), addr, avr->ramend);
		CRASH();
	}
	return avr->data[addr];
}

// converts a number of usec to a number of machine cycles, at current speed
uint64_t avr_usec_to_cycles(avr_t * avr, uint32_t usec)
{
	return avr->frequency * (uint64_t)usec / 1000000;
}

uint32_t avr_cycles_to_usec(avr_t * avr, uint64_t cycles)
{
	return 1000000 * cycles / avr->frequency;
}

// converts a number of hz (to megahertz etc) to a number of cycle
uint64_t avr_hz_to_cycles(avr_t * avr, uint32_t hz)
{
	return avr->frequency / hz;
}

void avr_cycle_timer_register(avr_t * avr, uint64_t when, avr_cycle_timer_t timer, void * param)
{
	avr_cycle_timer_cancel(avr, timer, param);

	if (avr->cycle_timer_map == 0xffffffff) {
		fprintf(stderr, "avr_cycle_timer_register is full!\n");
		return;
	}
	when += avr->cycle;
	for (int i = 0; i < 32; i++)
		if (!(avr->cycle_timer_map & (1 << i))) {
			avr->cycle_timer[i].timer = timer;
			avr->cycle_timer[i].param = param;
			avr->cycle_timer[i].when = when;
			avr->cycle_timer_map |= (1 << i);
			return;
		}
}

void avr_cycle_timer_register_usec(avr_t * avr, uint32_t when, avr_cycle_timer_t timer, void * param)
{
	avr_cycle_timer_register(avr, avr_usec_to_cycles(avr, when), timer, param);
}

void avr_cycle_timer_cancel(avr_t * avr, avr_cycle_timer_t timer, void * param)
{
	if (!avr->cycle_timer_map)
		return;
	for (int i = 0; i < 32; i++)
		if ((avr->cycle_timer_map & (1 << i)) &&
				avr->cycle_timer[i].timer == timer &&
				avr->cycle_timer[i].param == param) {
			avr->cycle_timer[i].timer = NULL;
			avr->cycle_timer[i].param = NULL;
			avr->cycle_timer[i].when = 0;
			avr->cycle_timer_map &= ~(1 << i);
			return;
		}
}

/*
 * run thru all the timers, call the ones that needs it,
 * clear the ones that wants it, and calculate the next
 * potential cycle we could sleep for...
 */
static uint64_t avr_cycle_timer_check(avr_t * avr)
{
	if (!avr->cycle_timer_map)
		return (uint32_t)-1;

	uint64_t min = (uint64_t)-1;

	for (int i = 0; i < 32; i++) {
		if (!(avr->cycle_timer_map & (1 << i)))
			continue;

		if (avr->cycle_timer[i].when <= avr->cycle) {
			// call it
			avr->cycle_timer[i].when =
					avr->cycle_timer[i].timer(avr,
							avr->cycle_timer[i].when,
							avr->cycle_timer[i].param);
			if (avr->cycle_timer[i].when == 0) {
				// clear it
				avr->cycle_timer[i].timer = NULL;
				avr->cycle_timer[i].param = NULL;
				avr->cycle_timer[i].when = 0;
				avr->cycle_timer_map &= ~(1 << i);
				continue;
			}
		}
		if (avr->cycle_timer[i].when < min)
			min = avr->cycle_timer[i].when;
	}
	return min - avr->cycle;
}

int avr_run(avr_t * avr)
{
	avr_gdb_processor(avr, avr->state == cpu_Stopped);

	if (avr->state == cpu_Stopped)
		return avr->state;

	// if we are stepping one instruction, we "run" for one..
	int step = avr->state == cpu_Step;
	if (step) {
		avr->state = cpu_Running;
	}
	
	uint16_t new_pc = avr->pc;

	if (avr->state == cpu_Running) {
		new_pc = avr_run_one(avr);
		avr_dump_state(avr);
	}

	// if we just re-enabled the interrupts...
	if (avr->sreg[S_I] && !(avr->data[R_SREG] & (1 << S_I))) {
	//	printf("*** %s: Renabling interrupts\n", __FUNCTION__);
		avr->pending_wait++;
	}
	avr_io_t * port = avr->io_port;
	while (port) {
		if (port->run)
			port->run(port);
		port = port->next;
	}
	avr_cycle_count_t sleep = avr_cycle_timer_check(avr);

	avr->pc = new_pc;

	if (avr->state == cpu_Sleeping) {
		if (!avr->sreg[S_I]) {
			printf("simavr: sleeping with interrupts off, quitting gracefully\n");
			exit(0);
		}
		/*
		 * try to sleep for as long as we can (?)
		 */
		uint32_t usec = avr_cycles_to_usec(avr, sleep);
		if (avr->gdb) {
			while (avr_gdb_processor(avr, usec))
				;
		} else
			usleep(usec);
		avr->cycle += sleep;
	}
	// Interrupt servicing might change the PC too
	if (avr->state == cpu_Running || avr->state == cpu_Sleeping) {
		avr_service_interrupts(avr);

		avr->data[R_SREG] = 0;
		for (int i = 0; i < 8; i++)
			if (avr->sreg[i] > 1) {
				printf("** Invalid SREG!!\n");
				CRASH();
			} else if (avr->sreg[i])
				avr->data[R_SREG] |= (1 << i);
	}

	if (step) {
		avr->state = cpu_StepDone;
	}

	return avr->state;
}


extern avr_kind_t tiny13;
extern avr_kind_t tiny25,tiny45,tiny85;
extern avr_kind_t mega48,mega88,mega168;
extern avr_kind_t mega644;

avr_kind_t * avr_kind[] = {
	&tiny13,
	&tiny25,
	&tiny45,
	&tiny85,
	&mega48,
	&mega88,
	&mega168,
	&mega644,
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

