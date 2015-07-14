/*
	fido_declare.h
	Copyright (C) 2003-2012 Michel Pollet <buserror@gmail.com>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
 * FIFO helpers, aka circular buffers
 *
 * these macros define accessories for FIFOs of any name, type and
 * any (power of two) size
 */

#ifndef __FIFO_DECLARE__
#define __FIFO_DECLARE__

#ifdef __cplusplus
extern "C" {
#endif

/*
	doing a :
	DECLARE_FIFO(uint8_t, myfifo, 128);

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

	In your .c you need to 'implement' the fifo:
	DEFINE_FIFO(uint8_t, myfifo)

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
#define FIFO_SYNC
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

/* We should not need volatile */
#ifndef FIFO_VOLATILE
#define FIFO_VOLATILE
#endif
#ifndef FIFO_SYNC
#define FIFO_SYNC __sync_synchronize()
#endif

#ifndef FIFO_ZERO_INIT
#define FIFO_ZERO_INIT {0}
#endif
#define FIFO_NULL { FIFO_ZERO_INIT, 0, 0, 0 }

/* New compilers don't like unused static functions. However,
 * we do like 'static inlines' for these small accessors,
 * so we mark them as 'unused'. It stops it complaining */
#ifdef __GNUC__
#define FIFO_DECL static __attribute__ ((unused))
#else
#define FIFO_DECL static
#endif

#define DECLARE_FIFO(__type, __name, __size) \
enum { __name##_overflow_f = (1 << 0) }; \
enum { __name##_fifo_size = (__size) }; \
typedef struct __name##_t {			\
	__type		buffer[__name##_fifo_size];		\
	FIFO_VOLATILE FIFO_CURSOR_TYPE	read;		\
	FIFO_VOLATILE FIFO_CURSOR_TYPE	write;		\
	FIFO_VOLATILE uint8_t	flags;		\
} __name##_t

#define DEFINE_FIFO(__type, __name) \
FIFO_DECL FIFO_INLINE FIFO_BOOL_TYPE __name##_write(__name##_t * c, __type b)\
{\
	FIFO_CURSOR_TYPE now = c->write;\
	FIFO_CURSOR_TYPE next = (now + 1) & (__name##_fifo_size-1);\
	if (c->read != next) {	\
		c->buffer[now] = b;\
		FIFO_SYNC; \
		c->write = next;\
		return 1;\
	}\
	return 0;\
}\
FIFO_DECL FIFO_INLINE FIFO_BOOL_TYPE __name##_isfull(__name##_t *c)\
{\
	FIFO_CURSOR_TYPE next = (c->write + 1) & (__name##_fifo_size-1);\
	return c->read == next;\
}\
FIFO_DECL FIFO_INLINE FIFO_BOOL_TYPE __name##_isempty(__name##_t * c)\
{\
	return c->read == c->write;\
}\
FIFO_DECL FIFO_INLINE __type __name##_read(__name##_t * c)\
{\
	__type res = FIFO_ZERO_INIT; \
	FIFO_CURSOR_TYPE read = c->read;\
	if (read == c->write)\
		return res;\
	res = c->buffer[read];\
	FIFO_SYNC; \
	c->read = (read + 1) & (__name##_fifo_size-1);\
	return res;\
}\
FIFO_DECL FIFO_INLINE FIFO_CURSOR_TYPE __name##_get_read_size(__name##_t *c)\
{\
	return ((c->write + __name##_fifo_size) - c->read) & (__name##_fifo_size-1);\
}\
FIFO_DECL FIFO_INLINE FIFO_CURSOR_TYPE __name##_get_write_size(__name##_t *c)\
{\
	return (__name##_fifo_size-1) - __name##_get_read_size(c);\
}\
FIFO_DECL FIFO_INLINE void __name##_read_offset(__name##_t *c, FIFO_CURSOR_TYPE o)\
{\
	FIFO_SYNC; \
	c->read = (c->read + o) & (__name##_fifo_size-1);\
}\
FIFO_DECL FIFO_INLINE __type __name##_read_at(__name##_t *c, FIFO_CURSOR_TYPE o)\
{\
	return c->buffer[(c->read + o) & (__name##_fifo_size-1)];\
}\
FIFO_DECL FIFO_INLINE void __name##_write_at(__name##_t *c, FIFO_CURSOR_TYPE o, __type b)\
{\
	c->buffer[(c->write + o) & (__name##_fifo_size-1)] = b;\
}\
FIFO_DECL FIFO_INLINE void __name##_write_offset(__name##_t *c, FIFO_CURSOR_TYPE o)\
{\
	FIFO_SYNC; \
	c->write = (c->write + o) & (__name##_fifo_size-1);\
}\
FIFO_DECL FIFO_INLINE void __name##_reset(__name##_t *c)\
{\
	FIFO_SYNC; \
	c->read = c->write = c->flags = 0;\
}\
struct __name##_t

#ifdef __cplusplus
};
#endif

#endif
