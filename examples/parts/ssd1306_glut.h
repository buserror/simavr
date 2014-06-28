/*
	ssd1306_glut.h

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
#ifndef __SSD1306_GLUT_H__
#define __SSD1306_GLUT_H__

#include "hd44780.h"

void
ssd1306_gl_draw(
		hd44780_t *b,
		uint32_t background,
		uint32_t character,
		uint32_t text,
		uint32_t shadow);

void
ssd1306_gl_init();

#endif
