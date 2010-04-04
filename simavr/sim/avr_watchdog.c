/*
	avr_watchdog.c

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
#include "avr_watchdog.h"

static avr_cycle_count_t avr_watchdog_timer(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_watchdog_t * p = (avr_watchdog_t *)param;

	printf("WATCHDOG timer fired.\n");
	avr_raise_interrupt(avr, &p->watchdog);

	if (!avr_regbit_get(avr, p->watchdog.enable)) {
		printf("WATCHDOG timer fired and interrupt is not enabled. Quitting\n");
		avr_sadly_crashed(avr, 10);
	}

	return 0;
}

static avr_cycle_count_t avr_wdce_clear(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_watchdog_t * p = (avr_watchdog_t *)param;
	avr_regbit_clear(p->io.avr, p->wdce);
	return 0;
}

static void avr_watchdog_write(avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_watchdog_t * p = (avr_watchdog_t *)param;
	// backup the registers
	// uint8_t wd = avr->data[p->wdce.reg];
	uint8_t wdce_o = avr_regbit_get(avr, p->wdce);	// old
	uint8_t wde_o = avr_regbit_get(avr, p->wde);
	uint8_t wdp_o[4];

//	printf("avr_watchdog_write %02x\n", v);
	for (int i = 0; i < 4; i++)
		wdp_o[i] = avr_regbit_get(avr, p->wdp[i]);

	avr->data[p->wdce.reg] = v;
	uint8_t wdce_n = avr_regbit_get(avr, p->wdce);	// new

	if (wdce_o /* || wdce_n */) {
		// make sure bit gets reset eventually
		if (wdce_n)
			avr_cycle_timer_register(avr, 4, avr_wdce_clear, p);

		uint8_t wdp = avr_regbit_get_array(avr, p->wdp, 4);
		p->cycle_count = 2048 << wdp;
		p->cycle_count = (p->cycle_count * avr->frequency) / 128000;
		if (avr_regbit_get(avr, p->wde)) {
			printf("Watchdog reset to %d cycles @ 128kz (* %d) = %d CPU cycles)\n", 2048 << wdp, 1 << wdp, (int)p->cycle_count);
			avr_cycle_timer_register(avr, p->cycle_count, avr_watchdog_timer, p);
		} else {
			printf("Watchdog disabled\n");
			avr_cycle_timer_cancel(avr, avr_watchdog_timer, p);
		}
	} else {
		// reset old values
		avr_regbit_setto(avr, p->wde, wde_o);
		for (int i = 0; i < 4; i++)
			avr_regbit_setto(avr, p->wdp[i], wdp_o[i]);
		v = avr->data[p->wdce.reg];
	}
	avr_core_watch_write(avr, addr, v);
}

/*
 * called by the core when a WTD instruction is found
 */
static int avr_watchdog_ioctl(struct avr_io_t * port, uint32_t ctl, void * io_param)
{
	avr_watchdog_t * p = (avr_watchdog_t *)port;
	int res = -1;

	if (ctl == AVR_IOCTL_WATCHDOG_RESET) {
		if (avr_regbit_get(p->io.avr, p->wde))
			avr_cycle_timer_register(p->io.avr, p->cycle_count, avr_watchdog_timer, p);
		res = 0;
	}

	return res;
}

static void avr_watchdog_reset(avr_io_t * port)
{
//	avr_watchdog_t * p = (avr_watchdog_t *)port;

}

static	avr_io_t	_io = {
	.kind = "watchdog",
	.reset = avr_watchdog_reset,
	.ioctl = avr_watchdog_ioctl,
};

void avr_watchdog_init(avr_t * avr, avr_watchdog_t * p)
{
	p->io = _io;

	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->watchdog);

	avr_register_io_write(avr, p->wdce.reg, avr_watchdog_write, p);
}

