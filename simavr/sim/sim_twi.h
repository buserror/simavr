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
#include "sim_irq.h"

/*
 * The TWI system is designed to be representing the same state as 
 * a TWI/i2c bus itself. So each "state" of the bus is an IRQ sent
 * by the master to the slave, with a couple sent from the
 * slave to the master.
 * This is designed to decorelate the operations on the "bus" so
 * the firmware has time to "run" before acknowledging a byte, for
 * example.
 * 
 * IRQ Timeline goes as follow  with an example transaction that
 * does write addres, write registrer, read a byte after a i2c restart
 * then stops the transaction.
 * 
 * Master:	START	MOSI	START	MISO	ACK	STOP
 * Slave:		ACK		ACK		ACK		MISO	
 */
enum twi_state_e {
	TWI_MASTER_STOP = 0,
	TWI_MASTER_START,			// master does a start with address
	TWI_MASTER_MOSI,			// master i2c write
	TWI_MASTER_MISO,			// master i2c read
	TWI_MASTER_ACK,				// master i2c ACK after a i2c read
	TWI_MASTER_STATE_COUNT,

	TWI_SLAVE_MISO = 0,			// slave i2c read.
	TWI_SLAVE_ACK,				// slave acknowledges TWI_MASTER_MOSI
	TWI_SLAVE_STATE_COUNT,	
};

#define TWI_ADDRESS_READ_MASK	0x01

typedef struct twi_slave_t {
	avr_irq_t		irq[TWI_SLAVE_STATE_COUNT];

	struct twi_bus_t * bus;	// bus we are attached to
	struct twi_slave_t * next;	// daisy chain on the bus
	
	uint32_t		address;	// can specify up to 4 matching addresses here
	int				match;		// we are selected on the bus
	int				index;		// byte index in the transaction
	
	uint8_t			latch;		// last received byte
} twi_slave_t;


typedef struct twi_bus_t {
	avr_irq_t		irq[TWI_MASTER_STATE_COUNT];
	
	struct twi_slave_t * slave;	// daisy chain on the bus
	struct twi_slave_t * peer;	// during a transaction, this is the selected slave

	uint8_t			latch;		// last received byte
	uint8_t			ack;		// last received ack
} twi_bus_t;

void twi_bus_init(twi_bus_t * bus);
int twi_bus_start(twi_bus_t * bus, uint8_t address);
int twi_bus_write(twi_bus_t * bus, uint8_t data);
uint8_t twi_bus_read(twi_bus_t * bus);
void twi_bus_stop(twi_bus_t * bus);

void twi_slave_init(twi_slave_t * slave, uint8_t address, void * param);
void twi_slave_detach(twi_slave_t * slave);

#endif /*  SIM_TWI_H_ */
