/*
	i2c_mcp23008.h

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


#ifndef __I2C_MCP23008_VIRT_H__
#define __I2C_MCP23008_VIRT_H__

#include "sim_avr.h"
#include "sim_irq.h"

#define MCP23008_REG_IODIR   0x0
#define MCP23008_REG_IPOL    0x1
#define MCP23008_REG_GPINTEN 0x2
#define MCP23008_REG_DEFVAL  0x3
#define MCP23008_REG_INTCON  0x4
#define MCP23008_REGBIT_INTPOL  1
#define MCP23008_REGBIT_ODR     2
#define MCP23008_REGBIT_HAEN    3
#define MCP23008_REGBIT_DISSLW  4
#define MCP23008_REGBIT_SEQOP   5
#define MCP23008_REG_IOCON   0x5
#define MCP23008_REG_GPPU    0x6
#define MCP23008_REG_INTF    0x7
#define MCP23008_REG_INTCAP  0x8
#define MCP23008_REG_GPIO    0x9
#define MCP23008_REG_OLAT    0xa

#define MCP23008_REG_NUM     11

/*
 * This is a mcp23008 GPIO extender
 */
typedef struct i2c_mcp23008_t {
	avr_irq_t *	irq;		// irq list
	uint8_t addr_base;
	uint8_t addr_mask;
	int verbose;

	uint8_t selected;		// selected address
	int index;	// byte index in current transaction

	uint8_t reg_addr;		// read/write address register
	uint8_t reg[MCP23008_REG_NUM];
} i2c_mcp23008_t;

/*
 * Initializes device.
 *
 * The address is the TWI/i2c address base, for example 0xa0 -- the 7 MSB are
 * relevant, the bit zero is always meant as the "read write" bit.
 * The "mask" parameter specifies which bits should be matched as a slave;
 * if you want to have a peripheral that handle read and write, use '1'; if you
 * want to also match several addresses on the bus, specify these bits on the
 * mask too.
 * Example:
 * Address 0xa1 mask 0x00 will match address 0xa0 in READ only
 * Address 0xa0 mask 0x01 will match address 0xa0 in read AND write mode
 * Address 0xa0 mask 0x03 will match 0xa0 0xa2 in read and write mode
 *
 */
void
i2c_mcp23008_init(
		struct avr_t * avr,
		i2c_mcp23008_t * p,
		uint8_t addr,
		uint8_t mask);

/*
 * Attach the eeprom to the AVR's TWI master code,
 * pass AVR_IOCTL_TWI_GETIRQ(0) for example as i2c_irq_base
 */
void
i2c_mcp23008_attach(
		struct avr_t * avr,
		i2c_mcp23008_t * p,
		uint32_t i2c_irq_base );

#endif /* __I2C_MCP23008_VIRT_H__ */
