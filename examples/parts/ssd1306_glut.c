/*
	ssd1306_glut.c

	Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>
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

static inline void
ssd1306_glColor32U (uint32_t color)
{
  glColor4f ((float) ((color >> 24) & 0xff) / 255.0f,
	     (float) ((color >> 16) & 0xff) / 255.0f,
	     (float) ((color >> 8) & 0xff) / 255.0f,
	     (float) ((color) & 0xff) / 255.0f);
}

void
ssd1306_gl_put_pixel_column(uint8_t block_pixel_column, uint32_t pixel_color)
{
  glEnable (GL_BLEND);
  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glBegin (GL_QUADS);
  ssd1306_glColor32U (pixel_color);
  //block_pixel_column = 0xAA;
  //printf("Data Byte: %i\n", block_pixel_column);
  for (int i = 0; i < 8; ++i)
    {
      if (block_pixel_column & (1 << i))
	{
	  glVertex2f (pix_size_g, pix_size_g * (i + 1));
	  glVertex2f (0, pix_size_g * (i + 1));
	  glVertex2f (0, pix_size_g * i);
	  glVertex2f (pix_size_g, pix_size_g * i);
	}
    }
  glEnd ();
}

void
ssd1306_gl_draw (ssd1306_t *b, uint32_t background_color, uint32_t pixel_color)
{
  int columns = b->w;
  int pages = b->pages;
  int rows = b->h;

  // Draw the background
  glDisable (GL_TEXTURE_2D);
  glDisable (GL_BLEND);
  ssd1306_glColor32U (background_color);
  glTranslatef (0, 0 , 0);
  glBegin (GL_QUADS);
  glVertex2f (rows, 0);
  glVertex2f (0,0);
  glVertex2f (0, columns);
  glVertex2f (rows, columns);
  glEnd ();
  glColor3f (1.0f, 1.0f, 1.0f);

  uint16_t buf_index = 0;
  for (int p = 0; p < pages; p++)
    {
      glPushMatrix ();
      for (int c = 0; c < columns; c++)
	{
	  ssd1306_gl_put_pixel_column(b->vram[buf_index++], pixel_color);
	  // Next column
	  glTranslatef (pix_size_g + pix_gap_g, 0, 0);
	}
      glPopMatrix ();
      // Next page
      glTranslatef (0, (rows/pages)*pix_size_g + pix_gap_g, 0);
    }

  ssd1306_set_flag(b, SSD1306_FLAG_DIRTY, 0);
}
