/*
	avr_lin.h

	Copyright 2008, 2011 Michel Pollet <buserror@gmail.com>
	Copyright 2011 Markus Lampert  <mlampert@telus.net>

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
#include "avr_lin.h"
#include "sim_time.h"


static void
avr_lin_baud_write(
        struct avr_t *avr,
        avr_io_addr_t addr,
        uint8_t v,
        void *param)
{
	avr_lin_t *p = (avr_lin_t*) param;

	if (p->r_linbtr != p->ldisr.reg || p->r_linbtr != p->lbt.reg) { // sanity check
		AVR_LOG(avr, LOG_ERROR, "LIN: LDISR and LBT[x] register different!\n");
		return;
	}

	AVR_LOG(avr, LOG_TRACE, "LIN: addr[%04x] = %02x\n", addr, v);
	if (addr == p->ldisr.reg) {
		if (avr_regbit_get(avr, p->lena)) {
			AVR_LOG(avr, LOG_WARNING, "LIN: LENA bit set on changing LBTR\n");
			return;
		}
		if ((v >> p->ldisr.bit) & p->ldisr.mask) {
			uint8_t lbt = (v >> p->lbt.bit) & p->lbt.mask;
			uint8_t ov = v;
			v = (1 << p->ldisr.bit) | (lbt << p->lbt.bit);
			AVR_LOG(avr, LOG_TRACE, "LIN: v=%02x -> LBT = %02x -> LINBT = %02x\n", ov, lbt, v);
		} else {
			v = 0x20;
		}
	}
	avr_core_watch_write(avr, addr, v); // actually set the value

	uint32_t lbt = avr_regbit_get(avr, p->lbt); // Min value CANNOT be zero
	uint32_t lbrr = (avr->data[p->r_linbrrh] << 8) | avr->data[p->r_linbrrl];
	AVR_LOG(avr, LOG_TRACE, "LIN: UART LBT/LBRR to %04x/%04x\n", lbt, lbrr);
	// there is no division by zero case here, lbt is >= 8
	//uint32_t baud = avr->frequency / (lbt * (lbrr + 1));
	uint32_t word_size = 1 /*start*/+ 8 /*data bits*/+ 1 /*parity*/+ 1 /*stop*/;
	int cycles_per_bit = lbt * (lbrr + 1);
	double baud = ((double)avr->frequency) / cycles_per_bit; // can be less than 1
	p->uart.cycles_per_byte = cycles_per_bit * word_size;

	AVR_LOG(avr, LOG_TRACE, "LIN: UART configured to %04x/%04x = %.4f bps, 8 data 1 stop\n", lbt,
	        lbrr, baud);

	//p->uart.cycles_per_byte = 1000000 / (baud / word_size);
	AVR_LOG(avr, LOG_TRACE, "LIN: Roughly %d usec per byte\n",
			avr_cycles_to_usec(avr, p->uart.cycles_per_byte));
}

static void
avr_lin_reset(
		avr_io_t *port)
{
	avr_lin_t *p = (avr_lin_t*) port;
	avr_t * avr = p->io.avr;

	AVR_LOG(avr, LOG_TRACE, "LIN: UART: reset\n");

	p->uart.io.reset(&p->uart.io);
	avr->data[p->r_linbtr] = 0x20;
}

static avr_io_t _io = {
		.kind = "lin",
		.reset = avr_lin_reset,
};

void
avr_lin_init(
		avr_t *avr,
		avr_lin_t *p)
{
	// init uart part
	avr_uart_init(avr, &p->uart);

	p->io = _io;
	avr_register_io_write(avr, p->r_linbtr, avr_lin_baud_write, p);
	avr_register_io_write(avr, p->r_linbrrl, avr_lin_baud_write, p);
}
