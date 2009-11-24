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

#include "simavr.h"

typedef struct avr_uart_t {
	avr_io_t	io;
	char name;
	avr_regbit_t	disabled;	// bit in the PRR
	
	uint8_t r_udr;
	uint8_t r_ucsra;
	uint8_t r_ucsrb;
	uint8_t r_ucsrc;

	uint8_t r_ubrrl,r_ubrrh;

	avr_int_vector_t rxc;
	avr_int_vector_t txc;
	avr_int_vector_t udrc;

	avr_regbit_t	udre; // AVR_IO_REGBIT(UCSR0A, UDRE0),
	
} avr_uart_t;

void avr_uart_init(avr_t * avr, avr_uart_t * port);

#endif /* AVR_UART_H_ */
