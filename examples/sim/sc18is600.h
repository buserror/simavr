/*
	sc18is600.h

	Copyright Yann GOUY <yann_gouy@yahoo.fr>

	partially taken by examples/board_i2c_eeprom
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


//
// quite basic simulation of the SC18IS600 SPI component
//

#include "sim_irq.h"

//--------------------------------------------------------------------
// public types
//

typedef enum {
	SC18_I2C_IRQ_IN,
	SC18_I2C_IRQ_OUT,
   	SC18_I2C_IRQ_COUNT,
} _sc18_i2c_irq_t;


//--------------------------------------------------------------------
// public functions
//

extern avr_irq_t * sc18is600_i2c_irq_get(void);


