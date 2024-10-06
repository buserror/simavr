/*
 sh1106_glut.c

 Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>

 Based on the hd44780 part:
 Copyright Luki <humbell@ethz.ch>
 Copyright 2011 Michel Pollet <buserror@gmail.com>

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
#include "sh1106_glut.h"

#if __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

sh1106_colour_t sh1106_oled_colour_g;
float sh1106_pix_size_g = 1.0;
float sh1106_pix_gap_g = 0.0;

// Keep colours in sync with enum in header.
float sh1106_colours[][3] =
{
	{ 1.0f, 1.0f, 1.0f },	// White
	{ 0.5f, 0.9f, 1.0f },	// Blue
};

void
sh1106_gl_init (float pix_size, sh1106_colour_t oled_colour)
{
	sh1106_pix_size_g = pix_size;
	sh1106_oled_colour_g = oled_colour;
}

void
sh1106_gl_set_colour (uint8_t invert, float opacity)
{
	if (invert)
	{
		glColor4f (sh1106_colours[sh1106_oled_colour_g][0],
		           sh1106_colours[sh1106_oled_colour_g][1],
		           sh1106_colours[sh1106_oled_colour_g][2],
		           opacity);
	} else
	{
		glColor4f (0.0f, 0.0f, 0.0f, 1.0f);
	}
}

float
sh1106_gl_get_pixel_opacity (uint8_t contrast)
{
	// Typically the screen will be clearly visible even at 0 contrast
	return contrast / 512.0 + 0.5;
}

uint8_t
sh1106_gl_reverse_byte (uint8_t byte)
{
	byte = (byte & 0xF0) >> 4 | (byte & 0x0F) << 4;
	byte = (byte & 0xCC) >> 2 | (byte & 0x33) << 2;
	byte = (byte & 0xAA) >> 1 | (byte & 0x55) << 1;
	return byte;
}

void
sh1106_gl_put_pixel_column (uint8_t block_pixel_column, float pixel_opacity,
                             int invert)
{
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin (GL_QUADS);

	sh1106_gl_set_colour (!invert, pixel_opacity);

	for (int i = 0; i < 8; ++i)
	{
		if (block_pixel_column & (1 << i))
		{
			glVertex2f (sh1106_pix_size_g, sh1106_pix_size_g * (i + 1));
			glVertex2f (0, sh1106_pix_size_g * (i + 1));
			glVertex2f (0, sh1106_pix_size_g * i);
			glVertex2f (sh1106_pix_size_g, sh1106_pix_size_g * i);
		}
	}
	glEnd ();
}

/*
 * Controls the mapping between the VRAM and the display.
 */
static uint8_t
sh1106_gl_get_vram_byte (sh1106_t *part, uint8_t page, uint8_t column)
{
	return part->vram[page][column];
}

static void
sh1106_gl_draw_pixels (sh1106_t *part, float opacity, uint8_t invert)
{
	for (int p = 0; p < part->pages; p++)
	{
		glPushMatrix ();
		for (int c = 0; c < part->columns; c++)
		{
			uint8_t vram_byte = sh1106_gl_get_vram_byte (part, p, c);
			sh1106_gl_put_pixel_column (vram_byte, opacity, invert);
			// Next column
			glTranslatef (sh1106_pix_size_g + sh1106_pix_gap_g, 0, 0);
		}
		glPopMatrix ();
		// Next page
		glTranslatef (0,
		              (part->rows / part->pages) * sh1106_pix_size_g + sh1106_pix_gap_g,
		              0);
	}
}

void
sh1106_gl_draw (sh1106_t *part)
{
	sh1106_set_flag (part, SH1106_FLAG_DIRTY, 0);

	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Draw background
	float opacity = sh1106_gl_get_pixel_opacity (part->contrast_register);
	int invert = sh1106_get_flag (part, SH1106_FLAG_DISPLAY_INVERTED);
	sh1106_gl_set_colour (invert, opacity);

	glTranslatef (0, 0, 0);
	glBegin (GL_QUADS);
	glVertex2f (0, part->rows*sh1106_pix_size_g);
	glVertex2f (0, 0);
	glVertex2f (part->columns*sh1106_pix_size_g, 0);
	glVertex2f (part->columns*sh1106_pix_size_g, part->rows*sh1106_pix_size_g);
	glEnd ();

	// Draw pixels
	if (sh1106_get_flag (part, SH1106_FLAG_DISPLAY_ON))
	{
		sh1106_gl_draw_pixels (part, opacity, invert);
	}
}
