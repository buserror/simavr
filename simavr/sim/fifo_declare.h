/*
	fifo_declare.h

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

/*
 * FIFO helpers, aka circular buffers
 *
 * these macros define accessory for fifos of any name and any size (power of two)
 */

#ifndef __FIFO_DECLARE__
#define __FIFO_DECLARE__

/*
	doing a :
	DEFINE_FIFO(uint8_t, myfifo, 128);

	will declare :
	enum : myfifo_overflow_f
	type : myfifo_t
	functions:
		// write a byte into the fifo, return 1 if there was room, 0 if there wasn't
		int myfifo_write(myfifo_t *c, uint8_t b);
		// reads a byte from the fifo, return 0 if empty. Use myfifo_isempty() to check beforehand
		uint8_t myfifo_read(myfifo_t *c);
		int myfifo_isfull(myfifo_t *c);
		int myfifo_isempty(myfifo_t *c);
		// returns number of items to read now
		uint16_t myfifo_get_read_size(myfifo_t *c);
		// read item at offset o from read cursor, no cursor advance
		uint8_t myfifo_read_at(myfifo_t *c, uint16_t o);
		// write b at offset o compared to current write cursor, no cursor advance
		void myfifo_write_at(myfifo_t *c, uint16_t o, uint8_t b);


	To use the fifo, you must declare at least one :
	myfifo_t fifo = FIFO_NULL;

	while (!myfifo_isfull(&fifo))
		myfifo_write(&fifo, 0xaa);
	....
	while (!myfifo_isempty(&fifo))
		b = myfifo_read(&fifo);
 */

#include <stdint.h>

#if __AVR__
#define FIFO_CURSOR_TYPE	uint8_t
#define FIFO_BOOL_TYPE	char
#define FIFO_INLINE
#endif
#ifndef	FIFO_CURSOR_TYPE
#define FIFO_CURSOR_TYPE	uint16_t
#endif
#ifndef	FIFO_BOOL_TYPE
#define FIFO_BOOL_TYPE	int
#endif
#ifndef	FIFO_INLINE
#define FIFO_INLINE	inline
#endif

#define FIFO_NULL { {0}, 0, 0, 0 }

#define DECLARE_FIFO(__type, __name, __size) \
enum { __name##_overflow_f = (1 << 0) }; \
typedef struct __name##_t {			\
	__type		buffer[__size];		\
	volatile FIFO_CURSOR_TYPE	read;		\
	volatile FIFO_CURSOR_TYPE	write;		\
	volatile uint8_t	flags;		\
} __name##_t

#define DEFINE_FIFO(__type, __name, __size) \
static FIFO_INLINE FIFO_BOOL_TYPE __name##_write(__name##_t * c, __type b)\
{\
	FIFO_CURSOR_TYPE now = c->write;\
	FIFO_CURSOR_TYPE next = (now + 1) & (__size-1);\
	if (c->read != next) {	\
		c->buffer[now] = b;\
		c->write = next;\
		return 1;\
	}\
	return 0;\
}\
static inline FIFO_BOOL_TYPE __name##_isfull(__name##_t *c)\
{\
	FIFO_CURSOR_TYPE next = (c->write + 1) & (__size-1);\
	return c->read == next;\
}\
static inline FIFO_BOOL_TYPE __name##_isempty(__name##_t * c)\
{\
	return c->read == c->write;\
}\
static FIFO_INLINE __type __name##_read(__name##_t * c)\
{\
	if (c->read == c->write)\
		return 0;\
	FIFO_CURSOR_TYPE read = c->read;\
	__type res = c->buffer[read];\
	c->read = (read + 1) & (__size-1);\
	return res;\
}\
static inline FIFO_CURSOR_TYPE __name##_get_read_size(__name##_t *c)\
{\
	return c->write > c->read ? c->write - c->read : __size - 1 - c->read + c->write;\
}\
static inline void __name##_read_offset(__name##_t *c, FIFO_CURSOR_TYPE o)\
{\
	c->read = (c->read + o) & (__size-1);\
}\
static inline __type __name##_read_at(__name##_t *c, FIFO_CURSOR_TYPE o)\
{\
	return c->buffer[(c->read + o) & (__size-1)];\
}\
static inline void __name##_write_at(__name##_t *c, FIFO_CURSOR_TYPE o, __type b)\
{\
	c->buffer[(c->write + o) & (__size-1)] = b;\
}\
static inline void __name##_write_offset(__name##_t *c, FIFO_CURSOR_TYPE o)\
{\
	c->write = (c->write + o) & (__size-1);\
}\
static inline void __name##_reset(__name##_t *c)\
{\
	c->read = c->write = c->flags = 0;\
}\
struct __name##_t

#endif
