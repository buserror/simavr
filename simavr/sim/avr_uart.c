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
#include <stdlib.h>
#include "avr_uart.h"
#include "sim_hex.h"
#include "sim_time.h"
#include "sim_gdb.h"

//#define TRACE(_w) _w
#ifndef TRACE
#define TRACE(_w)
#endif

DEFINE_FIFO(uint8_t, uart_fifo);

static inline void
avr_uart_clear_interrupt(
		avr_t * avr,
		avr_int_vector_t * vector)
{
	if (!vector->vector)
		return;
	// clear the interrupt flag even it's 'sticky'
	if (avr_regbit_get(avr, vector->raised))
		avr_clear_interrupt_if(avr, vector, 0);
	if (avr_regbit_get(avr, vector->raised))
		avr_regbit_clear(avr, vector->raised);
}

static inline void
avr_uart_regbit_clear(
		avr_t * avr,
		avr_regbit_t rb)
{
	uint16_t a = rb.reg;
	if (!a)
		return;
	avr_regbit_clear(avr, rb);
}

static avr_cycle_count_t
avr_uart_txc_raise(
		struct avr_t * avr,
		avr_cycle_count_t when,
		void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;
	if (p->tx_cnt) {
		// Even if the interrupt is disabled, still raise the TXC flag
		if (p->tx_cnt == 1)
			avr_raise_interrupt(avr, &p->txc);
		p->tx_cnt--;
	}
	if (p->udrc.vector) {// UDRE is disabled in the LIN mode
		if (p->tx_cnt) {
			if (avr_regbit_get(avr, p->udrc.raised)) {
				avr_uart_clear_interrupt(avr, &p->udrc);
			}
		} else {
			if (avr_regbit_get(avr, p->txen)) {
				// Even if the interrupt is disabled, still raise the UDRE flag
				avr_raise_interrupt(avr, &p->udrc);
				if (!avr_regbit_get(avr, p->udrc.enable)) {
					return 0; //polling mode: stop TX pump
				} else // udrc (alias udre) should be rased repeatedly while output buffer is empty
					return when + p->cycles_per_byte;
			} else
				return 0; // transfer disabled: stop TX pump
		}
	}
	if (p->tx_cnt)
		return when + p->cycles_per_byte;
	return 0; // stop TX pump
}

static avr_cycle_count_t
avr_uart_rxc_raise(
		struct avr_t * avr,
		avr_cycle_count_t when,
		void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;
	if (avr_regbit_get(avr, p->rxen)) {
		// rxc should be rased continiosly untill input buffer is empty
		if (!uart_fifo_isempty(&p->input)) {
			if (!avr_regbit_get(avr, p->rxc.raised)) {
				p->rxc_raise_time = when;
				p->rx_cnt = 0;
			}
			avr_raise_interrupt(avr, &p->rxc);
			return when + p->cycles_per_byte;
		}
	}
	return 0;
}

static uint8_t
avr_uart_rxc_read(
		struct avr_t * avr,
		avr_io_addr_t addr,
		void * param)
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

	if (p->flags & AVR_UART_FLAG_POLL_SLEEP) {

		if (ri && ti)
			usleep(1);
	}
	// if reception is idle and the fifo is empty, tell whomever there is room
	if (avr_regbit_get(avr, p->rxen) && uart_fifo_isempty(&p->input)) {
		avr_raise_irq(p->io.irq + UART_IRQ_OUT_XOFF, 0);
		avr_raise_irq(p->io.irq + UART_IRQ_OUT_XON, 1);
	}

	return v;
}

static uint8_t
avr_uart_read(
		struct avr_t * avr,
		avr_io_addr_t addr,
		void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;
	uint8_t v = 0;

	if (!avr_regbit_get(avr, p->rxen) ||
			!avr_regbit_get(avr, p->rxc.raised) // rxc flag not raised - nothing to read!
			) {
		AVR_LOG(avr, LOG_TRACE, "UART%c: attempt to read empty rx buffer\n", p->name);
		avr->data[addr] = 0;
		// made to trigger potential watchpoints
		avr_core_watch_read(avr, addr);
		//return 0;
		goto avr_uart_read_check;
	}
	if (!uart_fifo_isempty(&p->input)) { // probably redundant check
		v = uart_fifo_read(&p->input);
		p->rx_cnt++;
		if ((p->rx_cnt > 1) && // UART actually has 2-character rx buffer
				((avr->cycle-p->rxc_raise_time)/p->rx_cnt < p->cycles_per_byte)) {
			// prevent the firmware from reading input characters with non-realistic high speed
			avr_uart_clear_interrupt(avr, &p->rxc);
			p->rx_cnt = 0;
		}
	} else {
		AVR_LOG(avr, LOG_TRACE, "UART%c: BUG: rxc raised with empty rx buffer\n", p->name);
	}

//	TRACE(printf("UART read %02x %s\n", v, uart_fifo_isempty(&p->input) ? "EMPTY!" : "");)
	avr->data[addr] = v;
	// made to trigger potential watchpoints
	v = avr_core_watch_read(avr, addr);

avr_uart_read_check:
	if (uart_fifo_isempty(&p->input)) {
		avr_cycle_timer_cancel(avr, avr_uart_rxc_raise, p);
		avr_uart_clear_interrupt(avr, &p->rxc);
		avr_raise_irq(p->io.irq + UART_IRQ_OUT_XOFF, 0);
		avr_raise_irq(p->io.irq + UART_IRQ_OUT_XON, 1);
	}
	if (!uart_fifo_isfull(&p->input)) {
		avr_uart_regbit_clear(avr, p->dor);
	}

	return v;
}

static void
avr_uart_baud_write(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;
	avr_core_watch_write(avr, addr, v);
	uint32_t val = avr->data[p->r_ubrrl] | (avr->data[p->r_ubrrh] << 8);

	const int databits[] = { 5,6,7,8,  /* 'reserved', assume 8 */8,8,8, 9 };
	int db = databits[avr_regbit_get(avr, p->ucsz) | (avr_regbit_get(avr, p->ucsz2) << 2)];
	int sb = 1 + avr_regbit_get(avr, p->usbs);
	int word_size = 1 /* start */ + db /* data bits */ + 1 /* parity */ + sb /* stops */;
	int cycles_per_bit = (val+1)*8;
	if (!avr_regbit_get(avr, p->u2x))
		cycles_per_bit *= 2;
	double baud = ((double)avr->frequency) / cycles_per_bit; // can be less than 1
	p->cycles_per_byte = cycles_per_bit * word_size;

	AVR_LOG(avr, LOG_TRACE, "UART: %c configured to %04x = %.4f bps (x%d), %d data %d stop\n",
			p->name, val, baud, avr_regbit_get(avr, p->u2x)?2:1, db, sb);
	AVR_LOG(avr, LOG_TRACE, "UART: Roughly %d usec per byte\n",
			avr_cycles_to_usec(avr, p->cycles_per_byte));
}

static void
avr_uart_udr_write(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;

	// The byte to be sent should NOT be written there,
	// the value written could never be read back.
	//avr_core_watch_write(avr, addr, v);
	if (avr->gdb) {
		avr_gdb_handle_watchpoints(avr, addr, AVR_GDB_WATCH_WRITE);
	}

	//avr_cycle_timer_cancel(avr, avr_uart_txc_raise, p); // synchronize tx pump
	if (p->udrc.vector && avr_regbit_get(avr, p->udrc.raised)) {
		avr_uart_clear_interrupt(avr, &p->udrc);
	}

	if (p->flags & AVR_UART_FLAG_STDIO) {
		const int maxsize = 256;
		if (!p->stdio_out)
			p->stdio_out = malloc(maxsize);
		p->stdio_out[p->stdio_len++] = v < ' ' ? '.' : v;
		p->stdio_out[p->stdio_len] = 0;
		if (v == '\n' || p->stdio_len == maxsize) {
			p->stdio_len = 0;
			AVR_LOG(avr, LOG_OUTPUT,
					FONT_GREEN "%s\n" FONT_DEFAULT, p->stdio_out);
		}
	}
	TRACE(printf("UDR%c(%02x) = %02x\n", p->name, addr, v);)
	// tell other modules we are "outputting" a byte
	if (avr_regbit_get(avr, p->txen)) {
		avr_raise_irq(p->io.irq + UART_IRQ_OUTPUT, v);
		p->tx_cnt++;
		if (p->tx_cnt > 2) // AVR actually has 1-character UART tx buffer, plus shift register
			AVR_LOG(avr, LOG_TRACE,
					"UART%c: tx buffer overflow %d\n",
					p->name, (int)p->tx_cnt);
		if (avr_cycle_timer_status(avr, avr_uart_txc_raise, p) == 0)
			avr_cycle_timer_register(avr, p->cycles_per_byte,
					avr_uart_txc_raise, p); // start the tx pump
	}
}


static void
avr_uart_write(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;

	uint8_t masked_v = v;
	uint8_t clear_txc = 0;
	uint8_t clear_rxc = 0;

	// exclude these locations from direct write:
	if (p->udrc.raised.reg == addr) {
		masked_v &= ~(p->udrc.raised.mask << p->udrc.raised.bit);
		masked_v |= avr_regbit_get_raw(avr, p->udrc.raised);
	}
	if (p->txc.raised.reg == addr) {
		uint8_t mask = p->txc.raised.mask << p->txc.raised.bit;
		masked_v &= ~(mask);
		masked_v |= avr_regbit_get_raw(avr, p->txc.raised);
		// it can be cleared by writing a one to its bit location
		if (v & mask)
			clear_txc = 1;
	}
	if (p->rxc.raised.reg == addr) {
		uint8_t mask = p->rxc.raised.mask << p->rxc.raised.bit;
		masked_v &= ~(mask);
		masked_v |= avr_regbit_get_raw(avr, p->rxc.raised);
		if (!p->udrc.vector) {
			// In the LIN mode it can be cleared by writing a one to its bit location
			if (v & mask)
				clear_rxc = 1;
		}
	}
	// mainly to prevent application to confuse itself
	// by writing something there and reading it back:
	if (p->fe.reg == addr) {
		masked_v &= ~(p->fe.mask << p->fe.bit);
		masked_v |= avr_regbit_get_raw(avr, p->fe);
	}
	if (p->dor.reg == addr) {
		masked_v &= ~(p->dor.mask << p->dor.bit);
		//masked_v |= avr_regbit_get_raw(avr, p->dor);
	}
	if (p->upe.reg == addr) {
		masked_v &= ~(p->upe.mask << p->upe.bit);
		masked_v |= avr_regbit_get_raw(avr, p->upe);
	}
	if (p->rxb8.reg == addr) {
		masked_v &= ~(p->rxb8.mask << p->rxb8.bit);
		masked_v |= avr_regbit_get_raw(avr, p->rxb8);
	}

	uint8_t txen = avr_regbit_get(avr, p->txen);
	uint8_t rxen = avr_regbit_get(avr, p->rxen);
	uint8_t udrce = avr_regbit_get(avr, p->udrc.enable);
	// Now write whatever bits could be writen directly.
	// It is necessary anyway, to trigger potential watchpoints.
	avr_core_watch_write(avr, addr, masked_v);
	uint8_t new_txen = avr_regbit_get(avr, p->txen);
	uint8_t new_rxen = avr_regbit_get(avr, p->rxen);
	uint8_t new_udrce = avr_regbit_get(avr, p->udrc.enable);
	if (p->udrc.vector && (!udrce && new_udrce) && new_txen) {
		// If enabling the UDRC (alias is UDRE) interrupt, raise it immediately if FIFO is empty.
		// If the FIFO is not empty (clear timer is flying) we don't
		// need to raise the interrupt, it will happen when the timer
		// is fired.
		if (avr_cycle_timer_status(avr, avr_uart_txc_raise, p) == 0)
			avr_raise_interrupt(avr, &p->udrc);
	}
	if (clear_txc)
		avr_uart_clear_interrupt(avr, &p->txc);
	if (clear_rxc)
		avr_uart_clear_interrupt(avr, &p->rxc);

	///TODO: handle the RxD & TxD pins function override

	if (new_rxen != rxen) {
		if (new_rxen) {
			if (uart_fifo_isempty(&p->input)) {
				// if reception is enabled and the fifo is empty, tell whomever there is room
				avr_raise_irq(p->io.irq + UART_IRQ_OUT_XOFF, 0);
				avr_raise_irq(p->io.irq + UART_IRQ_OUT_XON, 1);
			}
		} else {
			avr_raise_irq(p->io.irq + UART_IRQ_OUT_XOFF, 1);
			avr_cycle_timer_cancel(avr, avr_uart_rxc_raise, p);
			// flush the Receive Buffer
			uart_fifo_reset(&p->input);
			// clear the rxc interrupt flag
			avr_uart_clear_interrupt(avr, &p->rxc);
		}
	}
	if (new_txen != txen) {
		if (p->udrc.vector && !new_txen) {
			avr_uart_clear_interrupt(avr, &p->udrc);
		}
	}
}

static void
avr_uart_irq_input(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	avr_uart_t * p = (avr_uart_t *)param;
	avr_t * avr = p->io.avr;

	// check to see if receiver is enabled
	if (!avr_regbit_get(avr, p->rxen))
		return;

	// reserved/not implemented:
	//avr_uart_regbit_clear(avr, p->fe);
	//avr_uart_regbit_clear(avr, p->upe);
	//avr_uart_regbit_clear(avr, p->rxb8);

	if (uart_fifo_isempty(&p->input) &&
			(avr_cycle_timer_status(avr, avr_uart_rxc_raise, p) == 0)
			) {
		avr_cycle_timer_register(avr, p->cycles_per_byte, avr_uart_rxc_raise, p); // start the rx pump
		p->rx_cnt = 0;
		avr_uart_regbit_clear(avr, p->dor);
	} else if (uart_fifo_isfull(&p->input)) {
		avr_regbit_setto(avr, p->dor, 1);
	}
	if (!avr_regbit_get(avr, p->dor)) { // otherwise newly received character must be rejected
		uart_fifo_write(&p->input, value); // add to fifo
	} else {
		AVR_LOG(avr, LOG_ERROR, "UART%c: %s: RX buffer overrun, lost char=%c=0x%02X\n", p->name, __func__,
				(char)value, (uint8_t)value );
	}

	TRACE(printf("UART IRQ in %02x (%d/%d) %s\n", value, p->input.read, p->input.write, uart_fifo_isfull(&p->input) ? "FULL!!" : "");)

	if (uart_fifo_isfull(&p->input))
		avr_raise_irq(p->io.irq + UART_IRQ_OUT_XOFF, 1);
}


void
avr_uart_reset(
		struct avr_io_t *io)
{
	avr_uart_t * p = (avr_uart_t *)io;
	avr_t * avr = p->io.avr;
	if (p->udrc.vector) {
		avr_regbit_set(avr, p->udrc.raised);
		avr_uart_regbit_clear(avr, p->dor);
	}
	avr_uart_clear_interrupt(avr, &p->txc);
	avr_uart_clear_interrupt(avr, &p->rxc);
	avr_irq_register_notify(p->io.irq + UART_IRQ_INPUT, avr_uart_irq_input, p);
	avr_cycle_timer_cancel(avr, avr_uart_rxc_raise, p);
	avr_cycle_timer_cancel(avr, avr_uart_txc_raise, p);
	uart_fifo_reset(&p->input);
	p->tx_cnt =  0;

	avr_regbit_set(avr, p->ucsz);
	avr_uart_regbit_clear(avr, p->ucsz2);

	// DEBUG allow printf without fiddling with enabling the uart
	avr_regbit_set(avr, p->txen);
	p->cycles_per_byte = avr_usec_to_cycles(avr, 100);
}

static int
avr_uart_ioctl(
		struct avr_io_t * port,
		uint32_t ctl,
		void * io_param)
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

void
avr_uart_init(
		avr_t * avr,
		avr_uart_t * p)
{
	p->io = _io;

//	printf("%s UART%c UDR=%02x\n", __FUNCTION__, p->name, p->r_udr);

	p->flags = AVR_UART_FLAG_POLL_SLEEP|AVR_UART_FLAG_STDIO;

	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->rxc);
	avr_register_vector(avr, &p->txc);
	avr_register_vector(avr, &p->udrc);

	// allocate this module's IRQ
	avr_io_setirqs(&p->io, AVR_IOCTL_UART_GETIRQ(p->name), UART_IRQ_COUNT, NULL);
	// Only call callbacks when the value change...
	p->io.irq[UART_IRQ_OUT_XOFF].flags |= IRQ_FLAG_FILTERED;

	avr_register_io_write(avr, p->r_udr, avr_uart_udr_write, p);
	avr_register_io_read(avr, p->r_udr, avr_uart_read, p);
	// monitor code that reads the rxc flag, and delay it a bit
	avr_register_io_read(avr, p->rxc.raised.reg, avr_uart_rxc_read, p);

	if (p->udrc.vector)
		avr_register_io_write(avr, p->udrc.enable.reg, avr_uart_write, p);
	if (p->r_ucsra)
		avr_register_io_write(avr, p->r_ucsra, avr_uart_write, p);
	if (p->r_ubrrl)
		avr_register_io_write(avr, p->r_ubrrl, avr_uart_baud_write, p);
	avr_register_io_write(avr, p->rxen.reg, avr_uart_write, p);
}

