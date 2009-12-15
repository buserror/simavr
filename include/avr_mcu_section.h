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
 * This header is used to pass "parameters" to the programmer or the simulator,
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

#include <stdint.h>

enum {
	AVR_MMCU_TAG = 0,
	AVR_MMCU_TAG_NAME,
	AVR_MMCU_TAG_FREQUENCY,
	AVR_MMCU_TAG_LFUSE,
	AVR_MMCU_TAG_HFUSE,
	AVR_MMCU_TAG_EFUSE,
	AVR_MMCU_TAG_SIGNATURE,
	AVR_MMCU_TAG_VCD_FILENAME,
	AVR_MMCU_TAG_VCD_PERIOD,	
	AVR_MMCU_TAG_VCD_TRACE,
};

#if __AVR__

#define _MMCU_ __attribute__((section(".mmcu")))
struct avr_mmcu_long_t {
	uint8_t tag;
	uint8_t len;
	uint32_t val; 
} __attribute__((__packed__));

struct avr_mmcu_string_t {
	uint8_t tag;
	uint8_t len;
	char string[]; 
} __attribute__((__packed__));

struct avr_mmcu_vcd_trace_t {
	uint8_t tag;
	uint8_t len;
	uint8_t mask;
	void * what;
	char name[]; 
} __attribute__((__packed__));

#define AVR_MCU_STRING(_tag, _str) \
const struct avr_mmcu_string_t _##_tag _MMCU_ = {\
	.tag = _tag,\
	.len = sizeof(_str),\
	.string = _str,\
}

#define AVR_MCU_LONG(_tag, _val) \
const struct avr_mmcu_long_t _##_tag _MMCU_ = {\
	.tag = _tag,\
	.len = sizeof(uint32_t),\
	.val = _val,\
}

#define AVR_MCU_BYTE(_tag, _val) \
const uint8_t _##_tag _MMCU_ = { _tag, 1, _val }

#define AVR_MCU_VCD_SYMBOL(_name) \
	.tag = AVR_MMCU_TAG_VCD_TRACE, \
	.len = sizeof(struct avr_mmcu_vcd_trace_t) - 2 + sizeof(_name),\
	.name = _name

// specified the nane and wanted period (usec) for a VCD file
// thid is not mandatory, a default one will be created if
// symbols are declared themselves
#define AVR_MCU_VCD_FILE(_name, _period) \
	AVR_MCU_STRING(AVR_MMCU_TAG_VCD_FILENAME, _name);\
	AVR_MCU_LONG(AVR_MMCU_TAG_VCD_PERIOD, _period)

/*
 * This the has to be used if you want to add other tags to the .mmcu section
 * the _mmcu symbol is used as an anchor to make sure it stays linked in.
 */
#define AVR_MCU(_speed, _name) \
	const uint8_t _mmcu[2] _MMCU_ = { AVR_MMCU_TAG, 0 }; \
	AVR_MCU_STRING(AVR_MMCU_TAG_NAME, _name);\
	AVR_MCU_LONG(AVR_MMCU_TAG_FREQUENCY, _speed)

#endif /* __AVR__ */


#endif
