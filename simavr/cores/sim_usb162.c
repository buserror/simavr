/*
	sim_usb162.c

	AT90USB162, see sim_megaxu2.h

	Copyright 2012 Torbjorn Tyridal <ttyridal@gmail.com>

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

#define SIM_MMCU		"at90usb162"
#define SIM_CORENAME	mcu_usb162

#define _AVR_IO_H_
#define __ASSEMBLER__
#ifndef __AVR_AT90USB162__
#define __AVR_AT90USB162__
#endif
#include "avr/iousb162.h"

#include "sim_megaxu2.h"

static avr_t * make()
{
	return avr_core_allocate(&SIM_CORENAME.core, sizeof(struct mcu_t));
}

avr_kind_t usb162 = {
	.names = { "at90usb162" },
	.make = make
};
