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

#define D(_w)

static uint8_t
avr_ioport_read(
		struct avr_t * avr,
		avr_io_addr_t addr,
		void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;
	uint8_t ddr = avr->data[p->r_ddr];
	uint8_t v = (avr->data[p->r_pin] & ~ddr) | (avr->data[p->r_port] & ddr);
	avr->data[addr] = v;
	// made to trigger potential watchpoints
	v = avr_core_watch_read(avr, addr);
	avr_raise_irq(p->io.irq + IOPORT_IRQ_REG_PIN, v);
	D(if (avr->data[addr] != v) printf("** PIN%c(%02x) = %02x\r\n", p->name, addr, v);)

	return v;
}

static void
avr_ioport_update_irqs_pin(
		avr_ioport_t * p,
		int32_t selected_pin
		)
{
	avr_t * avr = p->io.avr;
	uint8_t ddr = avr->data[p->r_ddr];
	uint8_t zstate_pin_mask = 0;
	int ind_from = 0;
	int ind_to = 8;
	if ((selected_pin >= 0) && (selected_pin < 8)) {
		ind_from = selected_pin;
		ind_to = selected_pin + 1;
	}
	// Set the PORT value if the pin is marked as output
	// otherwise, if there is an 'external' pullup, set it
	// otherwise, if the PORT pin was 1 to indicate an
	// internal pullup, set that.
	for (int i = ind_from; i < ind_to; i++) {
		if (ddr & (1 << i))
			avr_raise_irq(p->io.irq + i, (avr->data[p->r_port] >> i) & 1);
		else if ((p->io.irq + IOPORT_IRQ_PIN0_SRC_IMP + i)->value >= AVR_IOPORT_INTRN_PULLUP_IMP) {
			zstate_pin_mask |= (1 << i);
			if (p->external.pull_mask & (1 << i))
				avr_raise_irq(p->io.irq + i, ((p->external.pull_value >> i) & 1));
			else if ((avr->data[p->r_port] >> i) & 1)
				avr_raise_irq(p->io.irq + i, 1);
		}
	}
	uint8_t pin = (avr->data[p->r_pin] & ~ddr) | (avr->data[p->r_port] & ddr);
	pin = (pin & ~(p->external.pull_mask & zstate_pin_mask)) | (p->external.pull_value & zstate_pin_mask);
	avr_raise_irq(p->io.irq + IOPORT_IRQ_PIN_ALL, pin);

}

static void
avr_ioport_update_irqs(
		avr_ioport_t * p)
{
	avr_t * avr = p->io.avr;

	avr_ioport_update_irqs_pin(p, -1);
	// if IRQs are registered on the PORT register (for example, VCD dumps) send
	// those as well
	avr_io_addr_t port_io = AVR_DATA_TO_IO(p->r_port);
	if (avr->io[port_io].irq) {
		avr_raise_irq(avr->io[port_io].irq + AVR_IOMEM_IRQ_ALL, avr->data[p->r_port]);
		for (int i = 0; i < 8; i++)
			avr_raise_irq(avr->io[port_io].irq + i, (avr->data[p->r_port] >> i) & 1);
 	}
}

static void
avr_ioport_write(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;

	D(if (avr->data[addr] != v) printf("** PORT%c(%02x) = %02x\r\n", p->name, addr, v);)
	avr_core_watch_write(avr, addr, v);
	avr_raise_irq(p->io.irq + IOPORT_IRQ_REG_PORT, v);
	avr_ioport_update_irqs(p);
}

/*
 * This is a reasonably new behaviour for the io-ports. Writing 1's to the PIN register
 * toggles the PORT equivalent bit (regardless of direction
 */
static void
avr_ioport_pin_write(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;

	avr_ioport_write(avr, p->r_port, avr->data[p->r_port] ^ v, param);
}

/*
 * This is a the callback for the DDR register. There is nothing much to do here, apart
 * from triggering an IRQ in case any 'client' code is interested in the information,
 * and restoring all PIN bits marked as output to PORT values.
 */
static void
avr_ioport_ddr_write(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;

	D(if (avr->data[addr] != v) printf("** DDR%c(%02x) = %02x\r\n", p->name, addr, v);)
	for (int i = 0; i < 8; i++) {
		uint8_t mask = (1 << i);
		if ((v & mask) != (avr->data[addr] & mask)) {
			if ((v & mask) == 0)
				(p->io.irq + i)->flags |= IRQ_FLAG_INIT; // invalidate the pin prev.value if it's switched to input
		}
	}
	avr_raise_irq(p->io.irq + IOPORT_IRQ_DIRECTION_ALL, v);
	avr_core_watch_write(avr, addr, v);

	avr_ioport_update_irqs(p);
}

static void
_avr_ioport_irq_notify(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;
	avr_t * avr = p->io.avr;

	value &= 0xff;
	uint8_t mask = 1 << irq->irq;
		// set the real PIN bit. ddr doesn't matter here as it's masked when read.
	avr->data[p->r_pin] &= ~mask;
	if (value)
		avr->data[p->r_pin] |= mask;
}

/*
 * this is our "main" pin change callback, it can be triggered by either the
 * AVR code, or any external piece of code that see fit to do it.
 * Either way, this will raise pin change interrupts, if needed
 */
void
avr_ioport_irq_notify(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;
	avr_t * avr = p->io.avr;

	int output = value & AVR_IOPORT_OUTPUT;
	value &= 0xff;
	uint8_t mask = 1 << irq->irq;
	uint32_t prev_val = avr->data[p->r_pin] & mask;
		// set the real PIN bit. ddr doesn't matter here as it's masked when read.
	avr->data[p->r_pin] &= ~mask;
	if (value)
		avr->data[p->r_pin] |= mask;
	uint8_t ddr = avr->data[p->r_ddr];
	avr_irq_t * impedance_irq = p->io.irq + IOPORT_IRQ_PIN0_SRC_IMP + irq->irq;
	if (impedance_irq &&
		((impedance_irq->value >= AVR_IOPORT_INTRN_PULLUP_IMP) || (impedance_irq->flags & IRQ_FLAG_USER)) &&
		!((ddr & (1 << irq->irq)))) {
		// do a kind of recursion here to handle pull-ups
		avr_irq_register_notify(
			irq,
			_avr_ioport_irq_notify,
			param);
		avr_ioport_update_irqs_pin(p, irq->irq);
		avr_irq_unregister_notify(
			irq,
			_avr_ioport_irq_notify,
			param);
	}


	if (output)	// if the IRQ was marked as Output, also do the IO write
		avr_ioport_write(avr, p->r_port, (avr->data[p->r_port] & ~mask) | (value ? mask : 0), p);

	uint32_t new_val = avr->data[p->r_pin] & mask;
	if (p->r_pcint && (new_val != prev_val)) {
		// if the pcint bit is on, try to raise it
		int raise = avr->data[p->r_pcint] & mask;
		if (raise)
			avr_raise_interrupt(avr, &p->pcint);
	}
}

void
avr_ioport_irq_src_impedance(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	avr_ioport_t * p = (avr_ioport_t *)param;
	avr_irq_t * pin_irq = p->io.irq + irq->irq - IOPORT_IRQ_PIN0_SRC_IMP;
	if (!pin_irq)
		return;
	pin_irq->flags |= IRQ_FLAG_INIT;
	irq->value = value;
	irq->flags |= IRQ_FLAG_USER;
	avr_raise_irq(pin_irq, pin_irq->value);
	irq->flags &= ~IRQ_FLAG_USER;
}

static void
avr_ioport_reset(
		avr_io_t * port)
{
	avr_ioport_t * p = (avr_ioport_t *)port;
	for (int i = 0; i < IOPORT_IRQ_PIN_ALL; i++)
		avr_irq_register_notify(p->io.irq + i, avr_ioport_irq_notify, p);
	for (int i = 0; i < IOPORT_IRQ_COUNT; i++)
		p->io.irq[i].flags |= IRQ_FLAG_INIT;
	for (int i = IOPORT_IRQ_PIN0_SRC_IMP; i < (IOPORT_IRQ_PIN0_SRC_IMP + IOPORT_IRQ_PIN_ALL); i++) {
		avr_irq_register_notify(p->io.irq + i, avr_ioport_irq_src_impedance, p);
		avr_raise_irq(p->io.irq + i, (p->io.irq + i)->value);
	}
}

static int
avr_ioport_ioctl(
		struct avr_io_t * port,
		uint32_t ctl,
		void * io_param)
{
	avr_ioport_t * p = (avr_ioport_t *)port;
	avr_t * avr = p->io.avr;
	int res = -1;

	// all IOCTls require some sort of valid parameter, bail if not
	if (!io_param)
		return -1;

	switch(ctl) {
		case AVR_IOCTL_IOPORT_GETIRQ_REGBIT: {
			avr_ioport_getirq_t * r = (avr_ioport_getirq_t*)io_param;

			if (r->bit.reg == p->r_port || r->bit.reg == p->r_pin || r->bit.reg == p->r_ddr) {
				// it's us ! check the special case when the "all pins" irq is requested
				int o = 0;
				if (r->bit.mask == 0xff)
					r->irq[o++] = &p->io.irq[IOPORT_IRQ_PIN_ALL];
				else {
					// otherwise fill up the ones needed
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
			/*
			 * Set the default IRQ values when pin is set as input
			 */
			if (ctl == AVR_IOCTL_IOPORT_SET_EXTERNAL(p->name)) {
				avr_ioport_external_t * m = (avr_ioport_external_t*)io_param;
				p->external.pull_mask = m->mask;
				p->external.pull_value = m->value;
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
	[IOPORT_IRQ_PIN_ALL] = "8=all",
	[IOPORT_IRQ_DIRECTION_ALL] = "8>ddr",
	[IOPORT_IRQ_REG_PORT] = "8>port",
	[IOPORT_IRQ_REG_PIN] = "8>pin",
	[IOPORT_IRQ_PIN0_SRC_IMP] = ">srcimp0",
	[IOPORT_IRQ_PIN1_SRC_IMP] = ">srcimp1",
	[IOPORT_IRQ_PIN2_SRC_IMP] = ">srcimp2",
	[IOPORT_IRQ_PIN3_SRC_IMP] = ">srcimp3",
	[IOPORT_IRQ_PIN4_SRC_IMP] = ">srcimp4",
	[IOPORT_IRQ_PIN5_SRC_IMP] = ">srcimp5",
	[IOPORT_IRQ_PIN6_SRC_IMP] = ">srcimp6",
	[IOPORT_IRQ_PIN7_SRC_IMP] = ">srcimp7",
};

static	avr_io_t	_io = {
	.kind = "port",
	.reset = avr_ioport_reset,
	.ioctl = avr_ioport_ioctl,
	.irq_names = irq_names,
};

void avr_ioport_init(avr_t * avr, avr_ioport_t * p)
{
	if (!p->r_port) {
		printf("skipping PORT%c for core %s\n", p->name, avr->mmcu);
		return;
	}
	p->io = _io;
//	printf("%s PIN%c 0x%02x DDR%c 0x%02x PORT%c 0x%02x\n", __FUNCTION__,
//		p->name, p->r_pin,
//		p->name, p->r_ddr,
//		p->name, p->r_port);

	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->pcint);
	// allocate this module's IRQ
	avr_io_setirqs(&p->io, AVR_IOCTL_IOPORT_GETIRQ(p->name), IOPORT_IRQ_COUNT, NULL);

	for (int i = 0; i < IOPORT_IRQ_COUNT; i++)
		p->io.irq[i].flags |= IRQ_FLAG_FILTERED;
	for (int i = IOPORT_IRQ_PIN0_SRC_IMP; i < (IOPORT_IRQ_PIN0_SRC_IMP + IOPORT_IRQ_PIN_ALL); i++)
		(p->io.irq + i)->value = (uint32_t)-1;

	avr_register_io_write(avr, p->r_port, avr_ioport_write, p);
	avr_register_io_read(avr, p->r_pin, avr_ioport_read, p);
	avr_register_io_write(avr, p->r_pin, avr_ioport_pin_write, p);
	avr_register_io_write(avr, p->r_ddr, avr_ioport_ddr_write, p);
}

