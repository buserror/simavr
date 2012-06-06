/*
	c3arcball.c

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

 	This file is part of libc3.

	libc3 is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	libc3 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with libc3.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>
#include "c3arcball.h"


/**************************************** c3arcball_init_mat4() ****/
/* Takes as argument a c3mat4 to use instead of the internal rot  */

void
c3arcball_init_mat4(
		c3arcballp a,
		c3mat4p mtx )
{
    c3arcball_init(a);
    a->rot_ptr = mtx;
}


/**************************************** c3arcball_init_center() ****/
/* A constructor that accepts the screen center and arcball radius*/

void
c3arcball_init_center(
		c3arcballp a,
		const c3vec2 center,
		c3f radius )
{
    c3arcball_init(a);
    c3arcball_set_params(a, center, radius);
}


/************************************** c3arcball_set_params() ****/

void
c3arcball_set_params(
		c3arcballp a,
		const c3vec2 center,
		c3f radius)
{
    a->center      = center;
    a->radius      = radius;
}

/*************************************** c3arcball_init() **********/

void
c3arcball_init(
		c3arcballp a )
{
    a->center = c3vec2f( 0.0, 0.0 );
    a->radius         = 1.0;
    a->q_now          = c3quat_identity();
    a->rot_ptr		= &a->rot;
    a->rot   		= identity3D();
    a->q_increment    = c3quat_identity();
    a->rot_increment  = identity3D();
    a->is_mouse_down  = false;
    a->is_spinning    = false;
    a->damp_factor    = 0.0;
    a->zero_increment = true;
}

/*********************************** c3arcball_mouse_to_sphere() ****/

c3vec3
c3arcball_mouse_to_sphere(
		c3arcballp a,
		const c3vec2 p)
{
    c3f mag;
    c3vec2  v2 = c3vec2_divf(c3vec2_sub(p, a->center), a->radius);
    c3vec3  v3 = c3vec3f( v2.n[0], v2.n[1], 0.0 );

    mag = c3vec2_dot(v2, v2);

    if ( mag > 1.0 )
        v3 = c3vec3_normalize(v3);
    else
        v3.n[VZ] = (c3f) sqrt( 1.0 - mag );

    /* Now we add constraints - X takes precedence over Y */
    if ( a->constraint_x ) {
        v3 = c3arcball_constrain_vector( v3, c3vec3f( 1.0, 0.0, 0.0 ));
    } else if ( a->constraint_y ) {
    	v3 = c3arcball_constrain_vector( v3, c3vec3f( 0.0, 1.0, 0.0 ));
	}

    return v3;
}


/************************************ c3arcball_constrain_vector() ****/

c3vec3
c3arcball_constrain_vector(
		const c3vec3 vector,
		const c3vec3 axis)
{
//    return (vector - (vector * axis) * axis).normalize();
    return vector;
}

/************************************ c3arcball_mouse_down() **********/

void
c3arcball_mouse_down(
		c3arcballp a,
		int x,
		int y)
{
    a->down_pt = c3vec2f( (c3f)x, (c3f) y );
    a->is_mouse_down = true;

    a->q_increment   = c3quat_identity();
    a->rot_increment = identity3D();
    a->zero_increment = true;
}


/************************************ c3arcball_mouse_up() **********/

void
c3arcball_mouse_up(
		c3arcballp a)
{
    a->q_now = c3quat_mul(a->q_drag, a->q_now);
    a->is_mouse_down = false;
}


/********************************** c3arcball_mouse_motion() **********/

void
c3arcball_mouse_motion(
		c3arcballp a,
		int x,
		int y,
		int shift,
		int ctrl,
		int alt)
{
    /* Set the X constraint if CONTROL key is pressed, Y if ALT key */
	c3arcball_set_constraints(a, ctrl != 0, alt != 0 );

    c3vec2 new_pt = c3vec2f( (c3f)x, (c3f) y );
    c3vec3 v0 = c3arcball_mouse_to_sphere(a, a->down_pt );
    c3vec3 v1 = c3arcball_mouse_to_sphere(a, new_pt );

    c3vec3 cross = c3vec3_cross(v0, v1);

    a->q_drag = c3quat_vec3(cross, c3vec3_dot(v0, v1));

    //    *rot_ptr = (q_drag * q_now).to_mat4();
    c3mat4 temp = c3quat_to_mat4(a->q_drag);
    *a->rot_ptr = c3mat4_mul(a->rot_ptr, &temp);

    a->down_pt = new_pt;

    /* We keep a copy of the current incremental rotation (= q_drag) */
    a->q_increment   = a->q_drag;
    a->rot_increment = c3quat_to_mat4(a->q_increment);

    c3arcball_set_constraints(a, false, false);

	if (a->q_increment.s < .999999) {
		a->is_spinning = true;
		a->zero_increment = false;
	} else {
		a->is_spinning = false;
		a->zero_increment = true;
	}
}


/********************************** c3arcball_mouse_motion() **********/
#if 0
void
c3arcball_mouse_motion(
		c3arcballp a,
		int x,
		int y)
{
    mouse_motion(x, y, 0, 0, 0);
}
#endif

/***************************** c3arcball_set_constraints() **********/

void
c3arcball_set_constraints(
		c3arcballp a,
		bool _constraint_x,
		bool _constraint_y)
{
    a->constraint_x = _constraint_x;
    a->constraint_y = _constraint_y;
}

/***************************** c3arcball_idle() *********************/

void
c3arcball_idle(
		c3arcballp a)
{
	if (a->is_mouse_down) {
		a->is_spinning = false;
		a->zero_increment = true;
	}

	if (a->damp_factor < 1.0f)
		c3quat_scale_angle(&a->q_increment, 1.0f - a->damp_factor);

	a->rot_increment = c3quat_to_mat4(a->q_increment);

	if (a->q_increment.s >= .999999f) {
		a->is_spinning = false;
		a->zero_increment = true;
	}
}


/************************ c3arcball_set_damping() *********************/

void
c3arcball_set_damping(
		c3arcballp a,
		c3f d)
{
    a->damp_factor = d;
}



