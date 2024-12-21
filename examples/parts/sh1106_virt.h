/*
 sh1106_virt.h

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
 * This "Part" simulates the SH1106 OLED display driver.
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
 */

#ifndef __SH1106_VIRT_H__
#define __SH1106_VIRT_H__

#include "sim_irq.h"

#define SH1106_VIRT_DATA			1
#define SH1106_VIRT_INSTRUCTION 		0

#define SH1106_I2C_ADDRESS			0x3C
#define SH1106_I2C_ADDRESS_MASK			0xfe

#define SH1106_VIRT_PAGES			8
#define SH1106_VIRT_COLUMNS			132

/* Address setting commands */
#define SH1106_VIRT_SET_COLUMN_LOW_NIBBLE	0x00 
#define SH1106_VIRT_SET_COLUMN_HIGH_NIBBLE	0x10
#define SH1106_VIRT_SET_PAGE_START_ADDR		0xB0

/* Charge pump command table */
#define SH1106_VIRT_CHARGE_PUMP_VOLTAGE		0x30

/* Hardware config. commands */
#define SH1106_VIRT_SET_LINE			0x40
#define SH1106_VIRT_SET_SEG_REMAP_0  		0xA0
#define SH1106_VIRT_SET_SEG_REMAP_131		0xA1
#define SH1106_VIRT_SET_COM_SCAN_NORMAL		0xC0
#define SH1106_VIRT_SET_COM_SCAN_INVERTED	0xC8
#define SH1106_VIRT_SET_OFFSET			0xD3
#define SH1106_VIRT_SET_PADS    		0xDA

/* Fundamental commands. */
#define SH1106_VIRT_SET_CONTRAST		0x81
#define SH1106_VIRT_RESUME_TO_RAM_CONTENT	0xA4
#define SH1106_VIRT_IGNORE_RAM_CONTENT		0xA5
#define SH1106_VIRT_DISP_NORMAL			0xA6
#define SH1106_VIRT_DISP_INVERTED		0xA7
#define SH1106_VIRT_MULTIPLEX       		0xA8
#define SH1106_VIRT_DISP_SUSPEND		0xAE
#define SH1106_VIRT_DISP_ON			0xAF

/* DCDC command table */
#define SH1106_VIRT_DCDC_CONTROL_MODE		0xAD
#define SH1106_VIRT_DCDC_OFF			0x8A
#define SH1106_VIRT_DCDC_ON			0x8B

/* Timing & driving scheme setting commands */
#define SH1106_VIRT_SET_RATIO_OSC		0xD5
#define SH1106_VIRT_SET_CHARGE  		0xD9
#define SH1106_VIRT_SET_VCOM    		0xDB
#define SH1106_VIRT_NOP     			0xE3

/* Read-Modify-Write commands */
#define SH1106_VIRT_READ_MODIFY_WRITE_START  	0xE0
#define SH1106_VIRT_READ_MODIFY_WRITE_END 	0xEE

#define SH1106_CLEAR_COMMAND_REG(part)		part->command_register = 0x00

enum
{
	//IRQ_SH1106_ALL = 0,
	IRQ_SH1106_SPI_BYTE_IN,
	IRQ_SH1106_ENABLE,
	IRQ_SH1106_RESET,
	IRQ_SH1106_DATA_INSTRUCTION,
	//IRQ_SH1106_INPUT_COUNT,
	IRQ_SH1106_ADDR,		// << For VCD
	IRQ_SH1106_TWI_IN,
	IRQ_SH1106_TWI_OUT,
	IRQ_SH1106_COUNT
//TODO: Add IRQs for VCD: Internal state etc.
};

enum
{
	SH1106_FLAG_DISPLAY_INVERTED = 0,
	SH1106_FLAG_DISPLAY_ON,
	SH1106_FLAG_SEGMENT_REMAP_0,
	SH1106_FLAG_COM_SCAN_NORMAL,

	/*
	 * Internal flags, not SH1106
	 */
	SH1106_FLAG_BUSY,		// 1: Busy between instruction, 0: ready
	SH1106_FLAG_REENTRANT,		// 1: Do not update pins
	SH1106_FLAG_DIRTY,		// 1: Needs redisplay...
};

/*
 * Cursor position in VRAM
 */
struct sh1106_virt_cursor_t
{
	uint8_t page;
	uint8_t column;
};

typedef struct sh1106_t
{
	avr_irq_t * irq;
	struct avr_t * avr;
	uint8_t columns, rows, pages;
	struct sh1106_virt_cursor_t cursor, write_cursor_start, write_cursor_end;
	uint8_t vram[SH1106_VIRT_PAGES][SH1106_VIRT_COLUMNS];
	uint16_t flags;
	uint8_t command_register;
	uint8_t contrast_register;
	uint8_t cs_pin;
	uint8_t di_pin;
	uint8_t spi_data;

	uint8_t twi_selected;
	uint8_t twi_index;
} sh1106_t;

typedef struct sh1106_pin_t
{
	char port;
	uint8_t pin;
} sh1106_pin_t;

typedef struct sh1106_wiring_t
{
	sh1106_pin_t chip_select;
	sh1106_pin_t data_instruction;
	sh1106_pin_t reset;
} sh1106_wiring_t;

void
sh1106_init (struct avr_t *avr, struct sh1106_t * b, int width, int height);

static inline int
sh1106_set_flag (sh1106_t *b, uint16_t bit, int val)
{
	int old = b->flags & (1 << bit);
	b->flags = (b->flags & ~(1 << bit)) | (val ? (1 << bit) : 0);
	return old != 0;
}

static inline int
sh1106_get_flag (sh1106_t *b, uint16_t bit)
{
	return (b->flags & (1 << bit)) != 0;
}

void
sh1106_connect (sh1106_t * part, sh1106_wiring_t * wiring);

void
sh1106_connect_twi (sh1106_t * part, sh1106_wiring_t * wiring);

#endif
