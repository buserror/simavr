/*
 ssd1306.c

 SSD1306 display driver (SPI mode)

 Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>

 Inspired by the work of Gabriel Anzziani.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#include <string.h>

#include <avr/pgmspace.h>
#include <avr/io.h>
#include <util/delay.h>

#include "ssd1306.h"

uint8_t ssd1306_frame_buffer_g[SSD1306_PIXEL_PAGES][SSD1306_X_PIXELS];

void
ssd1306_reset_display (void)
{
  PORTB |= (1 << SSD1306_RESET_PIN);
  _delay_us (3);
  PORTB &= ~(1 << SSD1306_RESET_PIN);
  _delay_us (3);
  PORTB |= (1 << SSD1306_RESET_PIN);
}

static inline void
ssd1306_tx_spi_byte(const uint8_t byte)
{
  SPDR = byte;
  // Wait for transmission to complete
  while (!(SPSR & (1 << SPIF)));
}

void
ssd1306_write_data (const uint8_t byte)
{
  PORTB |= (1 << SSD1306_DATA_INST);
  PORTB &= ~(1 << SSD1306_CHIP_SELECT);
  ssd1306_tx_spi_byte(byte);
  PORTB |= (1 << SSD1306_CHIP_SELECT);
}

void
ssd1306_write_instruction (const uint8_t byte)
{
  PORTB &= ~((1 << SSD1306_DATA_INST) | (1 << SSD1306_CHIP_SELECT));
  ssd1306_tx_spi_byte(byte);
  PORTB |= (1 << SSD1306_CHIP_SELECT);
}

/* Initialise display mostly as per p64 of the datasheet */
void
ssd1306_init_display (void)
{
  ssd1306_reset_display ();

  ssd1306_set_power_state (POWER_STATE_SLEEP);

  ssd1306_write_instruction (SSD1306_SET_MULTIPLEX_RATIO);
  ssd1306_write_instruction (0x3F);

  ssd1306_write_instruction (SSD1306_SET_VERTICAL_OFFSET);
  ssd1306_write_instruction (0x00);

  ssd1306_write_instruction (SSD1306_SET_DISP_START_LINE);

  ssd1306_set_display_orientation(DISP_ORIENT_NORMAL);

  ssd1306_write_instruction (SSD1306_SET_WIRING_SCHEME);
  ssd1306_write_instruction (0x12);

  ssd1306_set_contrast (SSD1306_DEFAULT_CONTRAST);

  ssd1306_write_instruction (SSD1306_RESUME_TO_RAM_CONTENT);

  ssd1306_set_display_mode (DISPLAY_MODE_NORMAL);

  // Horizontal memory addressing mode
  ssd1306_write_instruction (SSD1306_MEM_ADDRESSING);
  ssd1306_write_instruction (0x00);

  ssd1306_write_instruction (SSD1306_SET_DISP_CLOCK);
  ssd1306_write_instruction (0x80);

  ssd1306_write_instruction (SSD1306_CHARGE_PUMP_REGULATOR);
  ssd1306_write_instruction (SSD1306_CHARGE_PUMP_ON);

  ssd1306_set_power_state (POWER_STATE_ON);
}

void
ssd1306_set_display_orientation (const disp_orient_t disp_orient)
{
  switch (disp_orient)
    {
    case DISP_ORIENT_NORMAL:
      ssd1306_write_instruction (SSD1306_SET_SEG_REMAP_0);
      ssd1306_write_instruction (SSD1306_SET_COM_SCAN_NORMAL);
      break;
    case DISP_ORIENT_NORMAL_MIRRORED:
      // The display is mirrored from the upper edge
      ssd1306_write_instruction (SSD1306_SET_SEG_REMAP_0);
      ssd1306_write_instruction (SSD1306_SET_COM_SCAN_INVERTED);
      break;
    case DISP_ORIENT_UPSIDE_DOWN:
      ssd1306_write_instruction (SSD1306_SET_SEG_REMAP_127);
      ssd1306_write_instruction (SSD1306_SET_COM_SCAN_INVERTED);
      break;
    case DISP_ORIENT_UPSIDE_DOWN_MIRRORED:
      // The upside down display is mirrored from the upper edge
      ssd1306_write_instruction (SSD1306_SET_SEG_REMAP_127);
      ssd1306_write_instruction (SSD1306_SET_COM_SCAN_NORMAL);
      break;
    default:
      break;
    }
}

/* Move the cursor to the start */
static void
ssd1306_reset_cursor (void)
{
  ssd1306_write_instruction (SSD1306_SET_PAGE_START_ADDR);
  ssd1306_write_instruction (SSD1306_SET_COL_HI_NIBBLE);
  ssd1306_write_instruction (SSD1306_SET_COL_LO_NIBBLE);
}

void
ssd1306_set_contrast (const uint8_t contrast)
{
  ssd1306_write_instruction (SSD1306_SET_CONTRAST);
  ssd1306_write_instruction (contrast);
}

void
ssd1306_set_display_mode(const display_mode_t display_mode)
{
  switch (display_mode) {
    case DISPLAY_MODE_NORMAL:
      ssd1306_write_instruction (SSD1306_DISP_NORMAL);
      break;
    case DISPLAY_MODE_INVERTED:
      ssd1306_write_instruction (SSD1306_DISP_INVERTED);
      break;
    default:
      ssd1306_write_instruction (SSD1306_DISP_NORMAL);
      break;
  }
}

void
ssd1306_set_power_state (const power_state_t power_state)
{
  switch (power_state)
    {
    case POWER_STATE_ON:
      ssd1306_write_instruction (SSD1306_DISP_ON);
      break;
    case POWER_STATE_SLEEP:
      ssd1306_write_instruction (SSD1306_DISP_SLEEP);
      break;
    default:
      break;
    }
}

void
ssd1306_write_byte (const uint8_t x, const uint8_t page, const uint8_t byte)
{
  ssd1306_write_instruction (SSD1306_SET_PAGE_START_ADDR | page);
  ssd1306_write_instruction (SSD1306_SET_COL_LO_NIBBLE | (x & 0xF));
  ssd1306_write_instruction (SSD1306_SET_COL_HI_NIBBLE | (x >> 4));
  ssd1306_write_data(byte);
}

void
ssd1306_clear_screen (void)
{
  ssd1306_reset_cursor ();

  for (uint16_t byte = 0; byte < SSD1306_PIXEL_BYTES; byte++)
    {
      ssd1306_write_data (0x00);
    }
}

/*  Transfer display buffer to LCD */
void
ssd1306_display_fb (void)
{
  ssd1306_reset_cursor ();

  for (uint8_t page = 0; page < SSD1306_PIXEL_PAGES; page++)
    {
      for (uint8_t column = 0; column < SSD1306_X_PIXELS; column++)
	{
	  ssd1306_write_data (ssd1306_frame_buffer_g[page][column]);
	}
    }
}

void
ssd1306_clear_fb (void)
{
  memset(ssd1306_frame_buffer_g, 0, SSD1306_PIXEL_BYTES);
}

void
ssd1306_set_pixel_fb (const uint8_t x, const uint8_t y, const pixel_state_t pixel_state)
{
  switch (pixel_state)
    {
    case PIXEL_STATE_ON:
      ssd1306_frame_buffer_g[y / SSD1306_PIXEL_PAGES][x] |= (1 << y % SSD1306_PIXEL_PAGES);
      break;
    case PIXEL_STATE_OFF:
      ssd1306_frame_buffer_g[y / SSD1306_PIXEL_PAGES][x] &= ~(1 << y % SSD1306_PIXEL_PAGES);
      break;
    default:
      break;
    }
}

/* Writes a run length encoded image to the display buffer */
void
ssd1306_write_image_fb (const uint8_t * image)
{
  uint8_t image_byte = 0, next_image_byte, write_byte_count = 0;

  for (uint8_t page = 0; page < SSD1306_PIXEL_PAGES; page++)
    {
      for (uint8_t column = 0; column < SSD1306_X_PIXELS; column++)
	{
	  if (!write_byte_count)
	    {
	      image_byte = pgm_read_byte_near (image++);
	      next_image_byte = pgm_read_byte_near (image++);
	      if (image_byte == next_image_byte)
		{
		  write_byte_count = pgm_read_byte_near (image++);
		}
	      else
		{
		  write_byte_count = 1;
		  image--;
		}
	    }
	  write_byte_count--;
	  ssd1306_frame_buffer_g[page][column] = image_byte;
	}
    }
}
