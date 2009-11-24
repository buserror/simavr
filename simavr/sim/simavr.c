/*
	simavr.c

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

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "simavr.h"
#include "sim_elf.h"

#include "sim_core.h"
#include "avr_eeprom.h"

void hdump(const char *w, uint8_t *b, size_t l)
{
	uint32_t i;
	if (l < 16) {
		printf("%s: ",w);
		for (i = 0; i < l; i++) printf("%02x",b[i]);
	} else {
		printf("%s:\n",w);
		for (i = 0; i < l; i++) {
			if (!(i & 0x1f)) printf("    ");
			printf("%02x",b[i]);
			if ((i & 0x1f) == 0x1f) {
				printf(" ");
				printf("\n");
			}
		}
	}
	printf("\n");
}



int avr_init(avr_t * avr)
{
	avr->flash = malloc(avr->flashend + 1);
	memset(avr->flash, 0xff, avr->flashend + 1);
	avr->data = malloc(avr->ramend + 1);
	memset(avr->data, 0, avr->ramend + 1);

	avr->state = cpu_Running;
	avr->frequency = 1000000;	// can be overriden via avr_mcu_section
	
	if (avr->init)
		avr->init(avr);
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
			port->reset(avr, port);
		port = port->next;
	}

}

int avr_ioctl(avr_t *avr, uint32_t ctl, void * io_param)
{
	avr_io_t * port = avr->io_port;
	int res = -1;
	while (port && res == -1) {
		if (port->ioctl)
			res = port->ioctl(avr, port, ctl, io_param);
		port = port->next;
	}
	return res;
}

void avr_register_io(avr_t *avr, avr_io_t * io)
{
	io->next = avr->io_port;
	avr->io_port = io;
}

void avr_register_io_read(avr_t *avr, uint8_t addr, avr_io_read_t readp, void * param)
{
	avr->ior[AVR_DATA_TO_IO(addr)].param = param;
	avr->ior[AVR_DATA_TO_IO(addr)].r = readp;
}

void avr_register_io_write(avr_t *avr, uint8_t addr, avr_io_write_t writep, void * param)
{
	avr->iow[AVR_DATA_TO_IO(addr)].param = param;
	avr->iow[AVR_DATA_TO_IO(addr)].w = writep;
}

void avr_register_vector(avr_t *avr, avr_int_vector_t * vector)
{
	if (vector->vector)
		avr->vector[vector->vector] = vector;
}

int avr_has_pending_interupts(avr_t * avr)
{
	return avr->pending[0] || avr->pending[1];
}

int avr_is_interupt_pending(avr_t * avr, avr_int_vector_t * vector)
{
	return avr->pending[vector->vector >> 5] & (1 << (vector->vector & 0x1f));
}

void avr_raise_interupt(avr_t * avr, avr_int_vector_t * vector)
{
	if (!vector->vector)
		return;
//	printf("%s raising %d\n", __FUNCTION__, vector->vector);
	if (vector->enable.reg) {
		if (!avr_regbit_get(avr, vector->enable))
			return;
	}
	if (!avr_is_interupt_pending(avr, vector)) {
		if (!avr->pending_wait)
			avr->pending_wait = 2;		// latency on interupts ??
		avr->pending[vector->vector >> 5] |= (1 << (vector->vector & 0x1f));

		if (vector->raised.reg)
			avr_regbit_set(avr, vector->raised);
		if (avr->state != cpu_Running) {
		//	printf("Waking CPU due to interrupt\n");
			avr->state = cpu_Running;	// in case we were sleeping
		}
	}
}

static void avr_clear_interupt(avr_t * avr, int v)
{
	avr_int_vector_t * vector = avr->vector[v];
	avr->pending[v >> 5] &= ~(1 << (v & 0x1f));
	if (!vector)
		return;
	printf("%s cleared %d\n", __FUNCTION__, vector->vector);
	if (vector->raised.reg)
		avr_regbit_clear(avr, vector->raised);
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
#if 0
	/*
	 * this only happend when the compiler is doctoring the stack before calls. Or
	 * if there is an invalid pointer somewhere...
	 */
	if (addr > _avr_sp_get(avr)) {
		avr->trace++;
		STATE("\e[31mmunching stack SP %04x, A=%04x <= %02x\e[0m\n", _avr_sp_get(avr), addr, v);
		avr->trace--;
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

/*
 * check wether interupts are pending. I so, check if the interupt "latency" is reached,
 * and if so triggers the handlers and jump to the vector.
 */
static void avr_service_interupts(avr_t * avr)
{
	if (!avr->sreg[S_I])
		return;

	if (avr_has_pending_interupts(avr)) {
		if (avr->pending_wait) {
			avr->pending_wait--;
			if (avr->pending_wait == 0) {
				int done = 0;
				for (int bi = 0; bi < 2 && !done; bi++) if (avr->pending[bi]) {
					for (int ii = 0; ii < 32 && !done; ii++)
						if (avr->pending[bi] & (1 << ii)) {

							int v = (bi * 32) + ii;	// vector

						//	printf("%s calling %d\n", __FUNCTION__, v);
							_avr_push16(avr, avr->pc >> 1);
							avr->sreg[S_I] = 0;
							avr->pc = v * avr->vector_size;

							avr_clear_interupt(avr, v);
							done++;
							break;
						}
					break;
				}
			}
		} else
			avr->pending_wait = 2;	// for next one...
	}
}


int avr_run(avr_t * avr)
{
	if (avr->state == cpu_Stopped)
		return avr->state;

	uint16_t new_pc = avr->pc;

	if (avr->state == cpu_Running) {
		new_pc = avr_run_one(avr);
		avr_dump_state(avr);
	} else
		avr->cycle ++;

	// re-synth the SREG
	//SREG();
	// if we just re-enabled the interrupts...
	if (avr->sreg[S_I] && !(avr->data[R_SREG] & (1 << S_I))) {
	//	printf("*** %s: Renabling interupts\n", __FUNCTION__);
		avr->pending_wait++;
	}
	avr_io_t * port = avr->io_port;
	while (port) {
		if (port->run)
			port->run(avr, port);
		port = port->next;
	}

	avr->pc = new_pc;

	if (avr->state == cpu_Sleeping) {
		if (!avr->sreg[S_I]) {
			printf("simavr: sleeping with interupts off, quitting gracefuly\n");
			exit(0);
		}
		usleep(500);
		long sleep = (float)avr->frequency * (1.0f / 500.0f);
		avr->cycle += sleep;
	//	avr->state = cpu_Running;
	}
	// Interrupt servicing might change the PC too
	if (avr->state == cpu_Running || avr->state == cpu_Sleeping) {
		avr_service_interupts(avr);

		avr->data[R_SREG] = 0;
		for (int i = 0; i < 8; i++)
			if (avr->sreg[i] > 1) {
				printf("** Invalid SREG!!\n");
				CRASH();
			} else if (avr->sreg[i])
				avr->data[R_SREG] |= (1 << i);
	}
	return avr->state;
}

extern avr_kind_t tiny85;
extern avr_kind_t mega48,mega88,mega168;
extern avr_kind_t mega644;

avr_kind_t * avr_kind[] = {
	&tiny85,
	&mega48,
	&mega88,
	&mega168,
	&mega644,
	NULL
};

int main(int argc, const char **argv)
{
	elf_firmware_t f;

	elf_read_firmware(argv[1], &f);

	printf("firmware %s f=%ld mmcu=%s\n", argv[1], f.mmcu.f_cpu, f.mmcu.name);

	avr_kind_t * maker = NULL;
	for (int i = 0; avr_kind[i] && !maker; i++) {
		for (int j = 0; avr_kind[i]->names[j]; j++)
			if (!strcmp(avr_kind[i]->names[j], f.mmcu.name)) {
				maker = avr_kind[i];
				break;
			}
	}
	if (!maker) {
		fprintf(stderr, "%s: AVR '%s' now known\n", argv[0], f.mmcu.name);
		exit(1);
	}

	avr_t * avr = maker->make();
	printf("Starting %s - flashend %04x ramend %04x e2end %04x\n", avr->mmcu, avr->flashend, avr->ramend, avr->e2end);
	avr_init(avr);
	avr->frequency = f.mmcu.f_cpu;
	avr->codeline = f.codeline;
	avr_loadcode(avr, f.flash, f.flashsize, 0);
	avr->codeend = f.flashsize - f.datasize;
	if (f.eeprom && f.eesize) {
		avr_eeprom_desc_t d = { .ee = f.eeprom, .offset = 0, .size = f.eesize };
		avr_ioctl(avr, AVR_IOCTL_EEPROM_SET, &d);
	}
//	avr->trace = 1;

	for (long long i = 0; i < 8000000*10; i++)
//	for (long long i = 0; i < 80000; i++)
		avr_run(avr);
	
}
