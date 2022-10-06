/*
	hd44780_glut.c

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

#include "hd44780_glut.h"

#if __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <pthread.h>
#include <stdio.h>

#include "lcd_font.h"	// generated with gimp

static GLuint font_texture;
static int charwidth = 5;
static int charheight = 7;

static pthread_mutex_t hd44780_state_mutex = PTHREAD_MUTEX_INITIALIZER;

void before_state_lock_cb(void *b)
{
	pthread_mutex_lock(&hd44780_state_mutex);
}

void after_state_lock_cb(void *b)
{
	pthread_mutex_unlock(&hd44780_state_mutex);
}

void hd44780_setup_mutex_for_gl(hd44780_t *b)
{
	b->on_state_lock = &before_state_lock_cb;
	b->on_state_lock_parameter = b;
	b->on_state_unlock = &after_state_lock_cb;
	b->on_state_unlock_parameter = b;
}

void
hd44780_gl_init()
{
	glGenTextures(1, &font_texture);
	glBindTexture(GL_TEXTURE_2D, font_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, 4,
			lcd_font.width,
			lcd_font.height, 0, GL_RGBA,
	        GL_UNSIGNED_BYTE,
	        lcd_font.pixel_data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(1.0f / (GLfloat) lcd_font.width, 1.0f / (GLfloat) lcd_font.height, 1.0f);

	glMatrixMode(GL_MODELVIEW);
}

static inline void
glColor32U(uint32_t color)
{
	glColor4f(
			(float)((color >> 24) & 0xff) / 255.0f,
			(float)((color >> 16) & 0xff) / 255.0f,
			(float)((color >> 8) & 0xff) / 255.0f,
			(float)((color) & 0xff) / 255.0f );
}

void
glputchar(char c,
		uint32_t character,
		uint32_t text,
		uint32_t shadow)
{
	int index = c;
	int left = index * charwidth;
	int right = index * charwidth + charwidth;
	int top = 0;
	int bottom = 7;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDisable(GL_TEXTURE_2D);
	glColor32U(character);
	glBegin(GL_QUADS);
	glVertex3i(5, 7, 0);
	glVertex3i(0, 7, 0);
	glVertex3i(0, 0, 0);
	glVertex3i(5, 0, 0);
	glEnd();

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, font_texture);
	if (shadow) {
		glColor32U(shadow);
		glPushMatrix();
		glTranslatef(.2f, .2f, 0);
		glBegin(GL_QUADS);
		glTexCoord2i(right, top);		glVertex3i(5, 0, 0);
		glTexCoord2i(left, top);		glVertex3i(0, 0, 0);
		glTexCoord2i(left, bottom);		glVertex3i(0, 7, 0);
		glTexCoord2i(right, bottom);	glVertex3i(5, 7, 0);
		glEnd();
		glPopMatrix();
	}
	glColor32U(text);
	glBegin(GL_QUADS);
	glTexCoord2i(right, top);		glVertex3i(5, 0, 0);
	glTexCoord2i(left, top);		glVertex3i(0, 0, 0);
	glTexCoord2i(left, bottom);		glVertex3i(0, 7, 0);
	glTexCoord2i(right, bottom);	glVertex3i(5, 7, 0);
	glEnd();

}

void
hd44780_gl_draw(
		hd44780_t *b,
		uint32_t background,
		uint32_t character,
		uint32_t text,
		uint32_t shadow)
{
	if (b->on_state_lock != &before_state_lock_cb ||
		b->on_state_unlock != &after_state_lock_cb)
	{
		printf("Error: the hd44780 instance is not using the mutex of the OpenGL thread!\nCall hd44780_setup_mutex_for_gl() first!\n");
		exit(EXIT_FAILURE);
	}

	int rows = b->w;
	int lines = b->h;
	int border = 3;

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glColor32U(background);
	glTranslatef(border, border, 0);
	glBegin(GL_QUADS);
	glVertex3f(rows * charwidth + (rows - 1) + border, -border, 0);
	glVertex3f(-border, -border, 0);
	glVertex3f(-border, lines * charheight + (lines - 1) + border, 0);
	glVertex3f(rows * charwidth + (rows - 1) + border, lines * charheight
	        + (lines - 1) + border, 0);
	glEnd();

	// create a local copy (so we can release the mutex as fast as possible)
	uint8_t vram[192];
	pthread_mutex_lock(&hd44780_state_mutex);
	// cgram is not updated yet
	// int cgram_dirty = hd44780_get_flag(b, HD44780_FLAG_CRAM_DIRTY);
	for (uint8_t i = 0; i < 192; i++)
		vram[i] = b->vram[i];
	// the values have been seen, they are not dirty anymore
	hd44780_set_flag(b, HD44780_FLAG_CRAM_DIRTY, 0);
	hd44780_set_flag(b, HD44780_FLAG_DIRTY, 0);
	pthread_mutex_unlock(&hd44780_state_mutex);

	glColor3f(1.0f, 1.0f, 1.0f);
	const uint8_t offset[] = { 0x00, 0x40, 0x00 + 20, 0x40 + 20 };
	for (int v = 0 ; v < lines; v++) {
		glPushMatrix();
		for (int i = 0; i < rows; i++) {
			glputchar(vram[offset[v] + i], character, text, shadow);
			glTranslatef(6, 0, 0);
		}
		glPopMatrix();
		glTranslatef(0, 8, 0);
	}
}
