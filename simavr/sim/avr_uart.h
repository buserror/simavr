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

DECLARE_FIFO(uint8_t, uart_fifo, 128);

enum {
	UART_IRQ_INPUT = 0,
	UART_IRQ_OUTPUT,
	UART_IRQ_COUNT
};

// add port number to get the real IRQ
#define AVR_IOCTL_UART_GETIRQ(_name) AVR_IOCTL_DEF('u','a','r',(_name))

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

	avr_io_addr_t r_ubrrl,r_ubrrh;

	avr_int_vector_t rxc;
	avr_int_vector_t txc;
	avr_int_vector_t udrc;	

	uart_fifo_t	input;
} avr_uart_t;

void avr_uart_init(avr_t * avr, avr_uart_t * port);

#endif /* AVR_UART_H_ */
