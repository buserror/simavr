/*
 pcd8544_virt.h

 Copyright 2017 Francisco Demartino <demartino.francisco@gmail.com>

 Based on the ssd1306 part:

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
 * This "part" simulates the PCD8544 (Nokia 5110) LED display driver.
 */

#ifndef __PCD8544_VIRT_H__
#define __PCD8544_VIRT_H__

#include "sim_irq.h"

#define PCD8544_VIRT_DATA       1
#define PCD8544_VIRT_COMMAND    0

#define PCD8544_VIRT_PAGES     6
#define PCD8544_VIRT_COLUMNS   84


// Usable regardless of H value (works both on H==0 and H==1)
#define PCD8544_I_NOP              0x00
#define PCD8544_I_FUNCTION_SET     0x20
// Also this is the only one that needs D/~C == D.
// Everything else uses D/~C == C
#define PCD8544_I_WRITE_DATA       0x80

// Usable only when H == 0
#define PCD8544_I_RESERVED_1       0x04
#define PCD8544_I_DISPLAY_CONTROL  0x08
#define PCD8544_I_RESERVED_2       0x10
#define PCD8544_I_SET_Y_ADDR       0x40
#define PCD8544_I_SET_X_ADDR       0x80

// Usable only when H == 1 (not implemented)
#define PCD8544_I_RESERVED_3       0x01
#define PCD8544_I_RESERVED_4       0x02
#define PCD8544_I_TEMP_CONTROL     0x04
#define PCD8544_I_RESERVED_5       0x08
#define PCD8544_I_BIAS_SYSTEM      0x10
#define PCD8544_I_RESERVED_6       0x40
#define PCD8544_I_SET_VOP          0x80

// related masks
#define PCD8544_MASK_Y             0x07
#define PCD8544_MASK_X             0x7F
#define PCD8544_MASK_VOP           0x7F


enum
{
	//IRQ_PCD8544_ALL = 0,
	IRQ_PCD8544_SPI_BYTE_IN,
	IRQ_PCD8544_ENABLE,
	IRQ_PCD8544_RESET,
	IRQ_PCD8544_data_command,
	//IRQ_PCD8544_INPUT_COUNT,
	IRQ_PCD8544_ADDR,		// << For VCD
	IRQ_PCD8544_COUNT
//TODO: Add IRQs for VCD: Internal state etc.
};

enum
{
	/*
	 * Internal flags, not PCD8544
	 */
	PCD8544_FLAG_BUSY,		// 1: Busy between instruction, 0: ready
	PCD8544_FLAG_REENTRANT,		// 1: Do not update pins
	PCD8544_FLAG_DIRTY,			// 1: Needs redisplay...
};

/*
 * Cursor position in VRAM
 */
struct pcd8544_virt_cursor_t
{
	uint8_t page;
	uint8_t column;
};

typedef struct pcd8544_t
{
	avr_irq_t * irq;
	struct avr_t * avr;
	uint8_t columns, rows, pages;
	struct pcd8544_virt_cursor_t cursor;
	uint8_t vram[PCD8544_VIRT_PAGES][PCD8544_VIRT_COLUMNS];
	uint16_t flags;
	uint8_t pd;
	uint8_t v;
	uint8_t h;
	uint8_t e;
	uint8_t d;
	uint8_t vop_register;
	uint8_t cs_pin;
	uint8_t dc_pin;
	uint8_t spi_data;
} pcd8544_t;

typedef struct pcd8544_pin_t
{
	char port;
	uint8_t pin;
} pcd8544_pin_t;

typedef struct pcd8544_wiring_t
{
	pcd8544_pin_t chip_select;
	pcd8544_pin_t data_command;
	pcd8544_pin_t reset;
} pcd8544_wiring_t;

void
pcd8544_init (struct avr_t *avr, struct pcd8544_t * b, int width, int height);

static inline int
pcd8544_set_flag (pcd8544_t *b, uint16_t bit, int val)
{
	int old = b->flags & (1 << bit);
	b->flags = (b->flags & ~(1 << bit)) | (val ? (1 << bit) : 0);
	return old != 0;
}

static inline int
pcd8544_get_flag (pcd8544_t *b, uint16_t bit)
{
	return (b->flags & (1 << bit)) != 0;
}

void
pcd8544_connect (pcd8544_t * part, pcd8544_wiring_t * wiring);

#endif
