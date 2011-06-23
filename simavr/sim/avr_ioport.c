/*
	avr_ioport.c

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
#include "avr_ioport.h"

static uint8_t avr_ioport_read(struct avr_t * avr, avr_io_addr_t addr, void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;
	uint8_t ddr = avr->data[p->r_ddr];
	uint8_t v = (avr->data[p->r_pin] & ~ddr) | (avr->data[p->r_port] & ddr);
	avr->data[addr] = v;
	// made to trigger potential watchpoints
	v = avr_core_watch_read(avr, addr);
//		printf("** PIN%c(%02x) = %02x\n", p->name, addr, v);

	return v;
}

static void avr_ioport_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;
	uint8_t oldv = avr->data[addr];

	avr_core_watch_write(avr, addr, v);
	if (v != oldv) {
		//	printf("PORT%c(%02x) = %02x (was %02x)\n", p->name, addr, v, oldv);
		int mask = v ^ oldv;

		// raise the internal IRQ callbacks
		for (int i = 0; i < 8; i++)
			if (mask & (1 << i))
				avr_raise_irq(p->io.irq + i, (v >> i) & 1);
		avr_raise_irq(p->io.irq + IOPORT_IRQ_PIN_ALL, v);
	}
}

/*
 * This is a reasonably new behaviour for the io-ports. Writing 1's to the PIN register
 * toggles the PORT equivalent bit (regardless of direction
 */
static void avr_ioport_pin_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;

	avr_ioport_write(avr, p->r_port, avr->data[p->r_port] ^ v, param);
}

/*
 * This is a the callback for the DDR register. There is nothing much to do here, apart
 * from triggering an IRQ in case any 'client' code is interested in the information.
 */
static void avr_ioport_ddr_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;

	avr_raise_irq(p->io.irq + IOPORT_IRQ_DIRECTION_ALL, v);
	avr_core_watch_write(avr, addr, v);
}

/*
 * this is our "main" pin change callback, it can be triggered by either the
 * AVR code, or any external piece of code that see fit to do it.
 * Either way, this will raise pin change interrupts, if needed
 */
void avr_ioport_irq_notify(struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;
	avr_t * avr = p->io.avr;

	int output = value & AVR_IOPORT_OUTPUT;
	value &= 0xff;
	uint8_t mask = 1 << irq->irq;
		// set the real PIN bit. ddr doesn't matter here as it's masked when read.
	avr->data[p->r_pin] &= ~mask;
	if (value)
		avr->data[p->r_pin] |= mask;

	if (output)	// if the IRQ was marked as Output, also do the IO write
		avr_ioport_write(avr, p->r_port, (avr->data[p->r_port] & ~mask) | (value ? mask : 0), p);

	if (p->r_pcint) {
		// if the pcint bit is on, try to raise it
		int raise = avr->data[p->r_pcint] & mask;
		if (raise)
			avr_raise_interrupt(avr, &p->pcint);
	}
}

static void avr_ioport_reset(avr_io_t * port)
{
	avr_ioport_t * p = (avr_ioport_t *)port;
	for (int i = 0; i < IOPORT_IRQ_PIN_ALL; i++) 
		avr_irq_register_notify(p->io.irq + i, avr_ioport_irq_notify, p);
}

static int avr_ioport_ioctl(struct avr_io_t * port, uint32_t ctl, void * io_param)
{
	avr_ioport_t * p = (avr_ioport_t *)port;
	avr_t * avr = p->io.avr;
	int res = -1;

	switch(ctl) {
		case AVR_IOCTL_IOPORT_GETIRQ_REGBIT: {
			avr_ioport_getirq_t * r = (avr_ioport_getirq_t*)io_param;

			if (r->bit.reg == p->r_port || r->bit.reg == p->r_pin || r->bit.reg == p->r_ddr) {
				// it's us ! check the special case when the "all pins" irq is requested
				int o = 0;
				if (r->bit.mask == 0xff)
					r->irq[o++] = &p->io.irq[IOPORT_IRQ_PIN_ALL];
				else {
					// otherwise fil up the ones needed
					for (int bi = 0; bi < 8; bi++)
						if (r->bit.mask & (1 << bi))
							r->irq[o++] = &p->io.irq[r->bit.bit + bi];
				}
				if (o < 8)
					r->irq[o] = NULL;
				return o;
			}
		}	break;
		default: {
			/*
			 * Return the port state if the IOCTL matches us.
			 */
			if (ctl == AVR_IOCTL_IOPORT_GETSTATE(p->name)) {
				avr_ioport_state_t state = {
					.name = p->name,
					.port = avr->data[p->r_port],
					.ddr = avr->data[p->r_ddr],
					.pin = avr->data[p->r_pin],
				};
				if (io_param)
					*((avr_ioport_state_t*)io_param) = state;
				res = 0;
			}
		}
	}

	return res;
}

static const char * irq_names[IOPORT_IRQ_COUNT] = {
	[IOPORT_IRQ_PIN0] = "=pin0",
	[IOPORT_IRQ_PIN1] = "=pin1",
	[IOPORT_IRQ_PIN2] = "=pin2",
	[IOPORT_IRQ_PIN3] = "=pin3",
	[IOPORT_IRQ_PIN4] = "=pin4",
	[IOPORT_IRQ_PIN5] = "=pin5",
	[IOPORT_IRQ_PIN6] = "=pin6",
	[IOPORT_IRQ_PIN7] = "=pin7",
	[IOPORT_IRQ_PIN_ALL] = "=all",
	[IOPORT_IRQ_DIRECTION_ALL] = ">ddr",
};

static	avr_io_t	_io = {
	.kind = "port",
	.reset = avr_ioport_reset,
	.ioctl = avr_ioport_ioctl,
	.irq_names = irq_names,
};

void avr_ioport_init(avr_t * avr, avr_ioport_t * p)
{
	p->io = _io;
//	printf("%s PIN%c 0x%02x DDR%c 0x%02x PORT%c 0x%02x\n", __FUNCTION__,
//		p->name, p->r_pin,
//		p->name, p->r_ddr,
//		p->name, p->r_port);
	
	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->pcint);
	// allocate this module's IRQ
	avr_io_setirqs(&p->io, AVR_IOCTL_IOPORT_GETIRQ(p->name), IOPORT_IRQ_COUNT, NULL);

	avr_register_io_write(avr, p->r_port, avr_ioport_write, p);
	avr_register_io_read(avr, p->r_pin, avr_ioport_read, p);
	avr_register_io_write(avr, p->r_pin, avr_ioport_pin_write, p);
	avr_register_io_write(avr, p->r_ddr, avr_ioport_ddr_write, p);
}

