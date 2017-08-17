/*
	avr_extint.c

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
#include <stdlib.h>
#include <string.h>
#include "avr_extint.h"
#include "avr_ioport.h"

static int
avr_extint_read_pin(
		avr_extint_t * p,
		int eint_no) // index of interrupt source
{
	avr_t * avr = p->io.avr;
	char port = p->eint[eint_no].port_ioctl & 0xFF;
	avr_ioport_state_t iostate;
	if (avr_ioctl(avr, AVR_IOCTL_IOPORT_GETSTATE( port ), &iostate) < 0)
		return -1;
	uint8_t bit = ( iostate.pin >> p->eint[eint_no].port_pin ) & 1;
	return (int)bit;
}

static uint8_t
avr_extint_get_mode(
		avr_extint_t * p,
		int eint_no) // index of interrupt source
{
	avr_t * avr = p->io.avr;
	uint8_t isc_bits = p->eint[eint_no].isc[1].reg ? 2 : 1;
	uint8_t mode = avr_regbit_get_array(avr, p->eint[eint_no].isc, isc_bits);

	// Asynchronous interrupts, eg int2 in m16, m32 etc. support only down/up
	if (isc_bits == 1)
		mode +=2;
	return mode;
}

static avr_cycle_count_t
avr_extint_poll_level_trig(
		struct avr_t * avr,
		avr_cycle_count_t when,
		void * param)
{
	avr_extint_poll_context_t *poll = (avr_extint_poll_context_t *)param;
	avr_extint_t * p = (avr_extint_t *)poll->extint;
	int eint_no = poll->eint_no;
	uint8_t mode = avr_extint_get_mode(p, eint_no);

	if (mode)
		goto terminate_poll; // only poll while in level triggering mode
	if (!avr_regbit_get(avr, p->eint[eint_no].vector.enable))
		goto terminate_poll; // only poll while External Interrupt Request X Enable flag is set
	int bit = avr_extint_read_pin(p, eint_no);
	if (bit)
		goto terminate_poll; // only poll while pin level remains low

	if (avr->sreg[S_I]) {
		uint8_t raised = avr_regbit_get(avr, p->eint[eint_no].vector.raised) || p->eint[eint_no].vector.pending;
		if (!raised)
			avr_raise_interrupt(avr, &p->eint[eint_no].vector);
	}
	return when+1;

terminate_poll:
	poll->is_polling = 0;
	return 0;
}

static void
avr_extint_fire_and_start_poll(
		avr_extint_t * p,
		int eint_no) // index of interrupt source
{
	avr_t * avr = p->io.avr;
	if (avr_regbit_get(avr, p->eint[eint_no].vector.enable)) {
		if (avr->sreg[S_I]) {
			uint8_t raised = avr_regbit_get(avr, p->eint[eint_no].vector.raised) || p->eint[eint_no].vector.pending;
			if (!raised)
				avr_raise_interrupt(avr, &p->eint[eint_no].vector);
		}
		if (p->eint[eint_no].strict_lvl_trig) {
			avr_extint_poll_context_t *poll = p->poll_contexts + eint_no;
			if (poll &&
				!poll->is_polling) {
				avr_cycle_timer_register(avr, 1, avr_extint_poll_level_trig, poll);
				poll->is_polling = 1;
			}
		}
	}
}

static avr_extint_t *
avr_extint_get(
		avr_t * avr)
{
	if (!avr)
		return NULL;
	avr_io_t * periferal = avr->io_port;
	while (periferal) {
		if (!strcmp(periferal->kind, "extint")) {
			return (avr_extint_t *)periferal;
		}
		periferal = periferal->next;
	}
	return NULL;
}

static uint8_t
avr_extint_count(
		avr_extint_t *extint)
{
	uint8_t count = 0;
	if (!extint)
		return 0;
	while (count < EXTINT_COUNT) {
		if (!extint->eint[count].port_ioctl)
			break;
		count++;
	}
	return count;
}

int
avr_extint_is_strict_lvl_trig(
		avr_t * avr,
		uint8_t extint_no)
{
	avr_extint_t *p = avr_extint_get(avr);
	if (!p || (avr_extint_count(p) <= extint_no))
		return -1;
	if (!p->eint[extint_no].isc[1].reg)
		return -1; // this is edge-only triggered interrupt
	return p->eint[extint_no].strict_lvl_trig;
}

void
avr_extint_set_strict_lvl_trig(
		avr_t * avr,
		uint8_t extint_no,
		uint8_t strict)
{
	avr_extint_t *p = avr_extint_get(avr);
	if (!p || (avr_extint_count(p) <= extint_no))
		return;
	if (!p->eint[extint_no].isc[1].reg)
		return; // this is edge-only triggered interrupt
	p->eint[extint_no].strict_lvl_trig = strict;
}

static void
avr_extint_irq_notify(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	avr_extint_t * p = (avr_extint_t *)param;
	avr_t * avr = p->io.avr;

	int up = !irq->value && value;
	int down = irq->value && !value;

	uint8_t mode = avr_extint_get_mode(p, (int)irq->irq);

	switch (mode) {
		case 0: // Level triggered (low level) interrupt
			{
				/**
				  Datasheet excerpt:
					>When the external interrupt is enabled and is configured as level triggered (only INT0/INT1),
					>the interrupt will trigger as long as the pin is held low.
					Thus we have to query the pin value continiously while it's held low and try to trigger the interrupt.
					This can be expensive, so avr_extint_set_strict_lvl_trig function provisioned to allow the user
					to turn this feature off. In this case bahaviour will be similar to the falling edge interrupt.
				 */
				if (!value) {
					avr_extint_fire_and_start_poll(p, (int)irq->irq);
				}
			}
			break;
		case 1: // Toggle-triggered interrupt
			if (up || down)
				avr_raise_interrupt(avr, &p->eint[irq->irq].vector);
			break;
		case 2: // Falling edge triggered
			if (down)
				avr_raise_interrupt(avr, &p->eint[irq->irq].vector);
			break;
		case 3: // Rising edge trigggerd
			if (up)
				avr_raise_interrupt(avr, &p->eint[irq->irq].vector);
			break;
	}
}

/* For interrupt flag reset by writing a logical one to it */
static void
avr_extint_flags_write(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_extint_t * p = (avr_extint_t *)param;

	uint8_t mask = 0;

	for (int i = 0; i < EXTINT_COUNT; i++) {
		if (!p->eint[i].vector.raised.reg)
			break;
		if (addr == p->eint[i].vector.raised.reg) {
			mask |= p->eint[i].vector.raised.mask << p->eint[i].vector.raised.bit;
		}
	}

	uint8_t masked_v = v & ~(mask);
	masked_v |= (avr->data[addr] & ~v) & mask;
	avr_core_watch_write(avr, addr, masked_v);
}

/*
 * With level mode capable interrupts,
 * when the External Interrupt Request Enable bits changes to the "enabled" state,
 * we must fire interrupt and start polling if other conditions met.
*/
static void
avr_extint_mask_write(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_extint_t * p = (avr_extint_t *)param;

	int changed = avr->data[addr] != v;
	avr_core_watch_write(avr, addr, v);

	if (!changed)
		return;
	for (int i = 0; i < EXTINT_COUNT; i++) {
		if (!p->eint[i].isc[1].reg)
			continue; // no level trig mode available
		if (addr == p->eint[i].vector.enable.reg) {
			uint8_t mode = avr_extint_get_mode(p, i);
			if (mode)
				continue;
			int bit = avr_extint_read_pin(p, i);
			if (bit)
				continue;
			avr_extint_fire_and_start_poll(p, i);
		}
	}
}

/*
 * With level mode capable interrupts,
 * when the Interrupt Sense Control bits changes to the "level" mode,
 * we must fire interrupt and start polling if other conditions met.
*/
static void
avr_extint_isc_write(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param)
{
	avr_extint_t * p = (avr_extint_t *)param;

	int changed = avr->data[addr] != v;
	avr_core_watch_write(avr, addr, v);

	if (!changed)
		return;
	for (int i = 0; i < EXTINT_COUNT; i++) {
		if (!p->eint[i].isc[1].reg)
			continue; // no level trig mode available
		if ((addr == p->eint[i].isc[0].reg) ||
			(addr == p->eint[i].isc[1].reg)) {
			uint8_t mode = avr_extint_get_mode(p, i);
			if (mode)
				continue;
			int bit = avr_extint_read_pin(p, i);
			if (bit)
				continue;
			avr_extint_fire_and_start_poll(p, i);
		}
	}
}

static void
avr_extint_reset(
		avr_io_t * port)
{
	avr_extint_t * p = (avr_extint_t *)port;

	for (int i = 0; i < EXTINT_COUNT; i++) {
		avr_irq_register_notify(p->io.irq + i, avr_extint_irq_notify, p);

		if (p->eint[i].port_ioctl) {
			if (p->eint[i].isc[1].reg) // level triggering available
				p->eint[i].strict_lvl_trig = 1; // turn on repetitive level triggering by default
			avr_irq_t * irq = avr_io_getirq(p->io.avr,
					p->eint[i].port_ioctl, p->eint[i].port_pin);

			avr_connect_irq(irq, p->io.irq + i);
		}
	}
}

static const char * irq_names[EXTINT_COUNT] = {
	[EXTINT_IRQ_OUT_INT0] = "<int0",
	[EXTINT_IRQ_OUT_INT1] = "<int1",
	[EXTINT_IRQ_OUT_INT2] = "<int2",
	[EXTINT_IRQ_OUT_INT3] = "<int3",
	[EXTINT_IRQ_OUT_INT4] = "<int4",
	[EXTINT_IRQ_OUT_INT5] = "<int5",
	[EXTINT_IRQ_OUT_INT6] = "<int6",
	[EXTINT_IRQ_OUT_INT7] = "<int7",
};

static	avr_io_t	_io = {
	.kind = "extint",
	.reset = avr_extint_reset,
	.irq_names = irq_names,
};

void
avr_extint_init(
		avr_t * avr,
		avr_extint_t * p)
{
	p->io = _io;

	avr_register_io(avr, &p->io);
	for (int i = 0; i < EXTINT_COUNT; i++) {
		p->poll_contexts[i].eint_no = i;
		p->poll_contexts[i].extint = p;
		avr_register_vector(avr, &p->eint[i].vector);
		if (p->eint[i].vector.raised.reg)
			avr_register_io_write(avr, p->eint[i].vector.raised.reg, avr_extint_flags_write, p);
		if (p->eint[i].isc[1].reg) {
			// level triggering available
			avr_register_io_write(avr, p->eint[i].vector.enable.reg, avr_extint_mask_write, p);
			avr_register_io_write(avr, p->eint[i].isc[0].reg, avr_extint_isc_write, p);
			if (p->eint[i].isc[0].reg != p->eint[i].isc[1].reg)
				avr_register_io_write(avr, p->eint[i].isc[1].reg, avr_extint_isc_write, p);
		}
	}

	// allocate this module's IRQ
	avr_io_setirqs(&p->io, AVR_IOCTL_EXTINT_GETIRQ(), EXTINT_COUNT, NULL);

}

