/*
	sim_twi.h

	Internal TWI/i2c slave/master subsystem

	You can have a "bus" to talk to a bunch of "slaves"
	
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

#ifndef SIM_TWI_H_
#define SIM_TWI_H_

#include <stdint.h>

enum twi_event {
    TWI_START,
    TWI_STOP,
	// return non-zero if this slave address is handled by this slave
	// if NULL, the "address" field is used instead. If this function
	// is present, 'address' field is not used.
    TWI_PROBE,
    TWI_NACK /* Masker NACKed a receive byte.  */
};

#define TWI_ADDRESS_READ_MASK	0x01

typedef struct twi_slave_t {
	struct twi_bus_t * bus;	// bus we are attached to
	struct twi_slave_t * next;	// daisy chain on the bus
	
	void * param;		// module private parameter
	uint8_t	address;	// slave address (lowest bit is not used, it's for the W bit)
	int byte_index;		// byte index in the transaction (since last start, restart)

	// handle start conditionto address+w, restart means "stop" wasn't called
	int (*event)(struct twi_slave_t* p, uint8_t address, enum twi_event event);

	// handle a data write, after a (re)start
	int (*write)(struct twi_slave_t* p, uint8_t v);

	// handle a data read, after a (re)start
	uint8_t (*read)(struct twi_slave_t* p);
} twi_slave_t;


typedef struct twi_bus_t {
	struct twi_slave_t * slave;	// daisy chain on the bus

	struct twi_slave_t * peer;	// during a transaction, this is the selected slave
} twi_bus_t;

void twi_bus_init(twi_bus_t * bus);
int twi_bus_start(twi_bus_t * bus, uint8_t address);
int twi_bus_write(twi_bus_t * bus, uint8_t data);
uint8_t twi_bus_read(twi_bus_t * bus);
void twi_bus_stop(twi_bus_t * bus);

void twi_slave_init(twi_slave_t * slave, uint8_t address, void * param);
void twi_slave_detach(twi_slave_t * slave);
int twi_slave_match(twi_slave_t * slave, uint8_t address);

#endif /*  SIM_TWI_H_ */
