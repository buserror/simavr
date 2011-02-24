/*
	sim_regbit.h

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

#ifndef __SIM_REGBIT_H__
#define __SIM_REGBIT_H__

#include "sim_avr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ARRAY_SIZE(_aa) (sizeof(_aa) / sizeof((_aa)[0]))

/*
 * this 'structure' is a packed representation of an IO register 'bit'
 * (or consecutive bits). This allows a way to set/get/clear them.
 * gcc is happy passing these as register value, so you don't need to
 * use a pointer when passing them along to functions.
 *
 * 9 bits ought to be enough, as it's the maximum I've seen (atmega2560)
 */
typedef struct avr_regbit_t {
	unsigned long reg : 9, bit : 3, mask : 8;
} avr_regbit_t;

/*
 * These accessors are inlined and are used to perform the operations on
 * avr_regbit_t definitions. This is the "official" way to access bits into registers
 * The small footprint costs brings much better versatility for functions/bits that are
 * not always defined in the same place on real AVR cores
 */
/*
 * set/get/clear io register bits in one operation
 */
static inline uint8_t avr_regbit_set(avr_t * avr, avr_regbit_t rb)
{
	uint8_t a = rb.reg;
	if (!a)
		return 0;
	uint8_t m = rb.mask << rb.bit;
	avr_core_watch_write(avr, a, avr->data[a] | m);
	return (avr->data[a] >> rb.bit) & rb.mask;
}

static inline uint8_t avr_regbit_setto(avr_t * avr, avr_regbit_t rb, uint8_t v)
{
	uint8_t a = rb.reg;
	if (!a)
		return 0;
	uint8_t m = rb.mask << rb.bit;
	avr_core_watch_write(avr, a, (avr->data[a] & ~(m)) | ((v << rb.bit) & m));
	return (avr->data[a] >> rb.bit) & rb.mask;
}

/*
 * Set the 'raw' bits, if 'v' is the unshifted value of the bits
 */
static inline uint8_t avr_regbit_setto_raw(avr_t * avr, avr_regbit_t rb, uint8_t v)
{
	uint8_t a = rb.reg;
	if (!a)
		return 0;
	uint8_t m = rb.mask << rb.bit;
	avr_core_watch_write(avr, a, (avr->data[a] & ~(m)) | ((v) & m));
	return (avr->data[a]) & (rb.mask << rb.bit);
}

static inline uint8_t avr_regbit_get(avr_t * avr, avr_regbit_t rb)
{
	uint8_t a = rb.reg;
	if (!a)
		return 0;
	//uint8_t m = rb.mask << rb.bit;
	return (avr->data[a] >> rb.bit) & rb.mask;
}

/*
 * Return the bit(s) 'in position' instead of zero based
 */
static inline uint8_t avr_regbit_get_raw(avr_t * avr, avr_regbit_t rb)
{
	uint8_t a = rb.reg;
	if (!a)
		return 0;
	//uint8_t m = rb.mask << rb.bit;
	return (avr->data[a]) & (rb.mask << rb.bit);
}

static inline uint8_t avr_regbit_clear(avr_t * avr, avr_regbit_t rb)
{
	uint8_t a = (rb.reg);
	uint8_t m = rb.mask << rb.bit;
	avr_core_watch_write(avr, a, avr->data[a] & ~m);
	return avr->data[a];
}


/*
 * This reads the bits for an array of avr_regbit_t, make up a "byte" with them.
 * This allows reading bits like CS0, CS1, CS2 etc even if they are not in the same
 * physical IO register.
 */
static inline uint8_t avr_regbit_get_array(avr_t * avr, avr_regbit_t *rb, int count)
{
	uint8_t res = 0;

	for (int i = 0; i < count; i++, rb++) if (rb->reg) {
		uint8_t a = (rb->reg);
		res |= ((avr->data[a] >> rb->bit) & rb->mask) << i;
	}
	return res;
}

#define AVR_IO_REGBIT(_io, _bit) { . reg = (_io), .bit = (_bit), .mask = 1 }
#define AVR_IO_REGBITS(_io, _bit, _mask) { . reg = (_io), .bit = (_bit), .mask = (_mask) }

#ifdef __cplusplus
};
#endif

#endif /* __SIM_REGBIT_H__ */
