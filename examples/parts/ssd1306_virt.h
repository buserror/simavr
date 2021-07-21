/*
 ssd1306_virt.h

 Copyright 2011 Michel Pollet <buserror@gmail.com>
 Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>

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
 * This "Part" simulates the SSD1306 OLED display driver.
 *
 * The following functions are currently supported:
 *
 * > Display reset
 * > Display on / suspend
 * > Setting of the contrast
 * > Inversion of the display
 * > Rotation of the display
 * > Writing to the VRAM using horizontal addressing mode
 *
 * It has been tested on a "JY MCU v1.5 OLED" in 4 wire SPI mode
 * with the E/RD and R/W lines hard wired low as per the datasheet.
 *
 */

#ifndef __SSD1306_VIRT_H__
#define __SSD1306_VIRT_H__

#include "sim_irq.h"

#define SSD1306_VIRT_DATA			1
#define SSD1306_VIRT_INSTRUCTION 		0

#define SSD1306_I2C_ADDRESS			0x3C
#define SSD1306_I2C_ADDRESS_MASK		0xfe

#define SSD1306_VIRT_PAGES			8
#define SSD1306_VIRT_COLUMNS			128

/* Fundamental commands. */
#define SSD1306_VIRT_SET_CONTRAST		0x81
#define SSD1306_VIRT_RESUME_TO_RAM_CONTENT	0xA4
#define SSD1306_VIRT_IGNORE_RAM_CONTENT		0xA5
#define SSD1306_VIRT_DISP_NORMAL		0xA6
#define SSD1306_VIRT_DISP_INVERTED		0xA7
#define SSD1306_VIRT_DISP_SUSPEND		0xAE
#define SSD1306_VIRT_DISP_ON			0xAF

/* Scrolling commands */
#define SSD1306_VIRT_SCROLL_RIGHT		0x26
#define SSD1306_VIRT_SCROLL_LEFT		0x27
#define SSD1306_VIRT_SCROLL_VR			0x29
#define SSD1306_VIRT_SCROLL_VL			0x2A
#define SSD1306_VIRT_SCROLL_OFF			0x2E
#define SSD1306_VIRT_SCROLL_ON   		0x2F
#define SSD1306_VIRT_VERT_SCROLL_A  		0xA3

/* Address setting commands */
#define SSD1306_VIRT_SET_COLUMN_LOW_NIBBLE	0x00
#define SSD1306_VIRT_SET_COLUMN_HIGH_NIBBLE	0x10
#define SSD1306_VIRT_MEM_ADDRESSING 		0x20
#define SSD1306_VIRT_SET_COL_ADDR		0x21
#define SSD1306_VIRT_SET_PAGE_ADDR		0x22
#define SSD1306_VIRT_SET_PAGE_START_ADDR	0xB0

/* Hardware config. commands */
#define SSD1306_VIRT_SET_LINE			0x40
#define SSD1306_VIRT_SET_SEG_REMAP_0  		0xA0
#define SSD1306_VIRT_SET_SEG_REMAP_127		0xA1
#define SSD1306_VIRT_MULTIPLEX       		0xA8
#define SSD1306_VIRT_SET_COM_SCAN_NORMAL	0xC0
#define SSD1306_VIRT_SET_COM_SCAN_INVERTED	0xC8
#define SSD1306_VIRT_SET_OFFSET			0xD3
#define SSD1306_VIRT_SET_PADS    		0xDA

/* Timing & driving scheme setting commands */
#define SSD1306_VIRT_SET_RATIO_OSC		0xD5
#define SSD1306_VIRT_SET_CHARGE  		0xD9
#define SSD1306_VIRT_SET_VCOM    		0xDB
#define SSD1306_VIRT_NOP     			0xE3

/* Charge pump command table */
#define SSD1306_VIRT_CHARGE_PUMP    		0x8D
#define SSD1306_VIRT_PUMP_ON     		0x14

#define SSD1306_CLEAR_COMMAND_REG(part)		part->command_register = 0x00

enum
{
	//IRQ_SSD1306_ALL = 0,
	IRQ_SSD1306_SPI_BYTE_IN,
	IRQ_SSD1306_ENABLE,
	IRQ_SSD1306_RESET,
	IRQ_SSD1306_DATA_INSTRUCTION,
	//IRQ_SSD1306_INPUT_COUNT,
	IRQ_SSD1306_ADDR,		// << For VCD
	IRQ_SSD1306_TWI_IN,
	IRQ_SSD1306_TWI_OUT,
	IRQ_SSD1306_COUNT
//TODO: Add IRQs for VCD: Internal state etc.
};

enum
{
	SSD1306_FLAG_DISPLAY_INVERTED = 0,
	SSD1306_FLAG_DISPLAY_ON,
	SSD1306_FLAG_SEGMENT_REMAP_0,
	SSD1306_FLAG_COM_SCAN_NORMAL,

	/*
	 * Internal flags, not SSD1306
	 */
	SSD1306_FLAG_BUSY,		// 1: Busy between instruction, 0: ready
	SSD1306_FLAG_REENTRANT,		// 1: Do not update pins
	SSD1306_FLAG_DIRTY,			// 1: Needs redisplay...
};

enum ssd1306_addressing_mode_t
{
	SSD1306_ADDR_MODE_HORZ = 0,
	SSD1306_ADDR_MODE_VERT,
	SSD1306_ADDR_MODE_PAGE
};

/*
 * Cursor position in VRAM
 */
struct ssd1306_virt_cursor_t
{
	uint8_t page;
	uint8_t column;
};

typedef struct ssd1306_t
{
	avr_irq_t * irq;
	struct avr_t * avr;
	uint8_t columns, rows, pages;
	struct ssd1306_virt_cursor_t cursor, write_cursor_start, write_cursor_end;
	uint8_t vram[SSD1306_VIRT_PAGES][SSD1306_VIRT_COLUMNS];
	uint16_t flags;
	uint8_t command_register;
	uint8_t contrast_register;
	uint8_t cs_pin;
	uint8_t di_pin;
	uint8_t spi_data;
	uint8_t reg_write_sz;
	enum ssd1306_addressing_mode_t addr_mode;

	uint8_t twi_selected;
	uint8_t twi_index;
} ssd1306_t;

typedef struct ssd1306_pin_t
{
	char port;
	uint8_t pin;
} ssd1306_pin_t;

typedef struct ssd1306_wiring_t
{
	ssd1306_pin_t chip_select;
	ssd1306_pin_t data_instruction;
	ssd1306_pin_t reset;
} ssd1306_wiring_t;

void
ssd1306_init (struct avr_t *avr, struct ssd1306_t * b, int width, int height);

static inline int
ssd1306_set_flag (ssd1306_t *b, uint16_t bit, int val)
{
	int old = b->flags & (1 << bit);
	b->flags = (b->flags & ~(1 << bit)) | (val ? (1 << bit) : 0);
	return old != 0;
}

static inline int
ssd1306_get_flag (ssd1306_t *b, uint16_t bit)
{
	return (b->flags & (1 << bit)) != 0;
}

void
ssd1306_connect (ssd1306_t * part, ssd1306_wiring_t * wiring);

void
ssd1306_connect_twi (ssd1306_t * part, ssd1306_wiring_t * wiring);

#endif
