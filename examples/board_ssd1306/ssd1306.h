/*
 ssd1306.h

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

#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

// SPI pin config
#define SSD1306_RESET_PIN   		PB3
#define	SSD1306_DATA_INST   		PB1
#define	SSD1306_CHIP_SELECT		PB4

#define SSD1306_X_PIXELS 		128
#define SSD1306_Y_PIXELS 		64
#define SSD1306_PIXEL_PAGES 		(SSD1306_Y_PIXELS / 8)
#define SSD1306_PIXEL_BYTES		(SSD1306_X_PIXELS * SSD1306_PIXEL_PAGES)


// Default settings
#define SSD1306_DEFAULT_CONTRAST 0x7F

// Fundamental commands
#define SSD1306_CHARGE_PUMP_REGULATOR 	0x8D
#define SSD1306_CHARGE_PUMP_ON   	0x14
#define SSD1306_SET_CONTRAST		0x81#define SSD1306_RESUME_TO_RAM_CONTENT	0xA4
#define SSD1306_IGNORE_RAM_CONTENT	0xA5
#define SSD1306_DISP_NORMAL		0xA6
#define SSD1306_DISP_INVERTED		0xA7
#define SSD1306_DISP_SLEEP 		0xAE
#define SSD1306_DISP_ON			0xAF

// Scroll commands
#define SSD1306_SCROLL_RIGHT		0x26
#define SSD1306_SCROLL_LEFT		0x27
#define SSD1306_SCROLL_VERTICAL_RIGHT	0x29
#define SSD1306_SCROLL_VERTICAL_LEFT	0x2A
#define SSD1306_SCROLL_OFF		0x2E
#define SSD1306_SCROLL_ON   		0x2F
#define SSD1306_VERT_SCROLL_AREA 	0xA3

// Address setting commands
#define SSD1306_SET_COL_LO_NIBBLE	0x00
#define SSD1306_SET_COL_HI_NIBBLE	0x10
#define SSD1306_MEM_ADDRESSING 		0x20
#define SSD1306_SET_COL_ADDR		0x21
#define SSD1306_SET_PAGE_ADDR		0x22
#define SSD1306_SET_PAGE_START_ADDR	0xB0

// Hardware configuration
#define SSD1306_SET_DISP_START_LINE	0x40
#define SSD1306_SET_SEG_REMAP_0  	0xA0#define SSD1306_SET_SEG_REMAP_127	0xA1
#define SSD1306_SET_MULTIPLEX_RATIO     0xA8
#define SSD1306_SET_COM_SCAN_NORMAL	0xC0
#define SSD1306_SET_COM_SCAN_INVERTED	0xC8
#define SSD1306_SET_VERTICAL_OFFSET	0xD3
#define SSD1306_SET_WIRING_SCHEME	0xDA
#define SSD1306_SET_DISP_CLOCK		0xD5
#define SSD1306_SET_PRECHARGE_PERIOD  	0xD9
#define SSD1306_SET_VCOM_DESELECT_LEVEL 0xDB
#define SSD1306_NOP			0xE3

typedef enum
{
  DISPLAY_MODE_NORMAL, DISPLAY_MODE_INVERTED
} display_mode_t;

typedef enum
{
  POWER_STATE_SLEEP, POWER_STATE_ON
} power_state_t;

typedef enum
{
  PIXEL_STATE_OFF, PIXEL_STATE_ON
} pixel_state_t;

typedef enum
{
  DISP_ORIENT_NORMAL,
  DISP_ORIENT_NORMAL_MIRRORED,
  DISP_ORIENT_UPSIDE_DOWN,
  DISP_ORIENT_UPSIDE_DOWN_MIRRORED
} disp_orient_t;

extern uint8_t ssd1306_frame_buffer_g[SSD1306_PIXEL_PAGES][SSD1306_X_PIXELS];

void
ssd1306_write_data (const uint8_t);
void
ssd1306_write_instruction (const uint8_t);
void
ssd1306_reset_display (void);
void
ssd1306_init_display (void);
void
ssd1306_set_display_orientation (const disp_orient_t disp_orient);
void
ssd1306_set_contrast (const uint8_t contrast);
void
ssd1306_set_power_state (const power_state_t power_state);
void
ssd1306_set_display_mode (const display_mode_t display_mode);
void
ssd1306_write_byte (const uint8_t x, const uint8_t page, const uint8_t byte);
void
ssd1306_clear_screen (void);

/* Frame buffer operations */
void
ssd1306_display_fb (void);
void
ssd1306_set_pixel_fb (const uint8_t x, const uint8_t y, const pixel_state_t pixel_state);
void
ssd1306_clear_fb (void);
void
ssd1306_write_image_fb (const uint8_t * image);

#endif
