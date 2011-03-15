/*
	avr_uart.h

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

#ifndef AVR_UART_H_
#define AVR_UART_H_

#include "sim_avr.h"

#include "fifo_declare.h"

DECLARE_FIFO(uint8_t, uart_fifo, 64);

/*
 * The method of "connecting" the the UART from external code is to use 4 IRQS.
 * The easy one is UART->YOU, where you will be called with the byte everytime
 * the AVR firmware sends one. Do whatever you like with it.
 *
 * The slightly more tricky one is the INPUT part. Since the AVR is quite a bit
 * slower than your code most likely, there is a way for the AVR UART to tell
 * you to "pause" sending it bytes when its own input buffer is full.
 * So, the UART will send XON to you when its fifo is empty, XON means you can
 * send as many bytes as you have until XOFF is sent. Note that these are two
 * IRQs because you /will/ be called with XOFF when sending a byte in INPUT...
 * So it's a reentrant process.
 *
 * When XOFF has been called, do not send any new bytes, they would be dropped.
 * Instead wait for XON again and continue.
 * See examples/parts/uart_udp.c for a full implementation
 *
 * Pseudo code:
 *
 * volatile int off = 0;
 * void irq_xon()
 * {
 *     off = 0;
 *     while (!off && bytes_left)
 *     avr_raise_irq(UART_IRQ_INPUT, a_byte);
 * }
 * void irq_xoff()
 * {
 *     off = 1;
 * }
 *
 */
enum {
	UART_IRQ_INPUT = 0,
	UART_IRQ_OUTPUT,
	UART_IRQ_OUT_XON,		// signaled (continuously) when input fifo is not full
	UART_IRQ_OUT_XOFF,		// signaled when input fifo IS full
	UART_IRQ_COUNT
};

// add port number to get the real IRQ
#define AVR_IOCTL_UART_GETIRQ(_name) AVR_IOCTL_DEF('u','a','r',(_name))

enum {
	// the uart code monitors for firmware that pool on
	// reception registers, and can do an atomic usleep()
	// if it's detected, this helps regulating CPU
	AVR_UART_FLAG_POOL_SLEEP = (1 << 0),
	AVR_UART_FLAG_STDIO = (1 << 1),			// print lines on the console
};

typedef struct avr_uart_t {
	avr_io_t	io;
	char name;
	avr_regbit_t	disabled;	// bit in the PRR
	
	avr_io_addr_t r_udr;
	avr_io_addr_t r_ucsra;
	avr_io_addr_t r_ucsrb;
	avr_io_addr_t r_ucsrc;

	avr_regbit_t	rxen;		// receive enabled
	avr_regbit_t	txen;		// transmit enable
	avr_regbit_t	u2x;		// double UART speed
	avr_regbit_t	usbs;		// stop bits
	avr_regbit_t	ucsz;		// data bits
	avr_regbit_t	ucsz2;		// data bits, continued

	avr_io_addr_t r_ubrrl,r_ubrrh;

	avr_int_vector_t rxc;
	avr_int_vector_t txc;
	avr_int_vector_t udrc;	

	uart_fifo_t	input;

	uint32_t		flags;
	avr_cycle_count_t usec_per_byte;
} avr_uart_t;

/* takes a uint32_t* as parameter */
#define AVR_IOCTL_UART_SET_FLAGS(_name)	AVR_IOCTL_DEF('u','a','s',(_name))
#define AVR_IOCTL_UART_GET_FLAGS(_name)	AVR_IOCTL_DEF('u','a','g',(_name))

void avr_uart_init(avr_t * avr, avr_uart_t * port);

#endif /* AVR_UART_H_ */
