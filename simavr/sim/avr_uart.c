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

#ifdef NO_COLOR
	#define FONT_GREEN		
	#define FONT_DEFAULT	
#else
	#define FONT_GREEN		"\e[32m"
	#define FONT_DEFAULT	"\e[0m"
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include "avr_uart.h"
#include "sim_hex.h"

DEFINE_FIFO(uint8_t, uart_fifo, 64);

static avr_cycle_count_t avr_uart_txc_raise(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;
	if (avr_regbit_get(avr, p->txen)) {
		// if the interrupts are not used, still raise the UDRE and TXC flag
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

static uint8_t avr_uart_rxc_read(struct avr_t * avr, avr_io_addr_t addr, void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;
	uint8_t v = avr_core_watch_read(avr, addr);

	//static uint8_t old = 0xff; if (v != old) printf("UCSRA read %02x\n", v); old = v;
	//
	// if RX is enabled, and there is nothing to read, and
	// the AVR core is reading this register, it's probably
	// to poll the RXC TXC flag and spinloop
	// so here we introduce a usleep to make it a bit lighter
	// on CPU and let data arrive
	//
	uint8_t ri = !avr_regbit_get(avr, p->rxen) || !avr_regbit_get(avr, p->rxc.raised);
	uint8_t ti = !avr_regbit_get(avr, p->txen) || !avr_regbit_get(avr, p->txc.raised);

	if (p->flags & AVR_UART_FLAG_POOL_SLEEP) {

		if (ri && ti)
			usleep(1);
	}
	// if reception is idle and the fifo is empty, tell whomever there is room
	if (avr_regbit_get(avr, p->rxen))
		avr_raise_irq(p->io.irq + UART_IRQ_OUT_XON, uart_fifo_isempty(&p->input) != 0);

	return v;
}

static uint8_t avr_uart_read(struct avr_t * avr, avr_io_addr_t addr, void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;

	// clear the rxc bit in case the code is using polling
	avr_regbit_clear(avr, p->rxc.raised);

	if (!avr_regbit_get(avr, p->rxen)) {
		avr->data[addr] = 0;
		// made to trigger potential watchpoints
		avr_core_watch_read(avr, addr);
		return 0;
	}
	uint8_t v = uart_fifo_read(&p->input);

	//printf("UART read %02x %s\n", v, uart_fifo_isempty(&p->input) ? "EMPTY!" : "");
	avr->data[addr] = v;
	// made to trigger potential watchpoints
	v = avr_core_watch_read(avr, addr);

	// trigger timer if more characters are pending
	if (!uart_fifo_isempty(&p->input))
		avr_cycle_timer_register_usec(avr, p->usec_per_byte, avr_uart_rxc_raise, p);

	return v;
}

static void avr_uart_baud_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;
	avr_core_watch_write(avr, addr, v);
	uint32_t val = avr->data[p->r_ubrrl] | (avr->data[p->r_ubrrh] << 8);
	uint32_t baud = avr->frequency / (val+1);
	if (avr_regbit_get(avr, p->u2x))
		baud /= 8;
	else
		baud /= 16;

	const int databits[] = { 5,6,7,8,  /* 'reserved', assume 8 */8,8,8, 9 };
	int db = databits[avr_regbit_get(avr, p->ucsz) | (avr_regbit_get(avr, p->ucsz2) << 2)];
	int sb = 1 + avr_regbit_get(avr, p->usbs);
	int word_size = 1 /* start */ + db /* data bits */ + 1 /* parity */ + sb /* stops */;

	printf("UART-%c configured to %04x = %d bps, %d data %d stop\n",
			p->name, val, baud, db, sb);
	// TODO: Use the divider value and calculate the straight number of cycles
	p->usec_per_byte = 1000000 / (baud / word_size);
	printf("Roughtly %d usec per bytes\n", (int)p->usec_per_byte);
}

static void avr_uart_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;

	if (addr == p->r_udr) {
		avr_core_watch_write(avr, addr, v);

		if ( p->udrc.vector)
			avr_regbit_clear(avr, p->udrc.raised);
		avr_cycle_timer_register_usec(avr,
				p->usec_per_byte, avr_uart_txc_raise, p); // should be uart speed dependent

		if (p->flags & AVR_UART_FLAG_STDIO) {
			static char buf[128];
			static int l = 0;
			buf[l++] = v < ' ' ? '.' : v;
			buf[l] = 0;
			if (v == '\n' || l == 127) {
				l = 0;
				printf( FONT_GREEN "%s\n" FONT_DEFAULT, buf);
			}
		}
	//	printf("UDR%c(%02x) = %02x\n", p->name, addr, v);
		// tell other modules we are "outputing" a byte
		if (avr_regbit_get(avr, p->txen))
			avr_raise_irq(p->io.irq + UART_IRQ_OUTPUT, v);
	}
	if (p->udrc.vector && addr == p->udrc.enable.reg) {
		/*
		 * If enabling the UDRC interrupt, raise it immediately if FIFO is empty
		 */
		uint8_t udrce = avr_regbit_get(avr, p->udrc.enable);
		avr_core_watch_write(avr, addr, v);
		uint8_t nudrce = avr_regbit_get(avr, p->udrc.enable);
		if (!udrce && nudrce) {
			// if the FIDO is not empty (clear timer is flying) we don't
			// need to raise the interrupt, it will happen when the timer
			// is fired.
			if (avr_cycle_timer_status(avr, avr_uart_txc_raise, p) == 0)
				avr_raise_interrupt(avr, &p->udrc);
		}
	}
	if (p->udrc.vector && addr == p->udrc.raised.reg) {
		// get the bits before the write
		//uint8_t udre = avr_regbit_get(avr, p->udrc.raised);
		uint8_t txc = avr_regbit_get(avr, p->txc.raised);

		// no need to write this value in here, only the
		// interrupt flags need clearing!
		// avr_core_watch_write(avr, addr, v);

		//avr_clear_interrupt_if(avr, &p->udrc, udre);
		avr_clear_interrupt_if(avr, &p->txc, txc);
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
		avr_cycle_timer_register_usec(avr, p->usec_per_byte, avr_uart_rxc_raise, p); // should be uart speed dependent
	uart_fifo_write(&p->input, value); // add to fifo

//	printf("UART IRQ in %02x (%d/%d) %s\n", value, p->input.read, p->input.write, uart_fifo_isfull(&p->input) ? "FULL!!" : "");

	avr_raise_irq(p->io.irq + UART_IRQ_OUT_XOFF, uart_fifo_isfull(&p->input) != 0);
}


void avr_uart_reset(struct avr_io_t *io)
{
	avr_uart_t * p = (avr_uart_t *)io;
	avr_t * avr = p->io.avr;
	if (p->udrc.vector)
		avr_regbit_set(avr, p->udrc.raised);
	avr_irq_register_notify(p->io.irq + UART_IRQ_INPUT, avr_uart_irq_input, p);
	avr_cycle_timer_cancel(avr, avr_uart_rxc_raise, p);
	avr_cycle_timer_cancel(avr, avr_uart_txc_raise, p);
	uart_fifo_reset(&p->input);

	// DEBUG allow printf without fidding with enabling the uart
	avr_regbit_set(avr, p->txen);
	p->usec_per_byte = 100;
}

static int avr_uart_ioctl(struct avr_io_t * port, uint32_t ctl, void * io_param)
{
	avr_uart_t * p = (avr_uart_t *)port;
	int res = -1;

	if (!io_param)
		return res;

	if (ctl == AVR_IOCTL_UART_SET_FLAGS(p->name)) {
		p->flags = *(uint32_t*)io_param;
		res = 0;
	}
	if (ctl == AVR_IOCTL_UART_GET_FLAGS(p->name)) {
		*(uint32_t*)io_param = p->flags;
		res = 0;
	}

	return res;
}

static const char * irq_names[UART_IRQ_COUNT] = {
	[UART_IRQ_INPUT] = "8<in",
	[UART_IRQ_OUTPUT] = "8>out",
	[UART_IRQ_OUT_XON] = ">xon",
	[UART_IRQ_OUT_XOFF] = ">xoff",
};

static	avr_io_t	_io = {
	.kind = "uart",
	.reset = avr_uart_reset,
	.ioctl = avr_uart_ioctl,
	.irq_names = irq_names,
};

void avr_uart_init(avr_t * avr, avr_uart_t * p)
{
	p->io = _io;

//	printf("%s UART%c UDR=%02x\n", __FUNCTION__, p->name, p->r_udr);

	p->flags = AVR_UART_FLAG_POOL_SLEEP|AVR_UART_FLAG_STDIO;

	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->rxc);
	avr_register_vector(avr, &p->txc);
	avr_register_vector(avr, &p->udrc);

	// allocate this module's IRQ
	avr_io_setirqs(&p->io, AVR_IOCTL_UART_GETIRQ(p->name), UART_IRQ_COUNT, NULL);
	// Only call callbacks when the value change...
	p->io.irq[UART_IRQ_OUT_XOFF].flags |= IRQ_FLAG_FILTERED;
	p->io.irq[UART_IRQ_OUT_XON].flags |= IRQ_FLAG_FILTERED;

	avr_register_io_write(avr, p->r_udr, avr_uart_write, p);
	avr_register_io_read(avr, p->r_udr, avr_uart_read, p);
	// monitor code that reads the rxc flag, and delay it a bit
	avr_register_io_read(avr, p->rxc.raised.reg, avr_uart_rxc_read, p);

	if (p->udrc.vector)
		avr_register_io_write(avr, p->udrc.enable.reg, avr_uart_write, p);
	if (p->r_ucsra)
		avr_register_io_write(avr, p->r_ucsra, avr_uart_write, p);
	if (p->r_ubrrl)
		avr_register_io_write(avr, p->r_ubrrl, avr_uart_baud_write, p);
}

