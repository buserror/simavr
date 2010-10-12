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

#ifdef __cplusplus
extern "C" {
#endif

enum {
	AVR_MMCU_TAG = 0,
	AVR_MMCU_TAG_NAME,
	AVR_MMCU_TAG_FREQUENCY,
	AVR_MMCU_TAG_VCC,
	AVR_MMCU_TAG_AVCC,
	AVR_MMCU_TAG_AREF,
	AVR_MMCU_TAG_LFUSE,
	AVR_MMCU_TAG_HFUSE,
	AVR_MMCU_TAG_EFUSE,
	AVR_MMCU_TAG_SIGNATURE,
	AVR_MMCU_TAG_SIMAVR_COMMAND,
	AVR_MMCU_TAG_SIMAVR_CONSOLE,
	AVR_MMCU_TAG_VCD_FILENAME,
	AVR_MMCU_TAG_VCD_PERIOD,	
	AVR_MMCU_TAG_VCD_TRACE,
};

enum {
	SIMAVR_CMD_NONE = 0,
	SIMAVR_CMD_VCD_START_TRACE,
	SIMAVR_CMD_VCD_STOP_TRACE,
	SIMAVR_CMD_UART_LOOPBACK,
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

struct avr_mmcu_addr_t {
	uint8_t tag;
	uint8_t len;
	void * what;
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

/*!
 * This Macro allows you to specify traces for the VCD file output
 * engine. This specifies a default header, and let you fill in the
 * relevant bits.
 * Example:
 *	const struct avr_mmcu_vcd_trace_t _mytrace[]  _MMCU_ = {
 *		{ AVR_MCU_VCD_SYMBOL("UDR0"), .what = (void*)&UDR0, },
 *		{ AVR_MCU_VCD_SYMBOL("UDRE0"), .mask = (1 << UDRE0), .what = (void*)&UCSR0A, },
 *	};
 * This structure will automatically tell simavr to add a VCD trace
 * for the UART register, and the UDRE0 bit, so you can trace exactly
 * the timing of the changed using gtkwave.
 */
#define AVR_MCU_VCD_SYMBOL(_name) \
	.tag = AVR_MMCU_TAG_VCD_TRACE, \
	.len = sizeof(struct avr_mmcu_vcd_trace_t) - 2 + sizeof(_name),\
	.name = _name

/*!
 * Specifies the name and wanted period (in usec) for a VCD file
 * this is not mandatory for the VCD output to work, if this tag
 * is not used, a VCD file will still be created with default values
 */
#define AVR_MCU_VCD_FILE(_name, _period) \
	AVR_MCU_STRING(AVR_MMCU_TAG_VCD_FILENAME, _name);\
	AVR_MCU_LONG(AVR_MMCU_TAG_VCD_PERIOD, _period)

/*!
 * It is possible to send "commands" to simavr from the
 * firmware itself. For this to work you need to specify
 * an IO register that is to be used for a write-only
 * bridge. A favourite is one of the usual "GPIO register"
 * that most (all ?) AVR have.
 * See definition of SIMAVR_CMD_* to see what commands can
 * be used from your firmware.
 */
#define AVR_MCU_SIMAVR_COMMAND(_register) \
	const struct avr_mmcu_addr_t _simavr_command_register _MMCU_ = {\
		.tag = AVR_MMCU_TAG_SIMAVR_COMMAND,\
		.len = sizeof(void *),\
		.what = (void*)_register, \
	}
/*!
 * Similar to AVR_MCU_SIMAVR_COMMAND, The CONSOLE allows the AVR code
 * to declare a register (typically a GPIO register, but any unused
 * register can work...) that will allow printing on the host's console
 * without using a UART to do debug.
 */
#define AVR_MCU_SIMAVR_CONSOLE(_register) \
	const struct avr_mmcu_addr_t _simavr_command_register _MMCU_ = {\
		.tag = AVR_MMCU_TAG_SIMAVR_CONSOLE,\
		.len = sizeof(void *),\
		.what = (void*)_register, \
	}

/*!
 * This tag allows you to specify the voltages used by your board
 * It is optional in most cases, but you will need it if you use
 * ADC module's IRQs. Not specifying it in this case might lead
 * to a divide-by-zero crash.
 * The units are Volts*1000 (millivolts)
 */
#define AVR_MCU_VOLTAGES(_vcc, _avcc, _aref) \
	AVR_MCU_LONG(AVR_MMCU_TAG_VCC, (_vcc));\
	AVR_MCU_LONG(AVR_MMCU_TAG_AVCC, (_avcc));\
	AVR_MCU_LONG(AVR_MMCU_TAG_AREF, (_aref));

/*!
 * This the has to be used if you want to add other tags to the .mmcu section
 * the _mmcu symbol is used as an anchor to make sure it stays linked in.
 */
#define AVR_MCU(_speed, _name) \
	const uint8_t _mmcu[2] _MMCU_ = { AVR_MMCU_TAG, 0 }; \
	AVR_MCU_STRING(AVR_MMCU_TAG_NAME, _name);\
	AVR_MCU_LONG(AVR_MMCU_TAG_FREQUENCY, _speed)

#endif /* __AVR__ */

#ifdef __cplusplus
};
#endif

#endif
