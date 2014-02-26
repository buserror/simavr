/*
	i2ctest.c --> eeprom_i2c.c

	Copyright 2008-2011 Michel Pollet <buserror@gmail.com>
	modified by Yann GOUY <yann_gouy@yahoo.fr>

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

#include "sim_avr.h"
#include "avr_twi.h"
#include "i2c_eeprom.h"


i2c_eeprom_t ee;


void simu_component_init(struct avr_t * avr)
{
	// initialize our 'peripheral'
	i2c_eeprom_init(avr, &ee, 0xa0, 0xfe, NULL, 1024);

	i2c_eeprom_attach(avr, &ee, AVR_IOCTL_TWI_GETIRQ(0));
	ee.verbose = 1;
}


void simu_component_fini(struct avr_t * avr)
{
	(void)avr;
}

