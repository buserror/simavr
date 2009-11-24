/*
	avr_mcu_section.h

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

#ifndef __AVR_MCU_SECTION_H__
#define __AVR_MCU_SECTION_H__

/*
 * This structure is used to pass "parameters" to the programmer or the simulator,
 * it tags the ELF file with a section that contains parameters about the physical
 * AVR this was compiled for, including the speed, model, and signature bytes.
 *
 * A programmer software can read this and verify fuses values for example, and a
 * simulator can instanciate the proper "model" of AVR, the speed and so on without
 * command line parameters.
 *
 * Exemple of use:
 *
 * #include "avr_mcu_section.h"
 * AVR_MCU(F_CPU, "atmega88");
 *
 */

typedef struct avr_mcu_t {
	long f_cpu;				// avr is little endian
	unsigned char id[4];	// signature bytes
	unsigned char fuse[4];	// optional
	char name[16];
} avr_mcu_t;

#define AVR_MCU(_speed, _name) \
const avr_mcu_t _mmcu __attribute__((section(".mmcu"))) = {\
	.f_cpu = _speed, \
	.id = {SIGNATURE_0, SIGNATURE_1, SIGNATURE_2}, \
	.name = _name,\
}

#endif
