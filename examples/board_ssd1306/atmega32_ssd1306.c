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
#include "ssd1306_utils.h"
#include "images.h"

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
  for (uint8_t contrast = 0; ;contrast++)
    {
      PORTB ^= (1 << PB0);
      ssd1306_set_contrast (contrast);
      _delay_ms (5);
    }
}

void
demo_show_image (void)
{
  ssd1306_image_to_buffer (logo);
  ssd1306_show_display ();
}

void
demo_set_pixels (void)
{
  // Draw some stripes
  uint8_t x = 0;
  for (uint8_t y = 0; y < SSD1306_Y_PIXELS; ++y)
    {
      for (x = 0; x < SSD1306_X_PIXELS; x+=2)
	{
	  ssd1306_set_pixel (x, y);
	}
    }
  ssd1306_show_display ();
}

void
demo_clear_screen (void)
{
  // Turn all pixels on
  memset(display_buffer, 0xFF, SSD1306_PIXEL_BYTES);
  ssd1306_show_display ();
  _delay_ms (1000);

  // Clear screen
  ssd1306_clear_display ();
  ssd1306_show_display ();
}

int
main ()
{
  // Setup
  spi_init ();
  ssd1306_init_display ();

  //demo_set_pixels();
  //demo_clear_screen();
  demo_show_image();
  ssd1306_set_display_mode(INVERTED);
  demo_set_contrast();

  for(;;){}


}

