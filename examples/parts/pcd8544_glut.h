/*
 pcd8544_glut.h

 Copyright 2017 Francisco Demartino <demartino.francisco@gmail.com>

 Based on the ssd1306 part:

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

#ifndef __PCD8544_GLUT_H__
#define __PCD8544_GLUT_H__

#include "pcd8544_virt.h"

void
pcd8544_gl_draw (pcd8544_t *part);

#endif
