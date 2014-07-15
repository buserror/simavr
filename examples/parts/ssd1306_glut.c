/*
	ssd1306_glut.c

	Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>

	Based on the hd44780 by:
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

#include "ssd1306_glut.h"

#if __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

float pix_size_g  = 1.0;
float pix_gap_g = 0.0;

void
ssd1306_gl_init (float pix_size)
{
  pix_size_g = pix_size;
  //See: http://www.opengl.org/sdk/docs/man/
}

void
ssd1306_gl_put_pixel_column(uint8_t block_pixel_column, float pixel_opacity, int invert)
{
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBegin (GL_QUADS);

  // TODO Clean up
  if (invert) {
      glColor4f (0.0f, 0.0f, 0.0f, 1.0f);
  } else {
      glColor4f (1.0f, 1.0f, 1.0f, pixel_opacity);
  }

  for (int i = 0; i < 8; ++i)
    {
      if (block_pixel_column & (1 << i))
	{
	  //glColor4f (1.0, 1.0, 1.0, pixel_opacity);
	  glVertex2f (pix_size_g, pix_size_g * (i + 1));
	  glVertex2f (0, pix_size_g * (i + 1));
	  glVertex2f (0, pix_size_g * i);
	  glVertex2f (pix_size_g, pix_size_g * i);
	}
    }
  glEnd ();
}

float
ssd1306_get_pixel_opacity(uint8_t contrast)
{
  // Typically the screen will be clearly visible even at 0 contrast
  return contrast / 512.0 + 0.5;
}

void
ssd1306_gl_draw (ssd1306_t *part)
{
  ssd1306_set_flag(part, SSD1306_FLAG_DIRTY, 0);

  int columns = part->w;
  int pages = part->pages;
  int rows = part->h;

  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBegin (GL_QUADS);

  // Draw background
  // TODO Clean up
  float opacity = ssd1306_get_pixel_opacity (part->contrast_register);
  int invert = ssd1306_get_flag(part, SSD1306_FLAG_DISPLAY_INVERTED);
  if (invert) {
      glColor4f (1.0f, 1.0f, 1.0f, opacity);
  } else {
      glColor4f (0.0f, 0.0f, 0.0f, 1.0f);
  }

  glTranslatef (0, 0 , 0);
  glBegin (GL_QUADS);
  glVertex2f (rows, 0);
  glVertex2f (0,0);
  glVertex2f (0, columns);
  glVertex2f (rows, columns);
  glEnd ();

  // Don't draw pixels if display is off
  if (!ssd1306_get_flag (part, SSD1306_FLAG_DISPLAY_ON))
    return;

  // Draw pixels
  uint16_t buf_index = 0;
  for (int p = 0; p < pages; p++)
    {
      glPushMatrix ();
      for (int c = 0; c < columns; c++)
	{
	  ssd1306_gl_put_pixel_column (
	      part->vram[buf_index++],
	      opacity, invert);
	  // Next column
	  glTranslatef (pix_size_g + pix_gap_g, 0, 0);
	}
      glPopMatrix ();
      // Next page
      glTranslatef (0, (rows/pages)*pix_size_g + pix_gap_g, 0);
    }
}
