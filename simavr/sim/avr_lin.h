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

#ifndef __AVR_LIN_H__
#define __AVR_LIN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_avr.h"
#include "avr_uart.h"

typedef struct avr_lin_t {
	avr_io_t io;

	avr_io_addr_t r_linbtr;
	avr_io_addr_t r_linbrrh, r_linbrrl;

	avr_regbit_t lena;
	avr_regbit_t ldisr;
	avr_regbit_t lbt;

	avr_uart_t uart;  // used when LIN controller is setup as a UART
} avr_lin_t;

void
avr_lin_init(
		avr_t *avr,
		avr_lin_t *port);

#ifdef __cplusplus
};
#endif

#endif /* __AVR_LIN_H__ */
