/*
	uart_pty.c

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

#include <sys/select.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <pty.h>
#include <signal.h>

#include "uart_pty.h"
#include "avr_uart.h"
#include "sim_hex.h"

DEFINE_FIFO(uint8_t,uart_pty_fifo);

/*
 * called when a byte is send via the uart on the AVR
 */
static void uart_pty_in_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	uart_pty_t * p = (uart_pty_t*)param;
	//printf("uart_pty_in_hook %02x\n", value);
	uart_pty_fifo_write(&p->in, value);
}

// try to empty our fifo, the uart_pty_xoff_hook() will be called when
// other side is full
static void  uart_pty_flush_incoming(uart_pty_t * p)
{
	while (p->xon && !uart_pty_fifo_isempty(&p->out)) {
		uint8_t byte = uart_pty_fifo_read(&p->out);
	//	printf("uart_pty_flush_incoming send %02x\n", byte);
		avr_raise_irq(p->irq + IRQ_UART_PTY_BYTE_OUT, byte);
	}
}

/*
 * Called when the uart has room in it's input buffer. This is called repeateadly
 * if necessary, while the xoff is called only when the uart fifo is FULL
 */
static void uart_pty_xon_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	uart_pty_t * p = (uart_pty_t*)param;
	if (!p->xon)
		printf("uart_pty_xon_hook\n");
	p->xon = 1;
	uart_pty_flush_incoming(p);
}

/*
 * Called when the uart ran out of room in it's input buffer
 */
static void uart_pty_xoff_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	uart_pty_t * p = (uart_pty_t*)param;
	if (p->xon)
		printf("uart_pty_xoff_hook\n");
	p->xon = 0;
}

static void * uart_pty_thread(void * param)
{
	uart_pty_t * p = (uart_pty_t*)param;

	while (1) {
		fd_set read_set, write_set;
		int max = p->s + 1;
		FD_ZERO(&read_set);
		FD_ZERO(&write_set);

		// read more only if buffer was flushed
		if (p->buffer_len == p->buffer_done)
			FD_SET(p->s, &read_set);
		if (!uart_pty_fifo_isempty(&p->in))
			FD_SET(p->s, &write_set);

		struct timeval timo = { 0, 500 };	// short, but not too short interval
		int ret = select(max, &read_set, &write_set, NULL, &timo);

		if (!ret)
			continue;
		if (ret < 0)
			break;

		if (FD_ISSET(p->s, &read_set)) {
			ssize_t r = read(p->s, p->buffer, sizeof(p->buffer)-1);
			p->buffer_len = r;
			p->buffer_done = 0;
		//	hdump("pty recv", p->buffer, r);
		}
		if (p->buffer_done < p->buffer_len) {
			// write them in fifo
			while (p->buffer_done < p->buffer_len && !uart_pty_fifo_isfull(&p->out))
				uart_pty_fifo_write(&p->out, p->buffer[p->buffer_done++]);
		}
		if (FD_ISSET(p->s, &write_set)) {
			uint8_t buffer[512];
			// write them in fifo
			uint8_t * dst = buffer;
			while (!uart_pty_fifo_isempty(&p->in) && dst < (buffer+sizeof(buffer)))
				*dst++ = uart_pty_fifo_read(&p->in);
			size_t len = dst - buffer;
			size_t r = write(p->s, buffer, len);
		//	hdump("pty send", buffer, r);
		}
	//	uart_pty_flush_incoming(p);
	}
	return NULL;
}

static const char * irq_names[IRQ_UART_PTY_COUNT] = {
	[IRQ_UART_PTY_BYTE_IN] = "8<uart_pty.in",
	[IRQ_UART_PTY_BYTE_OUT] = "8>uart_pty.out",
};

void uart_pty_init(struct avr_t * avr, uart_pty_t * p)
{
	p->avr = avr;
	p->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_UART_PTY_COUNT, irq_names);
	avr_irq_register_notify(p->irq + IRQ_UART_PTY_BYTE_IN, uart_pty_in_hook, p);

	int m, s;

	if (openpty(&m, &s, p->slavename, NULL, NULL) < 0) {
		fprintf(stderr, "%s: Can't create pty: %s", __FUNCTION__, strerror(errno));
		return ;
	}
	p->s = m;

	printf("uart_pty_init bridge on port *** %s ***\n", p->slavename);

	pthread_create(&p->thread, NULL, uart_pty_thread, p);

}

void uart_pty_stop(uart_pty_t * p)
{
	puts(__func__);
	pthread_kill(p->thread, SIGINT);
	close(p->s);
	void * ret;
	pthread_join(p->thread, &ret);
}

void uart_pty_connect(uart_pty_t * p, char uart)
{
	// disable the stdio dump, as we are sending binary there
	uint32_t f = 0;
	avr_ioctl(p->avr, AVR_IOCTL_UART_GET_FLAGS(uart), &f);
	f &= ~AVR_UART_FLAG_STDIO;
	avr_ioctl(p->avr, AVR_IOCTL_UART_SET_FLAGS(uart), &f);

	avr_irq_t * src = avr_io_getirq(p->avr, AVR_IOCTL_UART_GETIRQ(uart), UART_IRQ_OUTPUT);
	avr_irq_t * dst = avr_io_getirq(p->avr, AVR_IOCTL_UART_GETIRQ(uart), UART_IRQ_INPUT);
	avr_irq_t * xon = avr_io_getirq(p->avr, AVR_IOCTL_UART_GETIRQ(uart), UART_IRQ_OUT_XON);
	avr_irq_t * xoff = avr_io_getirq(p->avr, AVR_IOCTL_UART_GETIRQ(uart), UART_IRQ_OUT_XOFF);
	if (src && dst) {
		avr_connect_irq(src, p->irq + IRQ_UART_PTY_BYTE_IN);
		avr_connect_irq(p->irq + IRQ_UART_PTY_BYTE_OUT, dst);
	}
	if (xon)
		avr_irq_register_notify(xon, uart_pty_xon_hook, p);
	if (xoff)
		avr_irq_register_notify(xoff, uart_pty_xoff_hook, p);

	char link[128];
	sprintf(link, "/tmp/simavr-uart%c", uart);
	unlink(link);
	if (symlink(p->slavename, link) != 0) {
		fprintf(stderr, "WARN %s: Can't create %s: %s", __func__, link, strerror(errno));
	} else {
		printf("%s: %s now points to %s\n", __func__, link, p->slavename);
	}
	if (getenv("SIMAVR_UART_XTERM")) {
		char cmd[256];
		sprintf(cmd, "nohup xterm -e picocom -b 115200 %s >/dev/null 2>&1 &", p->slavename);
		system(cmd);
	}
}

