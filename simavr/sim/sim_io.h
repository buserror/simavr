/*
	sim_io.h

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

#ifndef __SIM_IO_H__
#define __SIM_IO_H__

#include "sim_avr.h"

/*
 * used by the ioports to implement their own features
 * see avr_eeprom.* for an example, and avr_ioctl().
 */
#define AVR_IOCTL_DEF(_a,_b,_c,_d) \
	(((_a) << 24)|((_b) << 16)|((_c) << 8)|((_d)))

/*
 * IO module base struct
 * Modules uses that as their first member in their own struct
 */
typedef struct avr_io_t {
	struct avr_io_t * next;
	const char * 		kind;		// pretty name, for debug

	uint32_t			irq_ioctl_get;	// used to get irqs from this module
	int					irq_count;	// number of (optional) irqs
	struct avr_irq_t *	irq;		// optional external IRQs
	// called at every instruction
	void (*run)(avr_t * avr, struct avr_io_t *io);
	// called at reset time
	void (*reset)(avr_t * avr, struct avr_io_t *io);
	// called externally. allow access to io modules and so on
	int (*ioctl)(avr_t * avr, struct avr_io_t *io, uint32_t ctl, void *io_param);
} avr_io_t;

/*
 * IO modules helper functions
 */

// registers an IO module, so it's run(), reset() etc are called
// this is called by the AVR core init functions, you /could/ register an external
// one after instanciation, for whatever purpose...
void avr_register_io(avr_t *avr, avr_io_t * io);
// register a callback for when IO register "addr" is read
void avr_register_io_read(avr_t *avr, uint8_t addr, avr_io_read_t read, void * param);
// register a callback for when the IO register is written. callback has to set the memory itself
void avr_register_io_write(avr_t *avr, uint8_t addr, avr_io_write_t write, void * param);
// call every IO modules until one responds to this
int avr_ioctl(avr_t *avr, uint32_t ctl, void * io_param);
// get the specific irq for a module, check AVR_IOCTL_IOPORT_GETIRQ for example
struct avr_irq_t * avr_io_getirq(avr_t * avr, uint32_t ctl, int index);

#endif /* __SIM_IO_H__ */
