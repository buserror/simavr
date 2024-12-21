/*
 sh1106_virt.c

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

#include "sh1106_virt.h"
#include "avr_spi.h"
#include "avr_twi.h"
#include "avr_ioport.h"

/*
 * Write a byte at the current cursor location and then scroll the cursor.
 */
static void
sh1106_write_data (sh1106_t *part)
{
	part->vram[part->cursor.page][part->cursor.column] = part->spi_data;
	//printf ("SH1106: Display @%d,%d =  %02x\n", part->cursor.page, part->cursor.column, part->spi_data);
	if (++(part->cursor.column) >= SH1106_VIRT_COLUMNS)
	{
		part->cursor.column = 0;
	}

	sh1106_set_flag (part, SH1106_FLAG_DIRTY, 1);
}

/*
 * Called on the first command byte sent. For setting single
 * byte commands and initiating multi-byte commands.
 */
void
sh1106_update_command_register (sh1106_t *part)
{
	switch (part->spi_data)
	{
		case SH1106_VIRT_SET_CONTRAST:
			part->command_register = part->spi_data;
			//printf ("SH1106: CONTRAST SET COMMAND: 0x%02x\n", part->spi_data);
			return;
		case SH1106_VIRT_DISP_NORMAL:
			sh1106_set_flag (part, SH1106_FLAG_DISPLAY_INVERTED,
								0);
			sh1106_set_flag (part, SH1106_FLAG_DIRTY, 1);
			//printf ("SH1106: DISPLAY NORMAL\n");
			SH1106_CLEAR_COMMAND_REG(part);
			return;
		case SH1106_VIRT_DISP_INVERTED:
			sh1106_set_flag (part, SH1106_FLAG_DISPLAY_INVERTED,
								1);
			sh1106_set_flag (part, SH1106_FLAG_DIRTY, 1);
			//printf ("SH1106: DISPLAY INVERTED\n");
			SH1106_CLEAR_COMMAND_REG(part);
			return;
		case SH1106_VIRT_DISP_SUSPEND:
			sh1106_set_flag (part, SH1106_FLAG_DISPLAY_ON, 0);
			sh1106_set_flag (part, SH1106_FLAG_DIRTY, 1);
			//printf ("SH1106: DISPLAY SUSPENDED\n");
			SH1106_CLEAR_COMMAND_REG(part);
			return;
		case SH1106_VIRT_DISP_ON:
			sh1106_set_flag (part, SH1106_FLAG_DISPLAY_ON, 1);
			sh1106_set_flag (part, SH1106_FLAG_DIRTY, 1);
			//printf ("SH1106: DISPLAY ON\n");
			SH1106_CLEAR_COMMAND_REG(part);
			return;
		case SH1106_VIRT_SET_PAGE_START_ADDR
						... SH1106_VIRT_SET_PAGE_START_ADDR
										+ SH1106_VIRT_PAGES - 1:
			part->cursor.page = part->spi_data
							- SH1106_VIRT_SET_PAGE_START_ADDR;
			//printf ("SH1106: SET PAGE ADDRESS: 0x%02x\n", part->cursor.page);
			SH1106_CLEAR_COMMAND_REG(part);
			return;
		case SH1106_VIRT_SET_COLUMN_LOW_NIBBLE
						... SH1106_VIRT_SET_COLUMN_LOW_NIBBLE + 0xF:
			part->spi_data -= SH1106_VIRT_SET_COLUMN_LOW_NIBBLE;
			part->cursor.column = (part->cursor.column & 0xF0) | (part->spi_data & 0xF);
			//printf ("SH1106: SET COLUMN LOW NIBBLE: 0x%02x\n",part->spi_data);
			SH1106_CLEAR_COMMAND_REG(part);
			return;
		case SH1106_VIRT_SET_COLUMN_HIGH_NIBBLE
						... SH1106_VIRT_SET_COLUMN_HIGH_NIBBLE + 0xF:
			part->spi_data -= SH1106_VIRT_SET_COLUMN_HIGH_NIBBLE;
			part->cursor.column = (part->cursor.column & 0xF) | ((part->spi_data & 0xF) << 4);
			//printf ("SH1106: SET COLUMN HIGH NIBBLE: 0x%02x\n", part->spi_data);
			SH1106_CLEAR_COMMAND_REG(part);
			return;
		case SH1106_VIRT_SET_SEG_REMAP_0:
			sh1106_set_flag (part, SH1106_FLAG_SEGMENT_REMAP_0,
								1);
			//printf ("SH1106: SET COLUMN ADDRESS 0 TO OLED SEG0 to \n");
			SH1106_CLEAR_COMMAND_REG(part);
			return;
		case SH1106_VIRT_SET_SEG_REMAP_131:
			sh1106_set_flag (part, SH1106_FLAG_SEGMENT_REMAP_0,
								0);
			//printf ("SH1106: SET COLUMN ADDRESS 131 TO OLED SEG0 to \n");
			SH1106_CLEAR_COMMAND_REG(part);
			return;
		case SH1106_VIRT_SET_COM_SCAN_NORMAL:
			sh1106_set_flag (part, SH1106_FLAG_COM_SCAN_NORMAL,
								1);
			//printf ("SH1106: SET COM OUTPUT SCAN DIRECTION NORMAL \n");
			SH1106_CLEAR_COMMAND_REG(part);
			return;
		case SH1106_VIRT_SET_COM_SCAN_INVERTED:
			sh1106_set_flag (part, SH1106_FLAG_COM_SCAN_NORMAL,
								0);
			//printf ("SH1106: SET COM OUTPUT SCAN DIRECTION REMAPPED \n");
			SH1106_CLEAR_COMMAND_REG(part);
			return;
		case SH1106_VIRT_SET_LINE:
			//printf ("SH1106: SET DISPLAY STARt LINE to 0x%02x\n", part->spi_data & 0x3f);
			return;
		case SH1106_VIRT_SET_RATIO_OSC:
		case SH1106_VIRT_MULTIPLEX:
		case SH1106_VIRT_SET_OFFSET:
		case SH1106_VIRT_SET_PADS:
		case SH1106_VIRT_SET_CHARGE:
		case SH1106_VIRT_SET_VCOM:
			part->command_register = part->spi_data;
			//printf ("SH1106: SET COMMAND 0x%02x \n", part->spi_data);
			return;
		case SH1106_VIRT_RESUME_TO_RAM_CONTENT:
			SH1106_CLEAR_COMMAND_REG(part);
			return;
		default:
			printf ("SH1106: WARNING: unknown/not implemented command %x\n", part->spi_data);
			// Unknown command
			return;
	}
}

/*
 * Multi-byte command setting
 */
void
sh1106_update_setting (sh1106_t *part)
{
	switch (part->command_register)
	{
		case SH1106_VIRT_SET_CONTRAST:
			part->contrast_register = part->spi_data;
			sh1106_set_flag (part, SH1106_FLAG_DIRTY, 1);
			SH1106_CLEAR_COMMAND_REG(part);
			//printf ("SH1106: CONTRAST SET: 0x%02x\n", part->contrast_register);
			return;
		case SH1106_VIRT_SET_RATIO_OSC:
		case SH1106_VIRT_MULTIPLEX:
		case SH1106_VIRT_SET_OFFSET:
		case SH1106_VIRT_SET_PADS:
		case SH1106_VIRT_SET_CHARGE:
		case SH1106_VIRT_SET_VCOM:
			SH1106_CLEAR_COMMAND_REG(part);
			//printf ("SH1106: SET DATA 0x%02x \n", part->spi_data);
			return;
		default:
			// Unknown command
			printf("SH1106: error: unknown update command %x\n",part->command_register);
			return;
	}
}

/*
 * Determines whether a new command has been sent, or
 * whether we are in the process of setting a multi-
 * byte command.
 */
static void
sh1106_write_command (sh1106_t *part)
{
	if (!part->command_register)
	{
		// Single byte or start of multi-byte command
		sh1106_update_command_register (part);
	} else
	{
		// Multi-byte command setting
		sh1106_update_setting (part);
	}
}

/*
 * Called when a TWI byte is sent
 */
static void
sh1106_twi_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
	sh1106_t * p = (sh1106_t*) param;
	avr_twi_msg_irq_t v;
	v.u.v = value;

	if (v.u.twi.msg & TWI_COND_STOP)
		p->twi_selected = 0;

	if (v.u.twi.msg & TWI_COND_START) {
		p->twi_selected = 0;
		p->twi_index = 0;
		if (((v.u.twi.addr>>1) & SH1106_I2C_ADDRESS_MASK) == SH1106_I2C_ADDRESS) {
			p->twi_selected = v.u.twi.addr;
			avr_raise_irq(p->irq + IRQ_SH1106_TWI_IN,
					avr_twi_irq_msg(TWI_COND_ACK, p->twi_selected, 1));
		}
	}

	if (p->twi_selected) {
		if (v.u.twi.msg & TWI_COND_WRITE) {
			avr_raise_irq(p->irq + IRQ_SH1106_TWI_IN,
					avr_twi_irq_msg(TWI_COND_ACK, p->twi_selected, 1));

			if (p->twi_index == 0) { // control byte
				if ((v.u.twi.data & (~(1<<6))) != 0) {
					printf("%s COND_WRITE %x\n", __FUNCTION__, v.u.twi.data);
					printf("%s ALERT: unhandled Co bit\n", __FUNCTION__);
					abort();
				}
				p->di_pin = v.u.twi.data ? SH1106_VIRT_DATA : SH1106_VIRT_INSTRUCTION;
			} else {
				p->spi_data = v.u.twi.data;

				switch (p->di_pin)
				{
					case SH1106_VIRT_DATA:
						sh1106_write_data (p);
						break;
					case SH1106_VIRT_INSTRUCTION:
						sh1106_write_command (p);
						break;
					default:
						// Invalid value
						break;
				}
			}
			p->twi_index++;
		}

		// SH1106 doesn't support read on serial interfaces
		// just return 0
		if (v.u.twi.msg & TWI_COND_READ) {
			uint8_t data = 0;
			avr_raise_irq(p->irq + IRQ_SH1106_TWI_IN,
					avr_twi_irq_msg(TWI_COND_READ, p->twi_selected, data));
			p->twi_index++;
			printf("%s ALERT: SH1106 doesn't support read on I2C interface\n", __FUNCTION__);
		}
	}
}

/*
 * Called when a SPI byte is sent
 */
static void
sh1106_spi_in_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
	sh1106_t * part = (sh1106_t*) param;

	// Chip select should be pulled low to enable
	if (part->cs_pin)
		return;

	part->spi_data = value & 0xFF;

	switch (part->di_pin)
	{
		case SH1106_VIRT_DATA:
			sh1106_write_data (part);
			break;
		case SH1106_VIRT_INSTRUCTION:
			sh1106_write_command (part);
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
sh1106_cs_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
	sh1106_t * p = (sh1106_t*) param;
	p->cs_pin = value & 0xFF;
	//printf ("SH1106: CHIP SELECT:  0x%02x\n", value);

}

/*
 * Called when data/instruction changes
 */
static void
sh1106_di_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
	sh1106_t * part = (sh1106_t*) param;
	part->di_pin = value & 0xFF;
	//printf ("SH1106: DATA / INSTRUCTION:  0x%08x\n", value);
}

/*
 * Called when a RESET signal is sent
 */
static void
sh1106_reset_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
	//printf ("SH1106: RESET\n");
	sh1106_t * part = (sh1106_t*) param;
	if (irq->value && !value)
	{
		// Falling edge
		memset (part->vram, 0, part->rows * part->pages);
		part->cursor.column = 0x80;
		part->cursor.page = 0;
		part->flags = 0;
		part->command_register = 0x00;
		part->contrast_register = 0x80;
		sh1106_set_flag (part, SH1106_FLAG_COM_SCAN_NORMAL, 1);
		sh1106_set_flag (part, SH1106_FLAG_SEGMENT_REMAP_0, 1);
	}

}

static const char *irq_names[IRQ_SH1106_COUNT] = {
		[IRQ_SH1106_SPI_BYTE_IN] = "=sh1106.SDIN",
		[IRQ_SH1106_RESET ] = "<sh1106.RS",
		[IRQ_SH1106_DATA_INSTRUCTION ] = "<sh1106.RW",
		[IRQ_SH1106_ENABLE] = "<sh1106.E",
		[IRQ_SH1106_ADDR] = "7>hd44780.ADDR",
		[IRQ_SH1106_TWI_OUT] = "32<sdd1306.TWI.out",
		[IRQ_SH1106_TWI_IN] = "8>sdd1306.TWI.in",
};

void
sh1106_connect (sh1106_t * part, sh1106_wiring_t * wiring)
{
	avr_connect_irq (
		avr_io_getirq (part->avr, AVR_IOCTL_SPI_GETIRQ(0),
			SPI_IRQ_OUTPUT),
			part->irq + IRQ_SH1106_SPI_BYTE_IN);

	avr_connect_irq (
		avr_io_getirq (part->avr,
			AVR_IOCTL_IOPORT_GETIRQ(
			wiring->chip_select.port),
			wiring->chip_select.pin),
			part->irq + IRQ_SH1106_ENABLE);

	avr_connect_irq (
		avr_io_getirq (part->avr,
			AVR_IOCTL_IOPORT_GETIRQ(
			wiring->data_instruction.port),
			wiring->data_instruction.pin),
			part->irq + IRQ_SH1106_DATA_INSTRUCTION);

	avr_connect_irq (
		avr_io_getirq (part->avr,
			AVR_IOCTL_IOPORT_GETIRQ(
			wiring->reset.port),
			wiring->reset.pin),
			part->irq + IRQ_SH1106_RESET);
}

void
sh1106_connect_twi (sh1106_t * part, sh1106_wiring_t * wiring)
{
	avr_connect_irq (
			avr_io_getirq (part->avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_OUTPUT),
			part->irq + IRQ_SH1106_TWI_OUT);

	avr_connect_irq (
			part->irq + IRQ_SH1106_TWI_IN,
			avr_io_getirq (part->avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT));

	if (wiring)
	{
		avr_connect_irq (
				avr_io_getirq (part->avr,
					AVR_IOCTL_IOPORT_GETIRQ(
					wiring->reset.port),
					wiring->reset.pin),
					part->irq + IRQ_SH1106_RESET);
	}
}

void
sh1106_init (struct avr_t *avr, struct sh1106_t * part, int width, int height)
{
	if (!avr || !part)
		return;

	memset (part, 0, sizeof(*part));
	part->avr = avr;
	part->columns = width;
	part->rows = height;
	part->pages = height / 8; 	// 8 pixels per page
	part->write_cursor_end.page = SH1106_VIRT_PAGES-1;
	part->write_cursor_end.column = SH1106_VIRT_COLUMNS-1;

	AVR_LOG(avr, LOG_OUTPUT, "SH1106: size %dx%d (flags=0x%04x)\n", part->columns, part->rows, part->flags);
	/*
	 * Register callbacks on all our IRQs
	 */
	part->irq = avr_alloc_irq (&avr->irq_pool, 0, IRQ_SH1106_COUNT,
	                           irq_names);

	avr_irq_register_notify (part->irq + IRQ_SH1106_SPI_BYTE_IN,
								sh1106_spi_in_hook, part);
	avr_irq_register_notify (part->irq + IRQ_SH1106_RESET,
								sh1106_reset_hook, part);
	avr_irq_register_notify (part->irq + IRQ_SH1106_ENABLE,
								sh1106_cs_hook, part);
	avr_irq_register_notify (part->irq + IRQ_SH1106_DATA_INSTRUCTION,
								sh1106_di_hook, part);
	avr_irq_register_notify (part->irq + IRQ_SH1106_TWI_OUT,
								sh1106_twi_hook, part);
}
