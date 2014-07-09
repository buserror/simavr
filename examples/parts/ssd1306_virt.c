/*
	ssd1306.c

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


static void
_ssd1306_reset_cursor (ssd1306_t *part)
{
  printf (">> RESET CURSOR\n");
  part->cursor = 0;
  ssd1306_set_flag (part, SSD1306_FLAG_DIRTY, 1);
  avr_raise_irq (part->irq + IRQ_SSD1306_ADDR, part->cursor);
}

static void
_ssd1306_clear_screen (ssd1306_t *part)
{
  printf (">> CLEAR SCREEN\n");
  memset (part->vram, 0, part->h * part->pages);
  ssd1306_set_flag (part, SSD1306_FLAG_DIRTY, 1);
  avr_raise_irq (part->irq + IRQ_SSD1306_ADDR, part->cursor);
}

/*
 * current data byte is ready in b->datapins
 */
static uint32_t
ssd1306_write_data (ssd1306_t *part)
{
  printf (">> SPI DATA: 0x%02x\n", part->spi_data);
  uint32_t delay = 37; // uS TODO: How long does this take?? -- Depends on internal disp clock?!

  // TODO: Check auto cursor increment
  part->vram[part->cursor++] = part->spi_data;
  if (part->cursor > 1023)
    {
      part->cursor = 0;
    }
  printf (">> CURSOR: 0x%02x\n", part->cursor);

  ssd1306_set_flag (part, SSD1306_FLAG_DIRTY, 1);
  return delay;
}

uint8_t
ssd1306_update_command_register (ssd1306_t *part)
{
  uint8_t delay = 30;

  // One shot commands and multi-byte commands
  switch (part->spi_data)
    {
    case SSD1306_VIRT_SET_CONTRAST:
      part->command_register = part->spi_data;
      printf (">> CONTRAST SET COMMAND: 0x%02x\n", part->spi_data);
      return delay;
    case SSD1306_VIRT_DISP_NORMAL:
      ssd1306_set_flag (part, SSD1306_FLAG_DISPLAY_INVERTED, 0);
      ssd1306_set_flag (part, SSD1306_FLAG_DIRTY, 1);
      printf (">> DISPLAY NORMAL\n");
      SSD1306_CLEAR_COMMAND_REG;
      return delay;
    case SSD1306_VIRT_DISP_INVERTED:
      ssd1306_set_flag (part, SSD1306_FLAG_DISPLAY_INVERTED, 1);
      ssd1306_set_flag (part, SSD1306_FLAG_DIRTY, 1);
      printf (">> DISPLAY INVERTED\n");
      SSD1306_CLEAR_COMMAND_REG;
      return delay;
    default:
      // Unknown command
      return delay;
    }
}

uint8_t
ssd1306_update_setting (ssd1306_t *part)
{
  uint8_t delay = 30;

  // Multi-byte commands
  switch (part->command_register)
    {
    case SSD1306_VIRT_SET_CONTRAST:
      part->contrast_register = part->spi_data;
      ssd1306_set_flag (part, SSD1306_FLAG_DIRTY, 1);
      SSD1306_CLEAR_COMMAND_REG;
      printf (">> CONTRAST SET: 0x%02x\n", part->contrast_register);
      return delay;
    default:
      // Command not recognised
      return delay;
    }
}

/*
 * current command is ready in b->datapins
 */
static uint32_t
ssd1306_write_command (ssd1306_t *part)
{
  uint32_t delay = 37; // uS

  if (!part->command_register)
    {
      ssd1306_update_command_register (part);
    }
  else
    {
      ssd1306_update_setting (part);
    }
  return delay;
}


/*
 * Called when a SPI byte is sent
 */
static void
ssd1306_spi_in_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
  ssd1306_t * part = (ssd1306_t*) param;

  // Chip select should be pulled low to enable
  if (part->cs)
    return;

  part->spi_data = value & 0xFF;

  // FIXME - Don't throw away the delays here
  switch (part->di)
    {
    case SSD1306_VIRT_DATA:
      ssd1306_write_data (part);
      break;
    case SSD1306_VIRT_INSTRUCTION:
      ssd1306_write_command(part);
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
  p->cs = value & 0xFF;
  //printf (">> CHIP SELECT:  0x%02x\n", value);

}

/*
 * Called when data/instruction changes
 */
static void
ssd1306_di_hook (struct avr_irq_t * irq, uint32_t value, void * param)
{
  ssd1306_t * p = (ssd1306_t*) param;
  p->di = value & 0xFF;
  //printf (">> DATA / INSTRUCTION:  0x%08x\n", value);
}

/*
 * Called when a RESET signal is sent
 */
static void ssd1306_reset_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
  printf(">> RESET\n");
  ssd1306_t * p = (ssd1306_t*)param;
  if (irq->value && !value) {
      // Falling edge
      memset(p->vram, 0, p->h * p->pages);
      p->cursor = 0;
      p->flags = 0;
      // TODO: Check this is all
      p->command_register = 0x00;
      p->contrast_register = 0x7F;
      //
  }

}

static const char * irq_names[IRQ_SSD1306_COUNT] =
  {   //[IRQ_SSD1306_ALL] = "7=ssd1306.pins",
      [IRQ_SSD1306_SPI_BYTE_IN] = "=ssd1306.SDIN",
      [IRQ_SSD1306_RESET ] = "<ssd1306.RS",
      [IRQ_SSD1306_DATA_INSTRUCTION] = "<ssd1306.RW",
      [IRQ_SSD1306_ENABLE] = "<ssd1306.E",
      [IRQ_SSD1306_ADDR] = "7>hd44780.ADDR"
  };

void
ssd1306_connect (ssd1306_t * part)
{
  avr_connect_irq (avr_io_getirq (part->avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT),
		   part->irq + IRQ_SSD1306_SPI_BYTE_IN);
  avr_connect_irq (avr_io_getirq (part->avr, AVR_IOCTL_IOPORT_GETIRQ ('B'), 4),
		   part->irq + IRQ_SSD1306_ENABLE);
  avr_connect_irq (avr_io_getirq (part->avr, AVR_IOCTL_IOPORT_GETIRQ ('B'), 1),
		   part->irq + IRQ_SSD1306_DATA_INSTRUCTION);
  avr_connect_irq (avr_io_getirq (part->avr, AVR_IOCTL_IOPORT_GETIRQ ('B'), 5),
		   part->irq + IRQ_SSD1306_RESET);
}

void
ssd1306_init(
		struct avr_t *avr,
		struct ssd1306_t * part,
		int width,
		int height )
{
	if(!avr || !part)
	  return;

	memset(part, 0, sizeof(*part));
	part->avr = avr;
	part->w = width;
	part->h = height;
	part->pages = 8;
	part->cs = 0;
	part->di = 0;

	/*
	 * Register callbacks on all our IRQs
	 */
	part->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_SSD1306_COUNT, irq_names);
	avr_irq_register_notify(part->irq + IRQ_SSD1306_SPI_BYTE_IN, ssd1306_spi_in_hook, part);
	avr_irq_register_notify(part->irq + IRQ_SSD1306_RESET, ssd1306_reset_hook, part);
	avr_irq_register_notify(part->irq + IRQ_SSD1306_ENABLE, ssd1306_cs_hook, part);
	avr_irq_register_notify(part->irq + IRQ_SSD1306_DATA_INSTRUCTION, ssd1306_di_hook, part);

	//TODO: Add E/RD and R/W(WR) lines and check they're always low

	_ssd1306_reset_cursor(part);
	_ssd1306_clear_screen(part);

	printf("SSD1306: %duS is %d cycles for your AVR\n",
			37, (int)avr_usec_to_cycles(avr, 37));
	printf("SSD1306: %duS is %d cycles for your AVR\n",
			1, (int)avr_usec_to_cycles(avr, 1));
}

