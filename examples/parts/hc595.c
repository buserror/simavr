/*
	hc595.c

	This defines a sample for a very simple "peripheral" 
	that can talk to an AVR core.
	It is in fact a bit more involved than strictly necessary,
	but is made to demonstrante a few useful features that are
	easy to use.
	
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

#include <stdlib.h>
#include <stdio.h>
#include "sim_avr.h"
#include "hc595.h"

/*
 * called when a SPI byte is sent
 */
static void hc595_spi_in_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	hc595_t * p = (hc595_t*)param;
	// send "old value" to any chained one..
	avr_raise_irq(p->irq + IRQ_HC595_SPI_BYTE_OUT, p->value);	
	p->value = (p->value << 8) | (value & 0xff);
}

/*
 * called when a LATCH signal is sent
 */
static void hc595_latch_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	hc595_t * p = (hc595_t*)param;
	if (irq->value && !value) {	// falling edge
		p->latch = p->value;
		avr_raise_irq(p->irq + IRQ_HC595_OUT, p->latch);
	}
}

/*
 * called when a RESET signal is sent
 */
static void hc595_reset_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	hc595_t * p = (hc595_t*)param;
	if (irq->value && !value) 	// falling edge
		p->latch = p->value = 0;
}

static const char * irq_names[IRQ_HC595_COUNT] = {
		[IRQ_HC595_SPI_BYTE_IN] = "8<hc595.in",
		[IRQ_HC595_SPI_BYTE_OUT] = "8>hc595.chain",
		[IRQ_HC595_IN_LATCH] = "<hc595.latch",
		[IRQ_HC595_IN_RESET] = "<hc595.reset",
		[IRQ_HC595_OUT] = "8>hc595.out",
};

void
hc595_init(
		struct avr_t * avr,
		hc595_t *p)
{
	p->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_HC595_COUNT, irq_names);
	avr_irq_register_notify(p->irq + IRQ_HC595_SPI_BYTE_IN, hc595_spi_in_hook, p);
	avr_irq_register_notify(p->irq + IRQ_HC595_IN_LATCH, hc595_latch_hook, p);
	avr_irq_register_notify(p->irq + IRQ_HC595_IN_RESET, hc595_reset_hook, p);
	
	p->latch = p->value = 0;
}

