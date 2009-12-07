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
#include "sim_twi.h"

void twi_bus_init(twi_bus_t * bus)
{
}

void twi_bus_attach(twi_bus_t * bus, twi_slave_t * slave)
{
	twi_slave_detach(slave);
	slave->bus = bus;
	slave->next = bus->slave;
	bus->slave = slave;
}

int twi_bus_start(twi_bus_t * bus, uint8_t address)
{
	// if we already have a peer, check to see if it's 
	// still matching, if so, skip the lookup
	if (bus->peer && twi_slave_match(bus->peer, address))
		return bus->peer->start(bus->peer, address, 1);
		
	bus->peer = NULL;
	twi_slave_t *s = bus->slave;
	while (s) {
		if (twi_slave_match(s, address)) {
			if (s->start(s, address, 0)) {
				bus->peer = s;
				s->byte_index = 0;
				return 1;
			}
		}
		s = s->next;
	}
	return 0;
}

void twi_bus_stop(twi_bus_t * bus)
{
	if (bus->peer && bus->peer->stop)
		bus->peer->stop(bus->peer);
	bus->peer = NULL;
}

int twi_bus_write(twi_bus_t * bus, uint8_t data)
{
	if (!bus->peer || !bus->peer->write)
		return 0;

	int res = bus->peer->write(bus->peer, data);
	if (bus->peer)
		bus->peer->byte_index++;
	return res;
}

uint8_t twi_bus_read(twi_bus_t * bus)
{
	if (!bus->peer || !bus->peer->read)
		return 0;

	uint8_t res = bus->peer->read(bus->peer);
	if (bus->peer)
		bus->peer->byte_index++;
	return res;	
}

void twi_slave_init(twi_slave_t * slave, void * param)
{
	slave->param = param;
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

int twi_slave_match(twi_slave_t * slave, uint8_t address)
{
	if (slave->has_address)
		return slave->has_address(slave, address);
	return (address & ~1) == (slave->address & ~1);
}

