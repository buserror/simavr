/*
	avr_twi.c

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

#include <stdio.h>
#include "avr_twi.h"

static uint8_t avr_twi_read(struct avr_t * avr, avr_io_addr_t addr, void * param)
{
	avr_twi_t * p = (avr_twi_t *)param;
//	uint8_t v = p->input_data_register;
//	p->input_data_register = 0;
//	printf("avr_twi_read = %02x\n", v);
	return 0;
}

static void avr_twi_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_twi_t * p = (avr_twi_t *)param;
#if 0
	if (addr == p->r_spdr) {
//		printf("avr_twi_write = %02x\n", v);
		avr_core_watch_write(avr, addr, v);

		if (avr_regbit_get(avr, p->spe)) {
			// in master mode, any byte is sent as it comes..
			if (avr_regbit_get(avr, p->mstr)) {
				avr_raise_irq(p->io.irq + TWI_IRQ_OUTPUT, v);
			}
		}
	}
#endif
}

static void avr_twi_irq_input(struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_twi_t * p = (avr_twi_t *)param;
	avr_t * avr = p->io.avr;

	// check to see if we are enabled
	if (!avr_regbit_get(avr, p->twen))
		return;
#if 0
	// double buffer the input.. ?
	p->input_data_register = value;
	avr_raise_interrupt(avr, &p->twi);

	// if in slave mode, 
	// 'output' the byte only when we received one...
	if (!avr_regbit_get(avr, p->mstr)) {
		avr_raise_irq(p->io.irq + TWI_IRQ_OUTPUT, avr->data[p->r_spdr]);
	}
#endif
}

static int twi_slave_has_address(struct twi_slave_t* p, uint8_t address)
{
	return 0;
}

	// handle start conditionto address+w, restart means "stop" wasn't called
static int twi_slave_start(struct twi_slave_t* p, uint8_t address, int restart)
{
	return 0;
}

	// handle a data write, after a (re)start
static int twi_slave_write(struct twi_slave_t* p, uint8_t v)
{
	return 0;
}

	// handle a data read, after a (re)start
static uint8_t twi_slave_read(struct twi_slave_t* p)
{
	return 0;
}

	// stop condition detected
static void twi_slave_stop(struct twi_slave_t* p)
{
}

static twi_slave_t slave_driver = {
	.has_address = twi_slave_has_address,
	.start = twi_slave_start,
	.stop = twi_slave_stop,
	.write = twi_slave_write,
	.read = twi_slave_read
};

void avr_twi_reset(struct avr_io_t *io)
{
	avr_twi_t * p = (avr_twi_t *)io;
	//avr_irq_register_notify(p->io.irq + TWI_IRQ_INPUT, avr_twi_irq_input, p);
}

static	avr_io_t	_io = {
	.kind = "twi",
	.reset = avr_twi_reset,
};

void avr_twi_init(avr_t * avr, avr_twi_t * p)
{
	p->io = _io;
	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->twi);
	p->slave = slave_driver;	// get default callbacks
	twi_slave_init(&p->slave, p);
	twi_bus_init(&p->bus);

	printf("%s TWI%c init\n", __FUNCTION__, p->name);

	// allocate this module's IRQ
	p->io.irq_count = TWI_IRQ_COUNT;
	p->io.irq = avr_alloc_irq(0, p->io.irq_count);
	p->io.irq_ioctl_get = AVR_IOCTL_TWI_GETIRQ(p->name);

	avr_register_io_write(avr, p->r_twdr, avr_twi_write, p);
	avr_register_io_read(avr, p->r_twdr, avr_twi_read, p);
}

