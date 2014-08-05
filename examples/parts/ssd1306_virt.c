/*
 ssd1306_virt.c

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

#include "ssd1306_virt.h"
#include "avr_spi.h"
#include "avr_ioport.h"

/*
 * Write a byte at the current cursor location and then scroll the cursor.
 */
static void
ssd1306_write_data (ssd1306_t *part)
{
	part->vram[part->cursor.page][part->cursor.column] = part->spi_data;

	// Scroll the cursor
	if (++(part->cursor.column) >= SSD1306_VIRT_COLUMNS)
	{
		part->cursor.column = 0;
		if (++(part->cursor.page) >= SSD1306_VIRT_PAGES)
		{
			part->cursor.page = 0;
		}
	}

	ssd1306_set_flag (part, SSD1306_FLAG_DIRTY, 1);
}

/*
 * Called on the first command byte sent. For setting single
 * byte commands and initiating multi-byte commands.
 */
void
ssd1306_update_command_register (ssd1306_t *part)
{
	switch (part->spi_data)
	{
		case SSD1306_VIRT_SET_CONTRAST:
			part->command_register = part->spi_data;
			//printf ("SSD1306: CONTRAST SET COMMAND: 0x%02x\n", part->spi_data);
			return;
		case SSD1306_VIRT_DISP_NORMAL:
			ssd1306_set_flag (part, SSD1306_FLAG_DISPLAY_INVERTED,
			                  0);
			ssd1306_set_flag (part, SSD1306_FLAG_DIRTY, 1);
			//printf ("SSD1306: DISPLAY NORMAL\n");
			SSD1306_CLEAR_COMMAND_REG(part);
			return;
		case SSD1306_VIRT_DISP_INVERTED:
			ssd1306_set_flag (part, SSD1306_FLAG_DISPLAY_INVERTED,
			                  1);
			ssd1306_set_flag (part, SSD1306_FLAG_DIRTY, 1);
			//printf ("SSD1306: DISPLAY INVERTED\n");
			SSD1306_CLEAR_COMMAND_REG(part);
			return;
		case SSD1306_VIRT_DISP_SUSPEND:
			ssd1306_set_flag (part, SSD1306_FLAG_DISPLAY_ON, 0);
			ssd1306_set_flag (part, SSD1306_FLAG_DIRTY, 1);
			//printf ("SSD1306: DISPLAY SUSPENDED\n");
			SSD1306_CLEAR_COMMAND_REG(part);
			return;
		case SSD1306_VIRT_DISP_ON:
			ssd1306_set_flag (part, SSD1306_FLAG_DISPLAY_ON, 1);
			ssd1306_set_flag (part, SSD1306_FLAG_DIRTY, 1);
			//printf ("SSD1306: DISPLAY ON\n");
			SSD1306_CLEAR_COMMAND_REG(part);
			return;
		case SSD1306_VIRT_SET_PAGE_START_ADDR
		                ... SSD1306_VIRT_SET_PAGE_START_ADDR
		                                + SSD1306_VIRT_PAGES - 1:
			part->cursor.page = part->spi_data
			                - SSD1306_VIRT_SET_PAGE_START_ADDR;
			//printf ("SSD1306: SET PAGE ADDRESS: 0x%02x\n", part->spi_data);
			SSD1306_CLEAR_COMMAND_REG(part);
			return;
		case SSD1306_VIRT_SET_COLUMN_LOW_NIBBLE
		                ... SSD1306_VIRT_SET_COLUMN_LOW_NIBBLE + 0xF:
			part->spi_data -= SSD1306_VIRT_SET_COLUMN_LOW_NIBBLE;
			part->cursor.column = (part->cursor.column & 0xF0)
			                | (part->spi_data & 0xF);
			//printf ("SSD1306: SET COLUMN LOW NIBBLE: 0x%02x\n",part->spi_data);
			SSD1306_CLEAR_COMMAND_REG(part);
			return;
		case SSD1306_VIRT_SET_COLUMN_HIGH_NIBBLE
		                ... SSD1306_VIRT_SET_COLUMN_HIGH_NIBBLE + 0xF:
			part->spi_data -= SSD1306_VIRT_SET_COLUMN_HIGH_NIBBLE;
			part->cursor.column = (part->cursor.column & 0xF)
			                | ((part->spi_data & 0xF) << 4);
			//printf ("SSD1306: SET COLUMN HIGH NIBBLE: 0x%02x\n", part->spi_data);
			SSD1306_CLEAR_COMMAND_REG(part);
			return;
		case SSD1306_VIRT_SET_SEG_REMAP_0:
			ssd1306_set_flag (part, SSD1306_FLAG_SEGMENT_REMAP_0,
			                  1);
			//printf ("SSD1306: SET COLUMN ADDRESS 0 TO OLED SEG0 to \n");
			SSD1306_CLEAR_COMMAND_REG(part);
			return;
		case SSD1306_VIRT_SET_SEG_REMAP_127:
			ssd1306_set_flag (part, SSD1306_FLAG_SEGMENT_REMAP_0,
			                  0);
			//printf ("SSD1306: SET COLUMN ADDRESS 127 TO OLED SEG0 to \n");
			SSD1306_CLEAR_COMMAND_REG(part);
			return;
		case SSD1306_VIRT_SET_COM_SCAN_NORMAL:
			ssd1306_set_flag (part, SSD1306_FLAG_COM_SCAN_NORMAL,
			                  1);
			//printf ("SSD1306: SET COM OUTPUT SCAN DIRECTION NORMAL \n");
			SSD1306_CLEAR_COMMAND_REG(part);
			return;
		case SSD1306_VIRT_SET_COM_SCAN_INVERTED:
			ssd1306_set_flag (part, SSD1306_FLAG_COM_SCAN_NORMAL,
			                  0);
			//printf ("SSD1306: SET COM OUTPUT SCAN DIRECTION REMAPPED \n");
			SSD1306_CLEAR_COMMAND_REG(part);
			return;
		default:
			// Unknown command
			return;
	}
}

/*
 * Multi-byte command setting
 */
void
ssd1306_update_setting (ssd1306_t *part)
{
	switch (part->command_register)
	{
		case SSD1306_VIRT_SET_CONTRAST:
			part->contrast_register = part->spi_data;
			ssd1306_set_flag (part, SSD1306_FLAG_DIRTY, 1);
			SSD1306_CLEAR_COMMAND_REG(part);
			//printf ("SSD1306: CONTRAST SET: 0x%02x\n", part->contrast_register);
			return;
		default:
			// Unknown command
			return;
	}
}

/*
 * Determines whether a new command has been sent, or
 * whether we are in the process of setting a multi-
 * byte command.
 */
static void
ssd1306_write_command (ssd1306_t *part)
{
	if (!part->command_register)
	{
		// Single byte or start of multi-byte command
		ssd1306_update_command_register (part);
	} else
	{
		// Multi-byte command setting
		ssd1306_update_setting (part);
	}
}

/*
 * Called when a SPI byte is sent
 */
static void
ssd1306_spi_in_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
	ssd1306_t * part = (ssd1306_t*) param;

	// Chip select should be pulled low to enable
	if (part->cs_pin)
		return;

	part->spi_data = value & 0xFF;

	switch (part->di_pin)
	{
		case SSD1306_VIRT_DATA:
			ssd1306_write_data (part);
			break;
		case SSD1306_VIRT_INSTRUCTION:
			ssd1306_write_command (part);
			break;
		default:
			// Invalid value
			break;
	}
}

/*
 * Called when chip select changes
 */
static void
ssd1306_cs_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
	ssd1306_t * p = (ssd1306_t*) param;
	p->cs_pin = value & 0xFF;
	//printf ("SSD1306: CHIP SELECT:  0x%02x\n", value);

}

/*
 * Called when data/instruction changes
 */
static void
ssd1306_di_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
	ssd1306_t * part = (ssd1306_t*) param;
	part->di_pin = value & 0xFF;
	//printf ("SSD1306: DATA / INSTRUCTION:  0x%08x\n", value);
}

/*
 * Called when a RESET signal is sent
 */
static void
ssd1306_reset_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
	//printf ("SSD1306: RESET\n");
	ssd1306_t * part = (ssd1306_t*) param;
	if (irq->value && !value)
	{
		// Falling edge
		memset (part->vram, 0, part->rows * part->pages);
		part->cursor.column = 0;
		part->cursor.page = 0;
		part->flags = 0;
		part->command_register = 0x00;
		part->contrast_register = 0x7F;
		ssd1306_set_flag (part, SSD1306_FLAG_COM_SCAN_NORMAL, 1);
		ssd1306_set_flag (part, SSD1306_FLAG_SEGMENT_REMAP_0, 1);
	}

}

static const char * irq_names[IRQ_SSD1306_COUNT] =
{ [IRQ_SSD1306_SPI_BYTE_IN] = "=ssd1306.SDIN", [IRQ_SSD1306_RESET
                ] = "<ssd1306.RS", [IRQ_SSD1306_DATA_INSTRUCTION
                ] = "<ssd1306.RW", [IRQ_SSD1306_ENABLE] = "<ssd1306.E",
                [IRQ_SSD1306_ADDR] = "7>hd44780.ADDR" };

void
ssd1306_connect (ssd1306_t * part, ssd1306_wiring_t * wiring)
{
	avr_connect_irq (
	                avr_io_getirq (part->avr, AVR_IOCTL_SPI_GETIRQ(0),
	                               SPI_IRQ_OUTPUT),
	                part->irq + IRQ_SSD1306_SPI_BYTE_IN);

	avr_connect_irq (
	                avr_io_getirq (part->avr,
	                               AVR_IOCTL_IOPORT_GETIRQ(
	                                               wiring->chip_select.port),
	                               wiring->chip_select.pin),
	                part->irq + IRQ_SSD1306_ENABLE);

	avr_connect_irq (
	                avr_io_getirq (part->avr,
	                               AVR_IOCTL_IOPORT_GETIRQ(
	                                               wiring->data_instruction.port),
	                               wiring->data_instruction.pin),
	                part->irq + IRQ_SSD1306_DATA_INSTRUCTION);

	avr_connect_irq (
	                avr_io_getirq (part->avr,
	                               AVR_IOCTL_IOPORT_GETIRQ(
	                                               wiring->reset.port),
	                               wiring->reset.pin),
	                part->irq + IRQ_SSD1306_RESET);
}

void
ssd1306_init (struct avr_t *avr, struct ssd1306_t * part, int width, int height)
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
	part->irq = avr_alloc_irq (&avr->irq_pool, 0, IRQ_SSD1306_COUNT,
	                           irq_names);

	avr_irq_register_notify (part->irq + IRQ_SSD1306_SPI_BYTE_IN,
	                         ssd1306_spi_in_hook, part);
	avr_irq_register_notify (part->irq + IRQ_SSD1306_RESET,
	                         ssd1306_reset_hook, part);
	avr_irq_register_notify (part->irq + IRQ_SSD1306_ENABLE,
	                         ssd1306_cs_hook, part);
	avr_irq_register_notify (part->irq + IRQ_SSD1306_DATA_INSTRUCTION,
	                         ssd1306_di_hook, part);

	printf ("SSD1306: %duS is %d cycles for your AVR\n", 37,
	        (int) avr_usec_to_cycles (avr, 37));
	printf ("SSD1306: %duS is %d cycles for your AVR\n", 1,
	        (int) avr_usec_to_cycles (avr, 1));
}
