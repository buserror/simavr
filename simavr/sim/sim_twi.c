/*
	sim_twi.c

	Internal TWI/i2c slave/master subsystem
	
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
#include <string.h>
#include <stdio.h>
#include "sim_twi.h"

static void twi_bus_master_irq_notify(struct avr_irq_t * irq, uint32_t value, void * param)
{
	twi_bus_t * bus = (twi_bus_t *)param;
	switch (irq->irq) {
		case TWI_MASTER_STOP:
			bus->peer = NULL;
			break;
		case TWI_MASTER_START:
			bus->peer = NULL;
			bus->ack = 0;
			break;
		case TWI_MASTER_MISO:
			bus->ack = 0;
			break;
		case TWI_MASTER_MOSI:
			bus->ack = 0;
			break;
		case TWI_MASTER_ACK:
			if (!bus->peer) {
			}
			break;
	}
}

static void twi_bus_slave_irq_notify(struct avr_irq_t * irq, uint32_t value, void * param)
{
	twi_slave_t * slave = (twi_slave_t*)param;
	twi_bus_t * bus = slave->bus;
	switch (irq->irq) {
		case TWI_SLAVE_MISO:
			bus->latch = value;
			break;
		case TWI_SLAVE_ACK:
			if (!bus->peer) {
				bus->peer = slave;
				printf("twi bus: slave %x selected\n", slave->address);
			}
			bus->ack = 0x80 | (value & 1);
			break;
	}
}

static void twi_slave_irq_notify(struct avr_irq_t * irq, uint32_t value, void * param)
{
	twi_slave_t * slave = (twi_slave_t*)param;
	switch (irq->irq) {
		case TWI_MASTER_STOP:
			if (slave->match) {
				// we were target
			}
			slave->match = 0;
			break;
		case TWI_MASTER_START:
			if ((value & 0xfe) == (slave->address & 0xfe)) {
				if (slave->match) {
					// restart
				}
				slave->match = 1;
				avr_raise_irq(slave->irq + TWI_SLAVE_ACK, 1);
			}
			break;
		case TWI_MASTER_MISO:
			break;
		case TWI_MASTER_MOSI:
			break;
		case TWI_MASTER_ACK:
			break;
	}
}

void twi_bus_init(twi_bus_t * bus)
{
	memset(bus, 0, sizeof(twi_bus_t));
	//avr_init_irq(bus->irq, 0, TWI_MASTER_STATE_COUNT);
	for (int i = 0; i < TWI_MASTER_STATE_COUNT; i++)
		avr_irq_register_notify(bus->irq + i, twi_bus_master_irq_notify, bus);
}

void twi_bus_attach(twi_bus_t * bus, twi_slave_t * slave)
{
	twi_slave_detach(slave);
	slave->bus = bus;
	slave->next = bus->slave;
	bus->slave = slave;
	
	for (int i = 0; i < TWI_SLAVE_STATE_COUNT; i++)
		avr_irq_register_notify(slave->irq + i, twi_bus_slave_irq_notify, slave);
	for (int i = 0; i < TWI_MASTER_STATE_COUNT; i++)
		avr_irq_register_notify(bus->irq + i, twi_slave_irq_notify, slave);
}

int twi_bus_start(twi_bus_t * bus, uint8_t address)
{
	avr_raise_irq(bus->irq + TWI_MASTER_START, address);
	return bus->peer != NULL ? 1 : 0;
}

void twi_bus_stop(twi_bus_t * bus)
{
	avr_raise_irq(bus->irq + TWI_MASTER_STOP, 0);
}


void twi_slave_init(twi_slave_t * slave, uint8_t address, void * param)
{
	memset(slave, 0, sizeof(twi_slave_t));
	slave->address = address;
//	slave->param = param;
}

void twi_slave_detach(twi_slave_t * slave)
{
	if (!slave || !slave->bus)
		return;
	twi_slave_t *s = slave->bus->slave;
	while (s) {
		if (s->next == slave) {
			// clear that, too
			if (slave->bus->peer == slave)
				slave->bus->peer = NULL;
				
			s->next = slave->next;
			slave->next = NULL;
			slave->bus = NULL;
			return;
		}
		s = s->next;
	}
}

