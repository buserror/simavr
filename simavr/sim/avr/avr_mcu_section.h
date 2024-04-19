/*
	avr_mcu_section.h

	Copyright 2008-2013 Michel Pollet <buserror@gmail.com>

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
 * simulator can instantiate the proper "model" of AVR, the speed and so on without
 * command line parameters.
 *
 * Example of use:
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
	AVR_MMCU_TAG_VCD_PORTPIN,
	AVR_MMCU_TAG_VCD_IRQ,
	AVR_MMCU_TAG_VCD_SRAM_8,
	AVR_MMCU_TAG_VCD_SRAM_16,
	AVR_MMCU_TAG_PORT_EXTERNAL_PULL,
};

enum {
	SIMAVR_CMD_NONE = 0,
	SIMAVR_CMD_VCD_START_TRACE,
	SIMAVR_CMD_VCD_STOP_TRACE,
	SIMAVR_CMD_UART_LOOPBACK,
};

#if __AVR__
/*
 * WARNING. Due to newer GCC being stupid, they introduced a bug that
 * prevents us introducing variable length strings in the declaration
 * of structs. Worked for a million years, and no longer.
 * So the new method declares the string as fixed size, and the parser
 * is forced to skip the zeroes in padding. Dumbo.
 */
#define _MMCU_ __attribute__((section(".mmcu"))) __attribute__((used))
struct avr_mmcu_long_t {
	uint8_t tag;
	uint8_t len;
	uint32_t val;
} __attribute__((__packed__));

struct avr_mmcu_string_t {
	uint8_t tag;
	uint8_t len;
	char string[64];
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
	char name[32];
} __attribute__((__packed__));

#define AVR_MCU_STRING(_tag, _str) \
	const struct avr_mmcu_string_t _##_tag _MMCU_ = {\
		.tag = _tag,\
		.len = sizeof(struct avr_mmcu_string_t) - 2,\
		.string = _str,\
	}
/*
 * This trick allows concatenation of tokens. We need a macro redirection
 * for it to work.
 * The goal is to make unique variable names (they don't matter anyway)
 */
#define DO_CONCAT2(_a, _b) _a##_b
#define DO_CONCAT(_a, _b) DO_CONCAT2(_a,_b)

#define AVR_MCU_LONG(_tag, _val) \
	const struct avr_mmcu_long_t DO_CONCAT(DO_CONCAT(_, _tag), __LINE__) _MMCU_ = {\
		.tag = _tag,\
		.len = sizeof(struct avr_mmcu_long_t) - 2,\
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
	.len = sizeof(struct avr_mmcu_vcd_trace_t) - 2,\
	.name = _name

#define AVR_MCU_VCD_SRAM_8(_name) \
	.tag = AVR_MMCU_TAG_VCD_SRAM_8, \
	.len = sizeof(struct avr_mmcu_vcd_trace_t) - 2,\
	.name = _name

#define AVR_MCU_VCD_SRAM_16(_name) \
	.tag = AVR_MMCU_TAG_VCD_SRAM_16, \
	.len = sizeof(struct avr_mmcu_vcd_trace_t) - 2,\
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
	const struct avr_mmcu_addr_t _simavr_console_register _MMCU_ = {\
		.tag = AVR_MMCU_TAG_SIMAVR_CONSOLE,\
		.len = sizeof(void *),\
		.what = (void*)_register, \
	}
/*!
 * Allows the firmware to hint simavr as to wether there are external
 * pullups/down on PORT pins. It helps if the firmware uses "open drain"
 * pins by toggling the DDR pins to switch between an output state and
 * a "default" state.
 * The value passed here will be output on the PORT IRQ when the DDR
 * pin is set to input again
 */
#define AVR_MCU_EXTERNAL_PORT_PULL(_port, _mask, _val) \
	AVR_MCU_LONG(AVR_MMCU_TAG_PORT_EXTERNAL_PULL, \
		(((unsigned long)((_port)&0xff) << 16) | \
		((unsigned long)((_mask)&0xff) << 8) | \
		((_val)&0xff)));
/*!
 * Add this port/pin to the VCD file. The syntax uses the name of the
 * port as a character, and not a pointer to a register.
 * AVR_MCU_VCD_PORT_PIN('B', 5);
 */
#define AVR_MCU_VCD_PORT_PIN(_port, _pin, _name) \
	const struct avr_mmcu_vcd_trace_t DO_CONCAT(DO_CONCAT(_, _tag), __LINE__) _MMCU_ = {\
		.tag = AVR_MMCU_TAG_VCD_PORTPIN, \
		.len = sizeof(struct avr_mmcu_vcd_trace_t) - 2,\
		.mask = _port, \
		.what = (void*)_pin, \
		.name = _name, \
	}

/*!
 * These allows you to add a trace showing how long an IRQ vector is pending,
 * and also how long it is running. You can specify the IRQ as a vector name
 * straight from the firmware file, and it will be named properly in the trace
 */

#define AVR_MCU_VCD_IRQ_TRACE(_vect_number, __what, _trace_name) \
	const struct avr_mmcu_vcd_trace_t DO_CONCAT(DO_CONCAT(_, _tag), __LINE__) _MMCU_ = {\
		.tag = AVR_MMCU_TAG_VCD_IRQ, \
		.len = sizeof(struct avr_mmcu_vcd_trace_t) - 2,\
		.mask = _vect_number, \
		.what = (void*)__what, \
		.name = _trace_name, \
	};
#define AVR_MCU_VCD_IRQ(_irq_name) \
	AVR_MCU_VCD_IRQ_TRACE(_irq_name##_vect_num, 1, #_irq_name)
#define AVR_MCU_VCD_IRQ_PENDING(_irq_name) \
	AVR_MCU_VCD_IRQ_TRACE(_irq_name##_vect_num, 0, #_irq_name "_pend")
#define AVR_MCU_VCD_ALL_IRQ() \
	AVR_MCU_VCD_IRQ_TRACE(0xff, 1, "IRQ")
#define AVR_MCU_VCD_ALL_IRQ_PENDING() \
	AVR_MCU_VCD_IRQ_TRACE(0xff, 0, "IRQ_PENDING")

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
	AVR_MCU_STRING(AVR_MMCU_TAG_NAME, _name);\
	AVR_MCU_LONG(AVR_MMCU_TAG_FREQUENCY, _speed);\
	const uint8_t _mmcu[2] _MMCU_ = { AVR_MMCU_TAG, 0 }

/*
 * The following MAP macros where copied from
 * https://github.com/swansontec/map-macro/blob/master/map.h
 *
 * The license header for that file is reproduced below:
 *
 * Copyright (C) 2012 William Swanson
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the names of the authors or
 * their institutions shall not be used in advertising or otherwise to
 * promote the sale, use or other dealings in this Software without
 * prior written authorization from the authors.
 */

#define _EVAL0(...) __VA_ARGS__
#define _EVAL1(...) _EVAL0 (_EVAL0 (_EVAL0 (__VA_ARGS__)))
#define _EVAL2(...) _EVAL1 (_EVAL1 (_EVAL1 (__VA_ARGS__)))
#define _EVAL3(...) _EVAL2 (_EVAL2 (_EVAL2 (__VA_ARGS__)))
#define _EVAL4(...) _EVAL3 (_EVAL3 (_EVAL3 (__VA_ARGS__)))
#define _EVAL(...)  _EVAL4 (_EVAL4 (_EVAL4 (__VA_ARGS__)))

#define _MAP_END(...)
#define _MAP_OUT

#define _MAP_GET_END() 0, _MAP_END
#define _MAP_NEXT0(test, next, ...) next _MAP_OUT
#define _MAP_NEXT1(test, next) _MAP_NEXT0 (test, next, 0)
#define _MAP_NEXT(test, next)  _MAP_NEXT1 (_MAP_GET_END test, next)

#define _MAP0(f, x, peek, ...) f(x) _MAP_NEXT (peek, _MAP1) (f, peek, __VA_ARGS__)
#define _MAP1(f, x, peek, ...) f(x) _MAP_NEXT (peek, _MAP0) (f, peek, __VA_ARGS__)
#define _MAP(f, ...) _EVAL (-MAP1 (f, __VA_ARGS__, (), 0))

/* End of original MAP macros. */

// Define MAP macros with one additional argument
#define _MAP0_1(f, a, x, peek, ...) f(a, x) _MAP_NEXT (peek, _MAP1_1) (f, a, peek, __VA_ARGS__)
#define _MAP1_1(f, a, x, peek, ...) f(a, x) _MAP_NEXT (peek, _MAP0_1) (f, a, peek, __VA_ARGS__)
#define _MAP_1(f, a, ...) _EVAL (_MAP1_1 (f, a, __VA_ARGS__, (), 0))

#define _SEND_SIMAVR_CMD_BYTE(reg, b)            reg = b;

// A helper macro for sending multi-byte commands
#define SEND_SIMAVR_CMD(reg, ...)		\
	do { \
		_MAP_1(_SEND_SIMAVR_CMD_BYTE, reg, __VA_ARGS__) \
	} while(0)

#endif /* __AVR__ */

#ifdef __cplusplus
};
#endif

#endif
