/*
 pcd8544_glut.c

 Copyright 2017 Francisco Demartino <demartino.francisco@gmail.com>

 Based on the ssd1306 part:

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
#include "pcd8544_glut.h"

#if __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

float pix_size_g = 0.9;
float pix_gap_g = 0.1;

float pcd8544_color_foreground[3] = { 0.0f, 0.0f, 0.0f }; // black
float pcd8544_color_background[3] = { 0.52f, 0.6f, 0.43f }; // LCD Greenish


void
pcd8544_gl_set_colour (uint8_t invert, float opacity)
{
	if (invert)
	{
		glColor4f (pcd8544_color_foreground[0],
		           pcd8544_color_foreground[1],
		           pcd8544_color_foreground[2],
		           opacity);
	} else
	{
		glColor4f (pcd8544_color_background[0],
		           pcd8544_color_background[1],
		           pcd8544_color_background[2],
		           opacity);
	}
}

float
pcd8544_gl_get_pixel_opacity (uint8_t contrast)
{
	// Typically the screen will be clearly visible even at 0 contrast
	return contrast / 128.0 + 0.5;
}

void
pcd8544_gl_put_pixel_column (uint8_t block_pixel_column, float pixel_opacity,
                             int invert)
{
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin (GL_QUADS);

	pcd8544_gl_set_colour (!invert, pixel_opacity);

	for (int i = 0; i < 8; ++i)
	{
		if (block_pixel_column & (1 << i))
		{
			glVertex2f (0, (pix_size_g + pix_gap_g) * i);
			glVertex2f (0, (pix_size_g + pix_gap_g) * i + pix_size_g);
			glVertex2f (pix_size_g, (pix_size_g + pix_gap_g) * i + pix_size_g);
			glVertex2f (pix_size_g, (pix_size_g + pix_gap_g) * i);
		}
	}
	glEnd ();
}

/*
 * Controls the mapping between the VRAM and the display.
 */
static uint8_t
pcd8544_gl_get_vram_byte (pcd8544_t *part, uint8_t page, uint8_t column)
{
		return part->vram[page][column];
}

static void
pcd8544_gl_draw_pixels (pcd8544_t *part, float opacity, uint8_t invert)
{
	float x_column_step = pix_size_g + pix_gap_g;
	float y_row_step = (part->rows / part->pages) * (pix_size_g + pix_gap_g);

	for (int p = 0; p < part->pages; p++)
	{
		glPushMatrix ();
		for (int c = 0; c < part->columns; c++)
		{
			uint8_t vram_byte = pcd8544_gl_get_vram_byte (part, p, c);
			pcd8544_gl_put_pixel_column (vram_byte, opacity, invert);
			// Next column
			glTranslatef (x_column_step, 0, 0);
		}
		glPopMatrix ();
		// Next page

		glTranslatef (0, y_row_step, 0);
	}
}

void
pcd8544_gl_draw (pcd8544_t *part)
{
	pcd8544_set_flag (part, PCD8544_FLAG_DIRTY, 0);

	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin (GL_QUADS);

	// Draw background
	float opacity = pcd8544_gl_get_pixel_opacity (part->vop_register);
	int invert = part->e;
	pcd8544_gl_set_colour (invert, opacity);

	glTranslatef (0, 0, 0);
	glBegin (GL_QUADS);
	glVertex2f (0, 0);
	glVertex2f (part->columns, 0);
	glVertex2f (part->columns, part->rows);
	glVertex2f (0, part->rows);
	glEnd ();

	// Draw pixels
	if (!part->pd)
	{
		pcd8544_gl_draw_pixels (part, opacity, invert);
	}
}
