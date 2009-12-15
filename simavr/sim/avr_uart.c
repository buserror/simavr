/*
	avr_uart.c

	Handles UART access
	Right now just handle "write" to the serial port at any speed
	and printf to the console when '\n' is written.

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
#include "avr_uart.h"

DEFINE_FIFO(uint8_t, uart_fifo, 128);

static avr_cycle_count_t avr_uart_txc_raise(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;
	if (avr_regbit_get(avr, p->txen)) {
		// if the interrupts are not used, still raised the UDRE and TXC flaga
		avr_raise_interrupt(avr, &p->udrc);
		avr_raise_interrupt(avr, &p->txc);
	}
	return 0;
}

static avr_cycle_count_t avr_uart_rxc_raise(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;
	if (avr_regbit_get(avr, p->rxen))
		avr_raise_interrupt(avr, &p->rxc);
	return 0;
}

static uint8_t avr_uart_read(struct avr_t * avr, avr_io_addr_t addr, void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;

	if (!avr_regbit_get(avr, p->rxen)) {
		avr->data[addr] = 0;
		// made to trigger potential watchpoints
		avr_core_watch_read(avr, addr);
		return 0;
	}
	uint8_t v = uart_fifo_read(&p->input);

	avr->data[addr] = v;
	// made to trigger potential watchpoints
	v = avr_core_watch_read(avr, addr);

	if (!uart_fifo_isempty(&p->input))
		avr_cycle_timer_register_usec(avr, 100, avr_uart_rxc_raise, p); // should be uart speed dependent

	return v;
}

static void avr_uart_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;

	if (addr == p->r_udr) {
		avr_core_watch_write(avr, addr, v);

		avr_regbit_clear(avr, p->udrc.raised);
		avr_cycle_timer_register_usec(avr, 100, avr_uart_txc_raise, p); // should be uart speed dependent

		static char buf[128];
		static int l = 0;
		buf[l++] = v < ' ' ? '.' : v;
		buf[l] = 0;
		if (v == '\n' || l == 127) {
			l = 0;
			printf("\e[32m%s\e[0m\n", buf);
		}
//		printf("UDR%c(%02x) = %02x\n", p->name, addr, v);
		// tell other modules we are "outputing" a byte
		if (avr_regbit_get(avr, p->txen))
			avr_raise_irq(p->io.irq + UART_IRQ_OUTPUT, v);
	} else {
		// get the bits before the write
		uint8_t udre = avr_regbit_get(avr, p->udrc.raised);
		uint8_t txc = avr_regbit_get(avr, p->txc.raised);

		avr_core_watch_write(avr, addr, v);

		// if writing one to a one, clear bit
		if (udre && avr_regbit_get(avr, p->udrc.raised))
			avr_regbit_clear(avr, p->udrc.raised);
		if (txc && avr_regbit_get(avr, p->txc.raised))
			avr_regbit_clear(avr, p->txc.raised);
	}
}

static void avr_uart_irq_input(struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;
	avr_t * avr = p->io.avr;

	// check to see fi receiver is enabled
	if (!avr_regbit_get(avr, p->rxen))
		return;

	if (uart_fifo_isempty(&p->input))
		avr_cycle_timer_register_usec(avr, 100, avr_uart_rxc_raise, p); // should be uart speed dependent
	uart_fifo_write(&p->input, value); // add to fifo
}


void avr_uart_reset(struct avr_io_t *io)
{
	avr_uart_t * p = (avr_uart_t *)io;
	avr_t * avr = p->io.avr;
	avr_regbit_set(avr, p->udrc.raised);
	avr_irq_register_notify(p->io.irq + UART_IRQ_INPUT, avr_uart_irq_input, p);
	avr_cycle_timer_cancel(avr, avr_uart_rxc_raise, p);
	avr_cycle_timer_cancel(avr, avr_uart_txc_raise, p);
	uart_fifo_reset(&p->input);

	// DEBUG allow printf without fidding with enabling the uart
	avr_regbit_set(avr, p->txen);

}

static	avr_io_t	_io = {
	.kind = "uart",
	.reset = avr_uart_reset,
};

void avr_uart_init(avr_t * avr, avr_uart_t * p)
{
	p->io = _io;
	avr_register_io(avr, &p->io);

//	printf("%s UART%c UDR=%02x\n", __FUNCTION__, p->name, p->r_udr);

	// allocate this module's IRQ
	p->io.irq_count = UART_IRQ_COUNT;
	p->io.irq = avr_alloc_irq(0, p->io.irq_count);
	p->io.irq_ioctl_get = AVR_IOCTL_UART_GETIRQ(p->name);

	avr_register_io_write(avr, p->r_udr, avr_uart_write, p);
	avr_register_io_read(avr, p->r_udr, avr_uart_read, p);

	avr_register_io_write(avr, p->r_ucsra, avr_uart_write, p);
}

