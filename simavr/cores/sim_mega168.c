/*
	sim_mega168.c

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

#include "sim_avr.h"

#define SIM_VECTOR_SIZE	4
#define SIM_MMCU		"atmega168"
#define SIM_CORENAME	mcu_mega168

#define _AVR_IO_H_
#define __ASSEMBLER__
#include "avr/iom168.h"
// instanciate the new core
#include "sim_megax8.h"

static avr_t * make()
{
	return &SIM_CORENAME.core;
}

avr_kind_t mega168 = {
	.names = { "atmega168", "atmega168p","atmega168pa" },
	.make = make
};

