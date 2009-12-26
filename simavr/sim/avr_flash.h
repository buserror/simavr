/*
	avr_flash.h

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


#ifndef __AVR_FLASH_H___
#define __AVR_FLASH_H___

#include "sim_avr.h"

/*
 * Handles self-programming subsystem if the core
 * supports it.
 */
typedef struct avr_flash_t {
	avr_io_t	io;

	uint16_t	spm_pagesize;
	uint8_t r_spm;
	avr_regbit_t selfprgen;
	avr_regbit_t pgers;		// page erase
	avr_regbit_t pgwrt;		// page write
	avr_regbit_t blbset;	// lock bit set

	avr_int_vector_t flash;	// Interrupt vector
} avr_flash_t;

void avr_flash_init(avr_t * avr, avr_flash_t * p);


#define AVR_IOCTL_FLASH_SPM		AVR_IOCTL_DEF('f','s','p','m')

#define AVR_SELFPROG_DECLARE(_spmr, _spen, _vector) \
	.selfprog = {\
		.r_spm = _spmr,\
		.spm_pagesize = SPM_PAGESIZE,\
		.selfprgen = AVR_IO_REGBIT(_spmr, _spen),\
		.pgers = AVR_IO_REGBIT(_spmr, PGERS),\
		.pgwrt = AVR_IO_REGBIT(_spmr, PGWRT),\
		.blbset = AVR_IO_REGBIT(_spmr, BLBSET),\
		.flash = {\
			.enable = AVR_IO_REGBIT(_spmr, SPMIE),\
			.vector = _vector,\
		},\
	}

#endif /* __AVR_FLASH_H___ */
