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

static void avr_watchdog_run_callback_software_reset(avr_t * avr)
{
	avr_reset(avr);
}

static avr_cycle_count_t avr_watchdog_timer(
		struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_watchdog_t * p = (avr_watchdog_t *)param;

	if (avr_regbit_get(avr, p->watchdog.enable)) {
		AVR_LOG(avr, LOG_TRACE, "WATCHDOG: timer fired.\n");
		avr_raise_interrupt(avr, &p->watchdog);
		return when + p->cycle_count;
	} else if (avr_regbit_get(avr, p->wde)) {
		AVR_LOG(avr, LOG_TRACE,
				"WATCHDOG: timer fired without interrupt. Resetting\n");

		p->reset_context.avr_run = avr->run;
		p->reset_context.wdrf = 1;

		/* Ideally we would perform a reset here via 'avr_reset'
		 * However, returning after reset would result in an unconsistent state.
		 * It seems our best (and cleanest) solution is to set a temporary call 
		 * back which can safely perform the reset for us...  During reset,
		 * the previous callback can be restored and safely resume.
		 */
		avr->run = avr_watchdog_run_callback_software_reset;
	}

	return 0;
}

static avr_cycle_count_t avr_wdce_clear(
		struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_watchdog_t * p = (avr_watchdog_t *)param;
	avr_regbit_clear(p->io.avr, p->wdce);
	return 0;
}

static void avr_watchdog_set_cycle_count_and_timer(
	avr_t * avr, 
	avr_watchdog_t * p, 
	uint8_t was_enabled, 
	int8_t old_wdp)
{
	// If nothing else, always ensure we have a valid cycle count...
	uint8_t wdp = avr_regbit_get_array(avr, p->wdp, 4);

	p->cycle_count = 2048 << wdp;
	p->cycle_count = (p->cycle_count * avr->frequency) / 128000;

	uint8_t wde = avr_regbit_get(avr, p->wde);
	uint8_t wdie = avr_regbit_get(avr, p->watchdog.enable);

	uint8_t enable_changed = (was_enabled != (wde || wdie));

	uint8_t wdp_changed = ((old_wdp >= 0) ? (wdp != old_wdp) : 0);
	
	if (!enable_changed && !wdp_changed)
		return;

	static char *message[2][2] = {
			{ 0, "reset" }, { "enabled", "enabled and set" } };

	if (wde || wdie) {
		AVR_LOG(avr, LOG_TRACE,
				"WATCHDOG: %s to %d cycles @ 128kz (* %d) = %d CPU cycles.\n",
				message[enable_changed][wdp_changed], 2048 << wdp,
				1 << wdp, (int)p->cycle_count);

		avr_cycle_timer_register(avr, p->cycle_count, avr_watchdog_timer, p);
	} else if (enable_changed) {
		AVR_LOG(avr, LOG_TRACE, "WATCHDOG: disabled\n");
		avr_cycle_timer_cancel(avr, avr_watchdog_timer, p);
	}
}

static void avr_watchdog_write(
		avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_watchdog_t * p = (avr_watchdog_t *)param;

	uint8_t old_wde = avr_regbit_get(avr, p->wde);
	uint8_t old_wdie = avr_regbit_get(avr, p->watchdog.enable);
	uint8_t old_wdce = avr_regbit_get(avr, p->wdce);
	
	uint8_t was_enabled = (old_wde || old_wdie);

	uint8_t old_v = avr->data[addr]; // allow gdb to see write...
	avr_core_watch_write(avr, addr, v);

	if (old_wdce) {
		uint8_t old_wdp = avr_regbit_get_array(avr, p->wdp, 4);

		// wdrf (watchdog reset flag) must be cleared before wde can be cleared.
		if (avr_regbit_get(avr, p->wdrf))
			avr_regbit_set(avr, p->wde);

		avr_watchdog_set_cycle_count_and_timer(avr, p, was_enabled, old_wdp);
	} else {
		/* easier to change only what we need rather than check and reset 
		 * locked/read-only bits.
		 */
		avr->data[addr] = old_v;
		
		uint8_t wdce_v = avr_regbit_from_value(avr, p->wdce, v);
		uint8_t wde_v = avr_regbit_from_value(avr, p->wde, v);

		if (wdce_v && wde_v) {
			avr_regbit_set(avr, p->wdce);

			avr_cycle_timer_register(avr, 4, avr_wdce_clear, p);
		} else {
			if (wde_v) // wde can be set but not cleared
				avr_regbit_set(avr, p->wde);

			avr_regbit_setto_raw(avr, p->watchdog.enable, v);

			avr_watchdog_set_cycle_count_and_timer(avr, p, was_enabled, -1);
		}
	}
}

/*
 * called by the core when a WTD instruction is found
 */
static int avr_watchdog_ioctl(
		struct avr_io_t * port, uint32_t ctl, void * io_param)
{
	avr_watchdog_t * p = (avr_watchdog_t *)port;
	int res = -1;

	if (ctl == AVR_IOCTL_WATCHDOG_RESET) {
		if (avr_regbit_get(p->io.avr, p->wde) ||
				avr_regbit_get(p->io.avr, p->watchdog.enable))
			avr_cycle_timer_register(p->io.avr, p->cycle_count,
					avr_watchdog_timer, p);
		res = 0;
	}

	return res;
}

static void avr_watchdog_irq_notify(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	avr_watchdog_t * p = (avr_watchdog_t *)param;
	avr_t * avr = p->io.avr;

	/* interrupt handling calls this twice...
	 * first when raised (during queuing), value = 1
	 * again when cleared (after servicing), value = 0
	 */

	if (!value && avr_regbit_get(avr, p->watchdog.raised)) {
		avr_regbit_clear(avr, p->watchdog.enable);
	}
}
		
static void avr_watchdog_reset(avr_io_t * port)
{
	avr_watchdog_t * p = (avr_watchdog_t *)port;
	avr_t * avr = p->io.avr;

	if (p->reset_context.wdrf) {
		p->reset_context.wdrf = 0;
		/*
		 * if watchdog reset kicked, then watchdog gets restarted at
		 * fastest interval
		 */
		avr->run = p->reset_context.avr_run;

		avr_regbit_set(avr, p->wde);
		avr_regbit_set(avr, p->wdrf);
		avr_regbit_set_array_from_value(avr, p->wdp, 4, 0);
		
		avr_watchdog_set_cycle_count_and_timer(avr, p, 0, 0);
	}
	/* TODO could now use the two pending/running IRQs to do the same
	 * as before */
	avr_irq_register_notify(p->watchdog.irq, avr_watchdog_irq_notify, p);
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

	p->reset_context.wdrf = 0;
}

