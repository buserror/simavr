/*
 pcd8544_virt.c

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sim_time.h"

#include "pcd8544_virt.h"
#include "avr_spi.h"
#include "avr_ioport.h"

/*
 * Write a byte at the current cursor location and then scroll the cursor.
 */
static void
pcd8544_write_data (pcd8544_t *part)
{

	part->vram[part->cursor.page][part->cursor.column] = part->spi_data;

	// Scroll the cursor
	if (part->v == 0) {
		// Horizontal addressing mode
		if (++(part->cursor.column) >= PCD8544_VIRT_COLUMNS) {
			part->cursor.column = 0;
			if (++(part->cursor.page) >= PCD8544_VIRT_PAGES) {
				part->cursor.page = 0;
			}
		}
	} else {
		// Vertical addressing mode
		if (++(part->cursor.page) >= PCD8544_VIRT_PAGES) {
			part->cursor.page = 0;
			if (++(part->cursor.column) >= PCD8544_VIRT_COLUMNS) {
				part->cursor.column = 0;
			}
		}
	}

	pcd8544_set_flag (part, PCD8544_FLAG_DIRTY, 1);
}


static void
pcd8544_parse_command (pcd8544_t *part)
{
	if (part->spi_data == PCD8544_I_NOP) {
		printf("pcd8544: nop\n");
		return;
	}

	if (part->h == 0) {
		if (part->spi_data & PCD8544_I_SET_X_ADDR) {
			part->cursor.column = part->spi_data & PCD8544_MASK_X;
			return;
		}
		if (part->spi_data & PCD8544_I_SET_Y_ADDR) {
			part->cursor.page = part->spi_data & PCD8544_MASK_Y;
			return;
		}
		if (part->spi_data & PCD8544_I_RESERVED_2) {
			printf("pcd8544: reserved 2\n");
			return;
		}
		if (part->spi_data & PCD8544_I_RESERVED_1) {
			printf("pcd8544: reserved 1\n");
			return;
		}
	}
	if (part->h == 1) {

		if (part->spi_data & PCD8544_I_SET_VOP) {
			part->vop_register = part->spi_data & PCD8544_MASK_VOP;
			printf("pcd8544: set vop to %08x\n", part->vop_register);
			return;
		}
		if (part->spi_data & PCD8544_I_RESERVED_6) {
			printf("pcd8544: reserved 6\n");
			return;
		}
		if (part->spi_data & PCD8544_I_BIAS_SYSTEM) {
			printf("pcd8544: bias system not implemented\n");
			return;
		}
		if (part->spi_data & PCD8544_I_RESERVED_5) {
			printf("pcd8544: reserved 5\n");
			return;
		}
		if (part->spi_data & PCD8544_I_TEMP_CONTROL) {
			printf("pcd8544: temp control not implemented\n");
			return;
		}
		if (part->spi_data & PCD8544_I_RESERVED_4) {
			printf("pcd8544: reserved 4\n");
			return;
		}
		if (part->spi_data & PCD8544_I_RESERVED_3) {
			printf("pcd8544: reserved 3\n");
			return;
		}
	}

	if (part->spi_data & PCD8544_I_FUNCTION_SET) {
		printf("pcd8544: function set: %08x\n", part->spi_data);
		printf("h: %d, v: %d, pd: %d\n", part->h, part->v, part->pd);
		part->h  = (part->spi_data >> 0) & 1;
		part->v  = (part->spi_data >> 1) & 1;
		part->pd = (part->spi_data >> 2) & 1;
		printf("h: %d, v: %d, pd: %d\n", part->h, part->v, part->pd);
		return;
	}
}

/*
 * Called when a SPI byte is sent
 */
static void
pcd8544_spi_in_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
	pcd8544_t * part = (pcd8544_t*) param;

	// Chip select should be pulled low to enable
	if (part->cs_pin)
		return;

	part->spi_data = value & 0xFF;

	switch (part->dc_pin)
	{
		case PCD8544_VIRT_DATA:
			pcd8544_write_data (part);
			break;
		case PCD8544_VIRT_COMMAND:
			pcd8544_parse_command (part);
			break;
		default:
			printf ("pcd8544: WTF\n");
			// Invalid value
			break;
	}
}

/*
 * Called when chip select changes
 */
static void
pcd8544_cs_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
	pcd8544_t * p = (pcd8544_t*) param;
	p->cs_pin = value & 0xFF;
	// printf ("pcd8544: CHIP SELECT:  0x%02x\n", value);

}

/*
 * Called when data/instruction changes
 */
static void
pcd8544_di_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
	pcd8544_t * part = (pcd8544_t*) param;
	part->dc_pin = value & 0xFF;
	// printf ("pcd8544: DATA / COMMAND:  0x%08x\n", value);
}

/*
 * Called when a RESET signal is sent
 */
static void
pcd8544_reset_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
	// printf ("pcd8544: RESET\n");
	pcd8544_t * part = (pcd8544_t*) param;
	if (irq->value && !value)
	{
		// Falling edge

		// datasheet says ram is undefined, but hey
		memset (part->vram, 0, part->rows * part->pages);
		part->pd = 1;
		part->v = 0;
		part->h = 0;
		part->e = 0;
		part->d = 0;
		part->cursor.column = 0;
		part->cursor.page = 0;
		part->flags = 0;
		part->vop_register = 0x00;
	}

}

static const char * irq_names[IRQ_PCD8544_COUNT] =
{ [IRQ_PCD8544_SPI_BYTE_IN] = "=pcd8544.SDIN", [IRQ_PCD8544_RESET
                ] = "<pcd8544.RS", [IRQ_PCD8544_data_command
                ] = "<pcd8544.RW", [IRQ_PCD8544_ENABLE] = "<pcd8544.E",
                [IRQ_PCD8544_ADDR] = "7>hd44780.ADDR" };

void
pcd8544_connect (pcd8544_t * part, pcd8544_wiring_t * wiring)
{
	avr_connect_irq (
	                avr_io_getirq (part->avr, AVR_IOCTL_SPI_GETIRQ(0),
	                               SPI_IRQ_OUTPUT),
	                part->irq + IRQ_PCD8544_SPI_BYTE_IN);

	avr_connect_irq (
	                avr_io_getirq (part->avr,
	                               AVR_IOCTL_IOPORT_GETIRQ(
	                                               wiring->chip_select.port),
	                               wiring->chip_select.pin),
	                part->irq + IRQ_PCD8544_ENABLE);

	avr_connect_irq (
	                avr_io_getirq (part->avr,
	                               AVR_IOCTL_IOPORT_GETIRQ(
	                                               wiring->data_command.port),
	                               wiring->data_command.pin),
	                part->irq + IRQ_PCD8544_data_command);

	avr_connect_irq (
	                avr_io_getirq (part->avr,
	                               AVR_IOCTL_IOPORT_GETIRQ(
	                                               wiring->reset.port),
	                               wiring->reset.pin),
	                part->irq + IRQ_PCD8544_RESET);
}

void
pcd8544_init (struct avr_t *avr, struct pcd8544_t * part, int width, int height)
{
	if (!avr || !part)
		return;

	memset (part, 0, sizeof(*part));
	part->avr = avr;
	part->columns = width;
	part->rows = height;
	part->pages = height / 8; 	// 8 pixels per page

	/*
	 * Register callbacks on all our IRQs
	 */
	part->irq = avr_alloc_irq (&avr->irq_pool, 0, IRQ_PCD8544_COUNT,
	                           irq_names);

	avr_irq_register_notify (part->irq + IRQ_PCD8544_SPI_BYTE_IN,
	                         pcd8544_spi_in_hook, part);
	avr_irq_register_notify (part->irq + IRQ_PCD8544_RESET,
	                         pcd8544_reset_hook, part);
	avr_irq_register_notify (part->irq + IRQ_PCD8544_ENABLE,
	                         pcd8544_cs_hook, part);
	avr_irq_register_notify (part->irq + IRQ_PCD8544_data_command,
	                         pcd8544_di_hook, part);

	printf ("pcd8544: %duS is %d cycles for your AVR\n", 37,
	        (int) avr_usec_to_cycles (avr, 37));
	printf ("pcd8544: %duS is %d cycles for your AVR\n", 1,
	        (int) avr_usec_to_cycles (avr, 1));
}
