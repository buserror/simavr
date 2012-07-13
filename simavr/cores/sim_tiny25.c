/*
	sim_tiny25.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
	                     Jon Escombe <lists@dresco.co.uk>

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

#define SIM_VECTOR_SIZE	2
#define SIM_MMCU		"attiny25"
#define SIM_CORENAME	mcu_tiny25

#define _AVR_IO_H_
#define __ASSEMBLER__
#include "avr/iotn25.h"
// instantiate the new core
#include "sim_tinyx5.h"

static avr_t * make()
{
	return avr_core_allocate(&SIM_CORENAME.core, sizeof(struct mcu_t));
}

avr_kind_t tiny25 = {
	.names = { "attiny25" },
	.make = make
};

