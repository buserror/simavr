/*
	atmega168_timer_64led.h
	
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
#ifndef __COMMON_H__
#define __COMMON_H__


#include <util/delay.h>
#include <avr/io.h>
#include <stdint.h>
#include <stdio.h>

static inline void delay_ms(uint16_t millis)
{
 // uint16_t loop;
  while ( millis-- )
	_delay_ms(1);
}

#include <avr/pgmspace.h>

#define printf(format, ...) printf_P(PSTR(format), ## __VA_ARGS__)
#define sprintf(wh, format, ...) sprintf_P(wh, PSTR(format), ## __VA_ARGS__)

/*!
 	Define pin accessors.
 	given a pin name, port, bit number and mask (how many bits it takes) this macro
 	defines a set of inline accessors to set/clear/read the pin
 */
#define PIN_DEFINE(__name, __port, __pin, __mask) \
	enum { __name##_PIN = (__pin), __name##_MASK = (__mask << __pin) }; \
	/* toggle pin in PORT */static inline void TOG_##__name() { PIN##__port ^= __mask << __pin; } \
	/* Clear Pin */			static inline void CLR_##__name() { PORT##__port &= ~(__mask << __pin); } \
	/* Set pin to 1 */		static inline void SET_##__name() { PORT##__port |= (__mask << __pin); } \
	/* Set pin to 0/1 */	static inline void SET_##__name##_V(uint8_t __val) { PORT##__port = (PORT##__port & ~(__mask << __pin)) | (__val << __pin); } \
	/* Get pin value */		static inline uint8_t GET##__name() { return (PIN##__port >> __pin) & __mask; } \
	/* Set pin direction */	static inline void DDR_##__name(uint8_t __val) { DDR##__port = (DDR##__port & ~(__mask << __pin)) | (__val << __pin); }

#if VERBOSE
#define V(w) w
#else
#define V(w)
#endif

#endif // __COMMON_H__
