/*
	sim_irq.h

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

#ifndef __SIM_IRQ_H__
#define __SIM_IRQ_H__

#include <stdint.h>

/*
 * Internal IRQ system
 * 
 * This subsystem allow any piece of code to "register" a hook to be called when an IRQ is
 * raised. The IRQ definition is up to the module defining it, for example a IOPORT pin change
 * might be an IRQ in wich case any oiece of code can be notified when a pin has changed state
 * 
 * The notify hooks are chained, and duplicates are filtered out so you can't register a
 * notify hook twice on one particylar IRQ
 * 
 * IRQ calling order is not defined, so don't rely on it.
 * 
 * IRQ hook needs to be registered in reset() handlers, ie after all modules init() bits
 * have been called, to prevent race condition of the initialization order.
 */
struct avr_irq_t;

typedef void (*avr_irq_notify_t)(struct avr_irq_t * irq, uint32_t value, void * param);


enum {
	IRQ_FLAG_NOT		= (1 << 0),	// change polarity of the IRQ
	IRQ_FLAG_FILTERED	= (1 << 1),	// do not "notify" if "value" is the same as previous raise
	IRQ_FLAG_ALLOC		= (1 << 2), // this irq structure was malloced via avr_alloc_irq
};

/*
 * Public IRQ structure
 */
typedef struct avr_irq_t {
	uint32_t			irq;		// any value the user needs
	uint32_t			value;		// current value
	uint8_t				flags;		// IRQ_* flags
	struct avr_irq_hook_t * hook;	// list of hooks to be notified
} avr_irq_t;

// allocates 'count' IRQs, initializes their "irq" starting from 'base' and increment
avr_irq_t * avr_alloc_irq(uint32_t base, uint32_t count);
void avr_free_irq(avr_irq_t * irq, uint32_t count);

// init 'count' IRQs, initializes their "irq" starting from 'base' and increment
void avr_init_irq(avr_irq_t * irq, uint32_t base, uint32_t count);
// 'raise' an IRQ. Ie call their 'hooks', and raise any chained IRQs, and set the new 'value'
void avr_raise_irq(avr_irq_t * irq, uint32_t value);
// this connects a "source" IRQ to a "destination" IRQ
void avr_connect_irq(avr_irq_t * src, avr_irq_t * dst);
// register a notification 'hook' for 'irq' -- 'param' is anything that your want passed back as argument
void avr_irq_register_notify(avr_irq_t * irq, avr_irq_notify_t notify, void * param);

#endif /* __SIM_IRQ_H__ */
