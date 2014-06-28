/*
	SSD1306.h

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
 * This "Part" simulates the business end of a SSD1306 LCD display
 * It supports from 8x1 to 20x4 or even 40x4 (not sure that exists)
 *
 * It works both in 4 bits and 8 bits mode and supports a "quicky" method
 * of driving that is commonly used on AVR, namely
 * (msb) RW:E:RS:D7:D6:D5:D4 (lsb)
 *
 * + As usual, the "RW" pin is optional if you are willing to wait for the
 *   specific number of cycles as per the datasheet (37uS between operations)
 * + If you decide to use the RW pin, the "busy" flag is supported and will
 *   be automaticly cleared on the second read, to exercisee the code a bit.
 * + Cursor is supported, but now "display shift"
 * + The Character RAM is supported, but is not currently drawn.
 *
 * To interface this part, you can use the "INPUT" IRQs and hook them to the
 * simavr instance, if you use the RW pins or read back frim the display, you
 * can hook the data pins /back/ to the AVR too.
 *
 * The "part" also provides various IRQs that are there to be placed in a VCD file
 * to show what is sent, and some of the internal status.
 *
 * This part has been tested with two different implementation of an AVR driver
 * for the SSD1306. The one shipped in this directory is straight out of the
 * avr-libc example code.
 */
#ifndef __SSD1306_VIRT_H__
#define __SSD1306_VIRT_H__

#include "sim_irq.h"

enum {
    IRQ_SSD1306_ALL = 0,
    IRQ_SSD1306_RESET,
    IRQ_SSD1306_DATA_INSTRUCTION,
    IRQ_SSD1306_ENABLE,
    IRQ_SSD1306_SPI_BYTE_IN,
    IRQ_SSD1306_INPUT_COUNT,
    IRQ_SSD1306_COUNT

    //TODO: Add IRQs for VCD: Internal state etc.
};

enum {
    SSD1306_FLAG_F = 0,         // 1: 5x10 Font, 0: 5x7 Font
    SSD1306_FLAG_N,             // 1: 2/4-lines Display, 0: 1-line Display,
    SSD1306_FLAG_D_L,           // 1: 4-Bit Interface, 0: 8-Bit Interface
    SSD1306_FLAG_R_L,           // 1: Shift right, 0: shift left
    SSD1306_FLAG_S_C,           // 1: Display shift, 0: Cursor move
    SSD1306_FLAG_B,             // 1: Cursor Blink
    SSD1306_FLAG_C,             // 1: Cursor on
    SSD1306_FLAG_D,             // 1: Set Entire Display memory (for clear)
    SSD1306_FLAG_S,             // 1: Follow display shift
    SSD1306_FLAG_I_D,			// 1: Increment, 0: Decrement

    /*
     * Internal flags, not SSD1306
     */
    SSD1306_FLAG_LOWNIBBLE,		// 1: 4 bits mode, write/read low nibble
    SSD1306_FLAG_BUSY,			// 1: Busy between instruction, 0: ready
    SSD1306_FLAG_REENTRANT,		// 1: Do not update pins

    SSD1306_FLAG_DIRTY,			// 1: needs redisplay...
    SSD1306_FLAG_CRAM_DIRTY,	// 1: Character memory has changed
};


typedef struct ssd1306_t
{
	avr_irq_t * irq;
	struct avr_t * avr;
	int	w, h;			// width and height of the LCD
	uint16_t cursor;		// offset in vram
	uint8_t  vram[128 * 64];	// p25 ds: GDDRAM = 128x64bit in 8 pages
	uint16_t flags;			// LCD flags ( SSD1306_FLAG_*)
	uint8_t pages;
} ssd1306_t;

void
ssd1306_init(
		struct avr_t *avr,
		struct ssd1306_t * b,
		int width,
		int height );
void
ssd1306_print(
		struct ssd1306_t *b);

static inline int
ssd1306_set_flag(
    ssd1306_t *b, uint16_t bit, int val)
{
	int old = b->flags &  (1 << bit);
	b->flags = (b->flags & ~(1 << bit)) | (val ? (1 << bit) : 0);
	return old != 0;
}

static inline int
ssd1306_get_flag(
    ssd1306_t *b, uint16_t bit)
{
	return (b->flags &  (1 << bit)) != 0;
}

#endif 
