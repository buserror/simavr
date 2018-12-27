/*
	avr_flash.c

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_cycle_timers.h"
#include "avr_flash.h"

static avr_cycle_count_t avr_progen_clear(avr_cycle_timer_pool_t * pool, avr_cycle_count_t when, void * param)
{
	avr_flash_t * p = (avr_flash_t *)param;
	avr_regbit_clear(p->io.avr, p->selfprgen);
	AVR_LOG(p->io.avr, LOG_WARNING, "FLASH: avr_progen_clear - SPM not received, clearing PRGEN bit\n");
	return 0;
}


static void avr_flash_write(avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_flash_t * p = (avr_flash_t *)param;

	avr_core_watch_write(avr, addr, v);

//	printf("** avr_flash_write %02x\n", v);

	if (avr_regbit_get(avr, p->selfprgen))
		if ( avr_cycle_timer_register(&(avr->cycle_timers), 4, avr_progen_clear, p) < 0 ) {; // 4 cycles is very little!
			AVR_LOG(avr, LOG_ERROR, "CYCLE: %s: pool is full (%d)!\n", __func__, MAX_CYCLE_TIMERS);
		}
}

static void avr_flash_clear_temppage(avr_flash_t *p)
{
	for (int i = 0; i < p->spm_pagesize / 2; i++) {
		p->tmppage[i] = 0xff;
		p->tmppage_used[i] = 0;
	}
}

static int avr_flash_ioctl(struct avr_io_t * port, uint32_t ctl, void * io_param)
{
	if (ctl != AVR_IOCTL_FLASH_SPM)
		return -1;

	avr_flash_t * p = (avr_flash_t *)port;
	avr_t * avr = p->io.avr;

	avr_flashaddr_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8);
	if (avr->rampz)
		z |= avr->data[avr->rampz] << 16;
	uint16_t r01 = avr->data[0] | (avr->data[1] << 8);

//	printf("AVR_IOCTL_FLASH_SPM %02x Z:%04x R01:%04x\n", avr->data[p->r_spm], z,r01);
	if (avr_regbit_get(avr, p->selfprgen)) {
		avr_cycle_timer_cancel(&(avr->cycle_timers), avr_progen_clear, p);

		if (avr_regbit_get(avr, p->pgers)) {
			z &= ~1;
			AVR_LOG(avr, LOG_TRACE, "FLASH: Erasing page %04x (%d)\n", (z / p->spm_pagesize), p->spm_pagesize);
			for (int i = 0; i < p->spm_pagesize; i++)
				avr->flash[z++] = 0xff;
		} else if (avr_regbit_get(avr, p->pgwrt)) {
			z &= ~(p->spm_pagesize - 1);
			AVR_LOG(avr, LOG_TRACE, "FLASH: Writing page %04x (%d)\n", (z / p->spm_pagesize), p->spm_pagesize);
			for (int i = 0; i < p->spm_pagesize / 2; i++) {
				avr->flash[z++] = p->tmppage[i];
				avr->flash[z++] = p->tmppage[i] >> 8;
			}
			avr_flash_clear_temppage(p);
		} else if (avr_regbit_get(avr, p->blbset)) {
			AVR_LOG(avr, LOG_TRACE, "FLASH: Setting lock bits (ignored)\n");
		} else if (p->flags & AVR_SELFPROG_HAVE_RWW && avr_regbit_get(avr, p->rwwsre)) {
			avr_flash_clear_temppage(p);
		} else {
			AVR_LOG(avr, LOG_TRACE, "FLASH: Writing temppage %08x (%04x)\n", z, r01);
			z >>= 1;
			if (!p->tmppage_used[z % (p->spm_pagesize / 2)]) {
				p->tmppage[z % (p->spm_pagesize / 2)] = r01;
				p->tmppage_used[z % (p->spm_pagesize / 2)] = 1;
			}
		}
	}
	avr_regbit_clear(avr, p->selfprgen);
	return 0;
}

static void
avr_flash_reset(avr_io_t * port)
{
	avr_flash_t * p = (avr_flash_t *) port;

	avr_flash_clear_temppage(p);
}

static void
avr_flash_dealloc(struct avr_io_t * port)
{
	avr_flash_t * p = (avr_flash_t *) port;

	if (p->tmppage)
		free(p->tmppage);

	if (p->tmppage_used)
		free(p->tmppage_used);
}

static	avr_io_t	_io = {
	.kind = "flash",
	.ioctl = avr_flash_ioctl,
	.reset = avr_flash_reset,
	.dealloc = avr_flash_dealloc,
};

void avr_flash_init(avr_t * avr, avr_flash_t * p)
{
	p->io = _io;
//	printf("%s init SPM %04x\n", __FUNCTION__, p->r_spm);

	if (!p->tmppage)
		p->tmppage = malloc(p->spm_pagesize);

	if (!p->tmppage_used)
		p->tmppage_used = malloc(p->spm_pagesize / 2);

	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->flash);

	avr_register_io_write(avr, p->r_spm, avr_flash_write, p);
}
