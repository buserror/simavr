/*
	uart_udp.c

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

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "uart_udp.h"
#include "avr_uart.h"
#include "sim_hex.h"

DEFINE_FIFO(uint8_t,uart_udp_fifo, 128);

/*
 * called when a byte is send via the uart on the AVR
 */
static void uart_udp_in_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	uart_udp_t * p = (uart_udp_t*)param;
}

/*
 * Called when the uart has room in it's input buffer. This is called repeateadly
 * if necessary, while the xoff is called only when the uart fifo is FULL
 */
static void uart_udp_xon_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	uart_udp_t * p = (uart_udp_t*)param;
	if (!p->xon)
		printf("uart_udp_xon_hook\n");
	p->xon = 1;
	// try to empty our fifo, the uart_udp_xoff_hook() will be called when
	// other side is full
	while (p->xon && !uart_udp_fifo_isempty(&p->out)) {
		avr_raise_irq(p->irq + IRQ_UART_UDP_BYTE_OUT, uart_udp_fifo_read(&p->out));
	}
}

/*
 * Called when the uart ran out of room in it's input buffer
 */
static void uart_udp_xoff_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	uart_udp_t * p = (uart_udp_t*)param;
	if (p->xon)
		printf("uart_udp_xoff_hook\n");
	p->xon = 0;
}

static void * uart_udp_thread(void * param)
{
	uart_udp_t * p = (uart_udp_t*)param;

	while (1) {
		fd_set read_set;
		int max;
		FD_ZERO(&read_set);

		FD_SET(p->s, &read_set);
		max = p->s + 1;

		struct timeval timo = { 0, 100 };	// short, but not too short interval
		int ret = select(max, &read_set, NULL, NULL, &timo);

		if (FD_ISSET(p->s, &read_set)) {
			uint8_t buffer[1024];

			socklen_t len = sizeof(p->peer);
			ssize_t r = recvfrom(p->s, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)&p->peer, &len);

			hdump("udp", buffer, len);

			// write them in fifo
			uint8_t * src = buffer;
			while (r-- && !uart_udp_fifo_isfull(&p->out))
				uart_udp_fifo_write(&p->out, *src++);
		}
	}
}


void uart_udp_init(struct avr_t * avr, uart_udp_t * p)
{
	p->avr = avr;
	p->irq = avr_alloc_irq(0, IRQ_UART_UDP_COUNT);
	avr_irq_register_notify(p->irq + IRQ_UART_UDP_BYTE_IN, uart_udp_in_hook, p);

	if ((p->s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		fprintf(stderr, "%s: Can't create socket: %s", __FUNCTION__, strerror(errno));
		return ;
	}

	struct sockaddr_in address = { 0 };
	address.sin_family = AF_INET;
	address.sin_port = htons (4321);

	if (bind(p->s, (struct sockaddr *) &address, sizeof(address))) {
		fprintf(stderr, "%s: Can not bind socket: %s", __FUNCTION__, strerror(errno));
		return ;
	}

	pthread_create(&p->thread, NULL, uart_udp_thread, p);

}

void uart_udp_connect(uart_udp_t * p, char uart)
{
	avr_irq_t * src = avr_io_getirq(p->avr, AVR_IOCTL_UART_GETIRQ(uart), UART_IRQ_OUTPUT);
	avr_irq_t * dst = avr_io_getirq(p->avr, AVR_IOCTL_UART_GETIRQ(uart), UART_IRQ_INPUT);
	avr_irq_t * xon = avr_io_getirq(p->avr, AVR_IOCTL_UART_GETIRQ(uart), UART_IRQ_OUT_XON);
	avr_irq_t * xoff = avr_io_getirq(p->avr, AVR_IOCTL_UART_GETIRQ(uart), UART_IRQ_OUT_XOFF);
	if (src && dst) {
		avr_connect_irq(src, p->irq + IRQ_UART_UDP_BYTE_IN);
		avr_connect_irq(p->irq + IRQ_UART_UDP_BYTE_OUT, dst);
	}
	if (xon)
		avr_irq_register_notify(xon, uart_udp_xon_hook, p);
	if (xoff)
		avr_irq_register_notify(xoff, uart_udp_xoff_hook, p);
}

