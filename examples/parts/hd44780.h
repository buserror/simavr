/*
	hd44780.h

	Copyright 2011 Michel Pollet <buserror@gmail.com>

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
 * This "Part" simulates the business end of a HD44780 LCD display
 * It supports from 8x1 to 20x4 or even 40x4 (not sure that exists)
 *
 * It works both in 4 bits and 8 bits mode and supports a "quicky" method
 * of driving that is commonly used on AVR, namely
 * (msb) RW:E:RS:D7:D6:D5:D4 (lsb)
 *
 * + As usual, the "RW" pin is optional if you are willing to wait for the
 *   specific number of cycles as per the datasheet (37uS between operations)
 * + If you decide to use the RW pin, the "busy" flag is supported and will
 *   be automatically cleared on the second read, to exercise the code a bit.
 * + Cursor is supported, but no "display shift"
 * + The Character RAM is supported, but is not currently drawn.
 *
 * To interface this part, you can use the "INPUT" IRQs and hook them to the
 * simavr instance, if you use the RW pins or read back from the display, you
 * can hook the data pins /back/ to the AVR too.
 *
 * The "part" also provides various IRQs that are there to be placed in a VCD file
 * to show what is sent, and some of the internal status.
 *
 * This part has been tested with two different implementation of an AVR driver
 * for the hd44780. The one shipped in this directory is straight out of the
 * avr-libc example code.
 */
#ifndef __HD44780_H__
#define __HD44780_H__

#include "sim_irq.h"

enum {
    IRQ_HD44780_ALL = 0,	// Only if (msb) RW:E:RS:D7:D6:D5:D4 (lsb)  configured
    IRQ_HD44780_RS,
    IRQ_HD44780_RW,
    IRQ_HD44780_E,
    // bidirectional
    IRQ_HD44780_D0,IRQ_HD44780_D1,IRQ_HD44780_D2,IRQ_HD44780_D3,
    IRQ_HD44780_D4,IRQ_HD44780_D5,IRQ_HD44780_D6,IRQ_HD44780_D7,
    IRQ_HD44780_INPUT_COUNT,

    IRQ_HD44780_BUSY = IRQ_HD44780_INPUT_COUNT,	// for VCD traces sake...
    IRQ_HD44780_ADDR,
    IRQ_HD44780_DATA_IN,
    IRQ_HD44780_DATA_OUT,
    IRQ_HD44780_COUNT
};

enum {
    HD44780_FLAG_F = 0,         // 1: 5x10 Font, 0: 5x7 Font
    HD44780_FLAG_N,             // 1: 2/4-lines Display, 0: 1-line Display,
    HD44780_FLAG_D_L,           // 1: 4-Bit Interface, 0: 8-Bit Interface
    HD44780_FLAG_R_L,           // 1: Shift right, 0: shift left
    HD44780_FLAG_S_C,           // 1: Display shift, 0: Cursor move
    HD44780_FLAG_B,             // 1: Cursor Blink
    HD44780_FLAG_C,             // 1: Cursor on
    HD44780_FLAG_D,             // 1: Set Entire Display memory (for clear)
    HD44780_FLAG_S,             // 1: Follow display shift
    HD44780_FLAG_I_D,           // 1: Increment, 0: Decrement

	/*
	 * Internal flags, not HD44780
	 */
    HD44780_FLAG_DIRTY,			// 1: needs redisplay...
    HD44780_FLAG_CRAM_DIRTY,	// 1: Character memory has changed
};

/*
 * Private internal flags. These are not protected by
 * callbacks, so other threads must not access them.
 */
enum {
    HD44780_PRIV_FLAG_BUSY = 0,			// 1: Busy between instructions, 0: ready
    HD44780_PRIV_FLAG_LOWNIBBLE,		// 1: 4 bits mode, write/read low nibble
    HD44780_PRIV_FLAG_REENTRANT,		// 1: Do not update pins
};


typedef struct hd44780_t
{
	avr_irq_t * irq;
	struct avr_t * avr;
	int		w, h;				// width and height of the LCD

	uint16_t cursor;			// offset in vram
	uint8_t  vram[0x80 + 0x40];

	uint16_t pinstate;			// 'actual' LCD data pins (IRQ bit field)
	// uint16_t oldstate;			/// previous pins
	uint8_t	 datapins;			// composite of 4 high bits, or 8 bits
	uint8_t  readpins;

	uint16_t flags;				// LCD flags ( HD44780_FLAG_*)
	// LCD private flags, not protected by callbacks ( HD44780_PRIV_FLAG_*)
	// You must not use these flags from a different thread than the simavr one.
	uint8_t private_flags;

	// These callbacks are called before and after a data or
	// command packet is sent to the unit and as a 
	// result the internal state of the device changes.
	// If you lock and unlock a mutex in these, you can guard
	// the cursor, vram and flags variables with it.
	// The avr thread reads these variables outside
	// of these functions, but writes always happen between them.
	void *on_state_lock_parameter;
	void (*on_state_lock)(void *);
	void *on_state_unlock_parameter;
	void (*on_state_unlock)(void *);
} hd44780_t;

void
hd44780_init(
		struct avr_t *avr,
		struct hd44780_t * b,
		int width,
		int height );
void
hd44780_print(
		struct hd44780_t *b);

static inline int
hd44780_set_flag(
		hd44780_t *b, uint16_t bit, int val)
{
	int old = b->flags &  (1 << bit);
	b->flags = (b->flags & ~(1 << bit)) | (val ? (1 << bit) : 0);
	return old != 0;
}

static inline int
hd44780_get_flag(
		hd44780_t *b, uint16_t bit)
{
	return (b->flags &  (1 << bit)) != 0;
}

static inline int
hd44780_set_private_flag(
		hd44780_t *b, uint16_t bit, int val)
{
	int old = b->private_flags &  (1 << bit);
	b->private_flags = (b->private_flags & ~(1 << bit)) | (val ? (1 << bit) : 0);
	return old != 0;
}

static inline int
hd44780_get_private_flag(
		hd44780_t *b, uint16_t bit)
{
	return (b->private_flags &  (1 << bit)) != 0;
}

static inline void
hd44780_lock_state(hd44780_t *b)
{
	if (b->on_state_lock)
		(*(b->on_state_lock))(b->on_state_lock_parameter);
}

static inline void
hd44780_unlock_state(hd44780_t *b)
{
	if (b->on_state_unlock)
		(*(b->on_state_unlock))(b->on_state_unlock_parameter);
}

#endif
