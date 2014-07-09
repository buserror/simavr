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

#define SSD1306_VIRT_DATA		1
#define SSD1306_VIRT_INSTRUCTION 	0

/* Fundamental commands. */
#define SSD1306_VIRT_SET_CONTRAST	0x81
#define SSD1306_VIRT_EON_OFF		0xA4
#define SSD1306_VIRT_EON_ON		0xA5
#define SSD1306_VIRT_DISP_NORMAL	0xA6
#define SSD1306_VIRT_DISP_INVERTED	0xA7
#define SSD1306_VIRT_DISP_OFF 		0xAE
#define SSD1306_VIRT_DISP_ON		0xAF

/* Scrolling commands */
#define SSD1306_VIRT_SCROLL_RIGHT	0x26
#define SSD1306_VIRT_SCROLL_LEFT	0x27
#define SSD1306_VIRT_SCROLL_VR		0x29
#define SSD1306_VIRT_SCROLL_VL		0x2A
#define SSD1306_VIRT_SCROLL_OFF		0x2E
#define SSD1306_VIRT_SCROLL_ON   	0x2F
#define SSD1306_VIRT_VERT_SCROLL_A  	0xA3

/* Address setting commands */
#define SSD1306_VIRT_SET_COL_LO		0x00
#define SSD1306_VIRT_SET_COL_HI		0x10
#define SSD1306_VIRT_MEM_ADDRESSING 	0x20
#define SSD1306_VIRT_SET_COL_ADDR	0x21
#define SSD1306_VIRT_SET_PAGE_ADDR	0x22
#define SSD1306_VIRT_SET_PAGE		0xB0

/* Hardware config. commands */
#define SSD1306_VIRT_SET_LINE		0x40
#define SSD1306_VIRT_SET_SEG_REMAP0  	0xA0
#define SSD1306_VIRT_SET_SEG_REMAP1	0xA1
#define SSD1306_VIRT_MULTIPLEX       	0xA8
#define SSD1306_VIRT_SET_SCAN_FLIP	0xC0
#define SSD1306_VIRT_SET_SCAN_NOR	0xC8
#define SSD1306_VIRT_SET_OFFSET		0xD3
#define SSD1306_VIRT_SET_PADS    	0xDA

/* Timing & driving scheme setting commands */
#define SSD1306_VIRT_SET_RATIO_OSC	0xD5
#define SSD1306_VIRT_SET_CHARGE  	0xD9
#define SSD1306_VIRT_SET_VCOM    	0xDB
#define SSD1306_VIRT_NOP     		0xE3

/* Charge pump command table */
#define SSD1306_VIRT_CHARGE_PUMP    	0x8D
#define SSD1306_VIRT_PUMP_OFF    	0x10
#define SSD1306_VIRT_PUMP_ON     	0x14

#define SSD1306_CLEAR_COMMAND_REG 	part->command_register = 0x00

enum {
    //IRQ_SSD1306_ALL = 0,
    IRQ_SSD1306_SPI_BYTE_IN,
    IRQ_SSD1306_ENABLE,
    IRQ_SSD1306_RESET,
    IRQ_SSD1306_DATA_INSTRUCTION,
    //IRQ_SSD1306_INPUT_COUNT,
    IRQ_SSD1306_ADDR,		//<< For VCD
    IRQ_SSD1306_COUNT
    //TODO: Add IRQs for VCD: Internal state etc.
};

enum {
    SSD1306_FLAG_DISPLAY_INVERTED = 0,

    /*
     * Internal flags, not SSD1306
     */
    SSD1306_FLAG_BUSY,			// 1: Busy between instruction, 0: ready
    SSD1306_FLAG_REENTRANT,		// 1: Do not update pins
    SSD1306_FLAG_DIRTY,			// 1: needs redisplay...
};


typedef struct ssd1306_t
{
	avr_irq_t * irq;
	struct avr_t * avr;
	int	w, h;			// width and height of the LCD
	uint16_t cursor;		// offset in vram
	uint8_t  vram[1024];		// p25 ds: GDDRAM = 128x64bit in 8 pages
	uint16_t flags;			// LCD flags ( SSD1306_FLAG_*)
	uint8_t command_register;
	uint8_t contrast_register;
	uint8_t pages;
	uint8_t cs;
	uint8_t di;
	uint8_t spi_data;
} ssd1306_t;

void
ssd1306_init(
		struct avr_t *avr,
		struct ssd1306_t * b,
		int width,
		int height );

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

void
ssd1306_connect (ssd1306_t * part);

#endif 
