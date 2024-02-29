/*
	hd44780_glut.h

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
#ifndef __HD44780_GLUT_H__
#define __HD44780_GLUT_H__

#include "hd44780.h"

// This sets the change callbacks of the hd44780 to
// lock and unlock the mutex of the internal display.
void
hd44780_setup_mutex_for_gl(hd44780_t *b);

// Draws the contents of the LCD display.
// You must call hd44780_gl_init() and
// hd44780_setup_mutex_for_gl() first.
void
hd44780_gl_draw(
		hd44780_t *b,
		uint32_t background,
		uint32_t character,
		uint32_t text,
		uint32_t shadow);

void
hd44780_gl_init();

#endif
