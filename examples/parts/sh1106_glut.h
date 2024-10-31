/*
 sh1106_glut.h

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

#ifndef __SH1106_GLUT_H__
#define __SH1106_GLUT_H__

#include "sh1106_virt.h"

// Keep colours in sync with array
typedef enum
{
	SH1106_GL_WHITE, SH1106_GL_BLUE
} sh1106_colour_t;

void
sh1106_gl_draw (sh1106_t *part);

void
sh1106_gl_init (float pix_size, sh1106_colour_t oled_colour);

#endif
