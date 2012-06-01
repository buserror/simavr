/*
	c3arcball.h

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>
  	Copyright (c) 1998 Paul Rademacher
    Feb 1998, Paul Rademacher (rademach@cs.unc.edu)
    Oct 2003, Nigel Stewart - GLUI Code Cleaning

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

/*
	Arcball, as described by Ken
	Shoemake in Graphics Gems IV.
	This class takes as input mouse events (mouse down, mouse drag,
	mouse up), and creates the appropriate quaternions and 4x4 matrices
	to represent the rotation given by the mouse.

	This class is used as follows:
	- initialize [either in the constructor or with set_params()], the
	center position (x,y) of the arcball on the screen, and the radius
	- on mouse down, call mouse_down(x,y) with the mouse position
	- as the mouse is dragged, repeatedly call mouse_motion() with the
	current x and y positions.  One can optionally pass in the current
	state of the SHIFT, ALT, and CONTROL keys (passing zero if keys
	are not pressed, non-zero otherwise), which constrains
	the rotation to certain axes (X for CONTROL, Y for ALT).
	- when the mouse button is released, call mouse_up()

	Axis constraints can also be explicitly set with the
	set_constraints() function.

	The current rotation is stored in the 4x4 float matrix 'rot'.
	It is also stored in the quaternion 'q_now'.
 */

#ifndef __C3ARCBALL_H___
#define __C3ARCBALL_H___

#include "c3quaternion.h"

typedef struct c3arcball {
    int  	is_mouse_down : 1,  /* true for down, false for up */
    		is_spinning : 1,
    		constraint_x : 1,
    		constraint_y : 1,
    		zero_increment : 1;
    c3quat  q_now, q_down, q_drag, q_increment;
    c3vec2  down_pt;
    c3mat4  rot, rot_increment;
    c3mat4  *rot_ptr;

    c3vec2  center;
    c3f		radius, damp_factor;
} c3arcball, *c3arcballp;

void
c3arcball_init(
		c3arcballp a );
void
c3arcball_init_mat4(
		c3arcballp a,
		c3mat4p mtx );
void
c3arcball_init_center(
		c3arcballp a,
		const c3vec2 center,
		c3f radius );
void
c3arcball_set_params(
		c3arcballp a,
		const c3vec2 center,
		c3f radius);
c3vec3
c3arcball_mouse_to_sphere(
		c3arcballp a,
		const c3vec2 p);
c3vec3
c3arcball_constrain_vector(
		const c3vec3 vector,
		const c3vec3 axis);
void
c3arcball_mouse_down(
		c3arcballp a,
		int x,
		int y);
void
c3arcball_mouse_up(
		c3arcballp a);

void
c3arcball_mouse_motion(
		c3arcballp a,
		int x,
		int y,
		int shift,
		int ctrl,
		int alt);
void
c3arcball_set_constraints(
		c3arcballp a,
		bool _constraint_x,
		bool _constraint_y);
void
c3arcball_idle(
		c3arcballp a);
void
c3arcball_set_damping(
		c3arcballp a,
		c3f d);

#endif /* __C3ARCBALL_H___ */
