/*
	sim_mega16u2.c

	ATmega16U2, see sim_megaxu2.h

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

#define SIM_MMCU		"atmega16u2"
#define SIM_CORENAME	mcu_mega16u2

#define _AVR_IO_H_
#define __ASSEMBLER__
#ifndef __AVR_ATmega16U2__
#define __AVR_ATmega16U2__
#endif
#include "avr/iom16u2.h"

#include "sim_megaxu2.h"

static avr_t * make()
{
	return avr_core_allocate(&SIM_CORENAME.core, sizeof(struct mcu_t));
}

avr_kind_t mega16u2 = {
	.names = { "atmega16u2" },
	.make = make
};
