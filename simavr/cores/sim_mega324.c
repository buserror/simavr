/*
	sim_mega324.c

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

#define SIM_MMCU		"atmega324"
#define SIM_CORENAME	mcu_mega324

#define _AVR_IO_H_
#define __ASSEMBLER__
#include "avr/iom324.h"

/* borken avr-libc missing these declarations :/ */
/* The one from 324a are also wrong, like, I'm embarassed for whomever
 * put that in... */
#ifndef LFUSE_DEFAULT
#define LFUSE_DEFAULT 0 // (FUSE_CKDIV8 & FUSE_SUT1 & FUSE_SUT0 & FUSE_CKSEL3 & FUSE_CKSEL2 & FUSE_CKSEL0)
#define HFUSE_DEFAULT 0 // (FUSE_JTAGEN & FUSE_SPIEN & FUSE_BOOTSZ1 & FUSE_BOOTSZ0)
#define EFUSE_DEFAULT (0xFF)
#endif

// instantiate the new core
#include "sim_megax4.h"

static avr_t * make()
{
	return avr_core_allocate(&SIM_CORENAME.core, sizeof(struct mcu_t));
}

avr_kind_t mega324 = {
	.names = { "atmega324", "atmega324p" },
	.make = make
};

