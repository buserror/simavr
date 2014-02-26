/*
	mpu-6050.h

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
	modified by Yann GOUY

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


#ifndef __MPU_6050_H___
#define __MPU_6050_H___

#include "sim_irq.h"

/*
 * Initializes an eeprom.
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
 * The "data" is optional, data is initialized as 0xff like a normal eeprom.
 */
void
mpu_6050_init(
		struct avr_t * avr,
		uint8_t addr);

#endif /* __MPU_6050_H___ */
