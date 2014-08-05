/*
 atmega32_ssd1306.c

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

#include <string.h>
#include <avr/io.h>
#include <util/delay.h>

#undef F_CPU
#define F_CPU 7380000

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega32");

#include "ssd1306.h"
#include "images.h"

#define DEFAULT_DELAY 10000

void
spi_init (void)
{
	DDRB |= (1 << PB4) | (1 << PB5) | (1 << PB7) | (1 << PB3) | (1 << PB1);
	SPCR |= (1 << SPE) | (1 << MSTR);
	// Double the speed
	SPSR |= (1 << SPI2X);
}

void
demo_set_contrast (void)
{
	for (uint16_t contrast = 0; contrast <= 255; contrast++)
	{
		PORTB ^= (1 << PB0);
		ssd1306_set_contrast (contrast);
		_delay_ms (DEFAULT_DELAY/200);
	}
}

void
demo_show_image (void)
{
	ssd1306_write_image_fb (logo);
	ssd1306_display_fb ();
	_delay_ms (DEFAULT_DELAY);
}

/* Draw some dots by writing directly to the VRAM */
void
demo_set_byte_direct (void)
{
	ssd1306_clear_screen ();
	uint8_t x = 0;
	for (uint8_t page = 0; page < SSD1306_PIXEL_PAGES; ++page)
	{
		for (x = 0; x < SSD1306_X_PIXELS; x += 2)
		{
			ssd1306_write_byte (x, page, 0xAA);
		}
	}
	_delay_ms (DEFAULT_DELAY);
}

/* Draw some stripes by setting individual pixels on the frame buffer */
void
demo_set_pixels (void)
{
	ssd1306_clear_fb ();
	uint8_t x = 0;
	for (uint8_t y = 0; y < SSD1306_Y_PIXELS; ++y)
	{
		for (x = 0; x < SSD1306_X_PIXELS; x += 2)
		{
			ssd1306_set_pixel_fb (x, y, PIXEL_STATE_ON);
		}
	}
	ssd1306_display_fb ();
	_delay_ms (DEFAULT_DELAY);
}

void
demo_clear_screen (void)
{
	// Turn all pixels on
	memset (ssd1306_frame_buffer_g, 0xFF, SSD1306_PIXEL_BYTES);
	ssd1306_display_fb ();
	_delay_ms (DEFAULT_DELAY);

	// Clear screen
	ssd1306_clear_screen ();
	_delay_ms (DEFAULT_DELAY);
}

void
demo_set_power_state (void)
{
	ssd1306_set_power_state (POWER_STATE_SLEEP);
	_delay_ms (DEFAULT_DELAY);
	ssd1306_set_power_state (POWER_STATE_ON);
	_delay_ms (DEFAULT_DELAY);
}

void
demo_rotate_display (void)
{
	for (uint8_t i = DISP_ORIENT_NORMAL;
	                i <= DISP_ORIENT_UPSIDE_DOWN_MIRRORED; ++i)
	{
		ssd1306_set_display_orientation (i);
		ssd1306_write_image_fb (logo);
		ssd1306_display_fb ();
		_delay_ms (DEFAULT_DELAY);
	}
}

void
demo_invert_image ()
{
	ssd1306_set_display_orientation (DISP_ORIENT_NORMAL);
	for (uint8_t i = DISPLAY_MODE_NORMAL; i <= DISPLAY_MODE_INVERTED; ++i)
	{
		ssd1306_set_display_mode (i);
		ssd1306_write_image_fb (logo);
		ssd1306_display_fb ();
		// Check inverted contrast works
		demo_set_contrast ();
	}
}

int
main ()
{
	spi_init ();

	/*
	 * Demonstrate the virtual part functionality. Runs approx 10 times
	 * faster on an i7-3740QM CPU @ 2.70GHz than in real life.
	 */
	for (;;)
	{
		ssd1306_init_display ();

		demo_show_image ();
		demo_set_power_state ();
		demo_set_contrast ();
		demo_set_byte_direct ();
		demo_set_pixels ();
		demo_clear_screen ();
		demo_rotate_display ();
		demo_invert_image ();
	}

}
