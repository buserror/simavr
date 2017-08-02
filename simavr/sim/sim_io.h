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

#ifdef __cplusplus
extern "C" {
#endif

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
	struct avr_io_t * 	next;
	avr_t *				avr;		// avr we are attached to
	const char * 		kind;		// pretty name, for debug

	const char ** 		irq_names;	// IRQ names

	uint32_t			irq_ioctl_get;	// used to get irqs from this module
	int					irq_count;	// number of (optional) irqs
	struct avr_irq_t *	irq;		// optional external IRQs
	// called at reset time
	void (*reset)(struct avr_io_t *io);
	// called externally. allow access to io modules and so on
	int (*ioctl)(struct avr_io_t *io, uint32_t ctl, void *io_param);

	// optional, a function to free up allocated system resources
	void (*dealloc)(struct avr_io_t *io);
} avr_io_t;

/*
 * IO modules helper functions
 */

// registers an IO module, so it's run(), reset() etc are called
// this is called by the AVR core init functions, you /could/ register an external
// one after instantiation, for whatever purpose...
void
avr_register_io(
		avr_t *avr,
		avr_io_t * io);
// Sets an IO module "official" IRQs and the ioctl used to get to them. if 'irqs' is NULL,
// 'count' will be allocated
avr_irq_t *
avr_io_setirqs(
		avr_io_t * io,
		uint32_t ctl,
		int count,
		avr_irq_t * irqs );

// register a callback for when IO register "addr" is read
void
avr_register_io_read(
		avr_t *avr,
		avr_io_addr_t addr,
		avr_io_read_t read,
		void * param);
// register a callback for when the IO register is written. callback has to set the memory itself
void
avr_register_io_write(
		avr_t *avr,
		avr_io_addr_t addr,
		avr_io_write_t write,
		void * param);
// call every IO modules until one responds to this
int
avr_ioctl(
		avr_t *avr,
		uint32_t ctl,
		void * io_param);
// get the specific irq for a module, check AVR_IOCTL_IOPORT_GETIRQ for example
struct avr_irq_t *
avr_io_getirq(
		avr_t * avr,
		uint32_t ctl,
		int index);

// get the IRQ for an absolute IO address
// this allows any code to hook an IRQ in any io address, for example
// tracing changes of values into a register
// Note that the values do not "magically" change, they change only
// when the AVR code attempt to read and write at that address
//
// the "index" is a bit number, or ALL bits if index == 8
#define AVR_IOMEM_IRQ_ALL 8
avr_irq_t *
avr_iomem_getirq(
		avr_t * avr,
		avr_io_addr_t addr,
		const char * name /* Optional, if NULL, "ioXXXX" will be used */ ,
		int index);

// Terminates all IOs and remove from them from the io chain
void
avr_deallocate_ios(
		avr_t *avr);

#ifdef __cplusplus
};
#endif

#endif /* __SIM_IO_H__ */
