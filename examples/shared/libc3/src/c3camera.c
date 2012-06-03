/*
	c3camera.c

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>
	Copyright (c) 1998 Paul Rademacher

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


#include "c3camera.h"


void
c3cam_set_distance(
		c3cam_p c,
		const c3f new_distance)
{
    if ( new_distance <= 0.0 )  /* Distance has to be positive */
        return;

    /* We find the current forward vector */
//    forward = lookat - eye;
    c->forward = c3vec3_normalize(c3vec3_sub(c->lookat, c->eye));

    /* Set distance */
    c->distance = new_distance;

    /* Find new eye point */
    c->eye = c3vec3_sub(c->lookat, c3vec3_mulf(c->forward, c->distance));
    c3cam_update(c);
}

void
c3cam_set_upv(
		c3cam_p c,
		const c3vec3 new_up)
{
    c->up = new_up;
    c3cam_update(c);
}

void
c3cam_set_upf(
		c3cam_p c,
		const c3f x,
		const c3f y,
		const c3f z)
{
	c3cam_set_upv(c, c3vec3f(x,y,z));
}

void
c3cam_set_eyev(
		c3cam_p c,
		const c3vec3 new_eye)
{
    c->eye = new_eye;
    c3cam_update(c);
}

void
c3cam_set_eyef(
		c3cam_p c,
		const c3f x,
		const c3f y,
		const c3f z)
{
	c3cam_set_eyev(c, c3vec3f(x,y,z));
}

void
c3cam_set_lookatv(
		c3cam_p c,
		const c3vec3 new_lookat)
{
    c->lookat = new_lookat;
    c3cam_update(c);
}

void
c3cam_set_lookatf(
		c3cam_p c,
		const c3f x,
		const c3f y,
		const c3f z)
{
	c3cam_set_lookatv(c, c3vec3f(x,y,z));
}


void
c3cam_roll(
		c3cam_p c,
		const c3f angle)
{
    c3mat4 rot = rotation3D(c->forward, angle );
    c->up = c3mat4_mulv3(&rot, c->up);
    c3cam_update(c);
}

void
c3cam_eye_yaw(
		c3cam_p c,
		const c3f angle)
{
    c3vec3 eye_pt = c3vec3_sub(c->eye, c->lookat); /* eye w/lookat at center */
    c3mat4 rot    = rotation3D( c->up, angle );

    eye_pt = c3mat4_mulv3(&rot, eye_pt);
    c->eye    = c3vec3_add(c->lookat, eye_pt);

    c3cam_update(c);
}

void
c3cam_eye_yaw_abs(
		c3cam_p c,
		const c3f angle,
		const c3vec3 axis)
{
    c3vec3 eye_pt = c3vec3_sub(c->eye, c->lookat); /* eye w/lookat at center */
    c3mat4 rot      = rotation3D( axis, angle );

    eye_pt = c3mat4_mulv3(&rot, eye_pt);
    c->eye    = c3vec3_add(c->lookat, eye_pt);

    c->up = c3mat4_mulv3(&rot, c->up);

    c3cam_update(c);
}


void
c3cam_eye_pitch(
		c3cam_p c,
		const c3f angle)
{
    c3vec3 eye_pt = c3vec3_sub(c->eye, c->lookat); /* eye w/lookat at center */
    c3mat4 rot    = rotation3D( c->side, angle );

    eye_pt = c3mat4_mulv3(&rot, eye_pt);
    c->eye    = c3vec3_add(c->lookat, eye_pt);

    c->up = c3mat4_mulv3(&rot, c->up);

    c3cam_update(c);
}

void
c3cam_lookat_yaw(
		c3cam_p c,
		const c3f angle)
{
    c3vec3 lookat_pt = c3vec3_sub(c->lookat, c->eye); /* lookat w/eye at center */
    c3mat4 rot = rotation3D( c->up, -angle );

    lookat_pt = c3mat4_mulv3(&rot, lookat_pt);
    c->lookat = c3vec3_add(c->eye, lookat_pt);

    c3cam_update(c);
}

void
c3cam_lookat_pitch(
		c3cam_p c,
		const c3f angle)
{
    c3vec3 lookat_pt = c3vec3_sub(c->lookat, c->eye); /* lookat w/eye at center */
    c3mat4 rot = rotation3D( c->side, -angle );

    lookat_pt = c3mat4_mulv3(&rot, lookat_pt);
    c->lookat = c3vec3_add(c->eye, lookat_pt);

    c->up = c3mat4_mulv3(&rot, c->up);

    c3cam_update(c);
}

void
c3cam_reset_up_axis(
		c3cam_p c,
		const int axis_num)
{
    c3vec3 eye_pt = c3vec3_sub(c->lookat, c->eye); /* eye w/lookat at center */
    c3f eye_distance = c3vec3_length(eye_pt);
    c->eye.n[axis_num] = c->lookat.n[axis_num];
    /* Bring eye to same level as lookat */

    c3vec3 vector = c3vec3_sub(c->eye, c->lookat);
    vector = c3vec3_normalize(vector);
    vector = c3vec3_mulf(vector, eye_distance);

    c->eye = c3vec3_add(c->lookat, vector);
    c->up = c3vec3f( 0.0, 0.0, 0.0 );
    c->up.n[axis_num] = 1.0;

    c3cam_update(c);
}

void
c3cam_reset_up(
		c3cam_p c)
{
	c3cam_reset_up_axis(c, VY ); /* Resets to the Y axis */
}

void
c3cam_movef(
		c3cam_p c,
		const c3f side_move,
		const c3f up_move,
		const c3f forw_move)
{
    c->eye    = c3vec3_add(c->eye, c3vec3_mulf(c->forward,		forw_move));
    c->eye    = c3vec3_add(c->eye, c3vec3_mulf(c->side,			side_move));
    c->eye    = c3vec3_add(c->eye, c3vec3_mulf(c->up,			up_move));
    c->lookat = c3vec3_add(c->lookat, c3vec3_mulf(c->forward,	forw_move));
    c->lookat = c3vec3_add(c->lookat, c3vec3_mulf(c->side,		side_move));
    c->lookat = c3vec3_add(c->lookat, c3vec3_mulf(c->up,		up_move));
    c3cam_update(c);
}

void
c3cam_movev(
		c3cam_p c,
		const c3vec3 v) /* A vector version of the above command */
{
	c3cam_movef(c, v.n[VX], v.n[VY], v.n[VZ] );
}

void
c3cam_move_by_eye(
		c3cam_p c,
		const c3vec3 new_eye)
{
    c3vec3 diff = c3vec3_sub(new_eye, c->eye);

    c->lookat = c3vec3_add(c->lookat, diff);
    c->eye    = c3vec3_add(c->eye, diff);

    c3cam_update(c);
}

void
c3cam_move_by_lookat(
		c3cam_p c,
		const c3vec3 new_lookat)
{
    c3vec3 diff = c3vec3_sub(new_lookat, c->lookat);

    c->lookat = c3vec3_add(c->lookat, diff);
    c->eye    = c3vec3_add(c->eye, diff);

    c3cam_update(c);
}

void
c3cam_move_abs(
		c3cam_p c,
		const c3vec3 v)
{
    c->lookat = c3vec3_add(c->lookat, v);
    c->eye    = c3vec3_add(c->eye, v);

    c3cam_update(c);
}

void
c3cam_rot_about_eye(
		c3cam_p c,
		const c3mat4p rot)
{
    c3vec3  view = c3vec3_sub(c->lookat, c->eye);

    view = c3mat4_mulv3(rot, view);
    c->up   = c3mat4_mulv3(rot, c->up);

    c->lookat = c3vec3_add(c->eye, view);

    c3cam_update(c);
}

void
c3cam_rot_about_lookat(
		c3cam_p c,
		const c3mat4p rot)
{
    // NOT QUITE RIGHT YET

    c3vec3 view = c3vec3_sub(c->eye, c->lookat);

    view = c3mat4_mulv3(rot, view);
    c->up   = c3mat4_mulv3(rot, c->up);

    c->eye = c3vec3_add(c->lookat, view);

    c3cam_update(c);
}

void
c3cam_update_matrix(
		c3cam_p c)
{
    c3cam_update(c);

    c->mtx = c3mat4_vec4(
    		c3vec4f(c->side.n[VX], 	c->up.n[VX],	c->forward.n[VX],	0.0),
    		c3vec4f(c->side.n[VY], 	c->up.n[VY],	c->forward.n[VY],	0.0),
    		c3vec4f(c->side.n[VZ], 	c->up.n[VZ],	c->forward.n[VZ],	0.0),
    		c3vec4f(0.0, 			0.0, 			0.0, 				1.0));
}
#if 0
void
c3cam_load_to_openGL(c3cam_p c)
{
    c3mat4  m;

    make_mtx();

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    glMultMatrixf( (c3f*) &mtx[0][0]);
    glTranslatef( -eye[VX], -eye[VY], -eye[VZ] );
}

void
c3cam_load_to_openGL_noident(c3cam_p c)
{
    c3mat4  m;

    make_mtx();

    glMatrixMode( GL_MODELVIEW );
    glMultMatrixf( (c3f*) &mtx[0][0]);
    glTranslatef( -eye[VX], -eye[VY], -eye[VZ] );
}
#endif

void
c3cam_reset(
		c3cam_p c)
{
	memset(c, 0, sizeof(*c));
    c->up = c3vec3f( 0.0, 1.0, 0.0 );
    c->eye = c3vec3f(0.0, 0.0, 10.0);
    c->lookat = c3vec3f(0.0,0.0,0.0);

    c->mtx = identity3D();

    c3cam_update(c);
}

c3cam_t
c3cam_new()
{
	c3cam_t c;
	c3cam_reset(&c);
	return c;
}

void
c3cam_init(
		c3cam_p c)
{
	c3cam_reset(&c);
}

void
c3cam_update(
		c3cam_p c)
{
	/* get proper side and forward vectors, and distance  */
	c->forward = c3vec3_minus(c3vec3_sub(c->lookat, c->eye));
	c->distance = c3vec3_length(c->forward);
	c->forward = c3vec3_divf(c->forward, c->distance);

	c->side = c3vec3_cross(c->up, c->forward);
	c->up = c3vec3_cross(c->forward, c->side);

	c->forward = c3vec3_normalize(c->forward);
	c->up = c3vec3_normalize(c->up);
	c->side = c3vec3_normalize(c->side);
}

# if 0
void
c3cam_dump(c3cam_p c, FILE *output) const
{
    fprintf( output, "Viewmodel: \n" );
    eye.print(    output, "  eye"    );
    lookat.print( output, "  lookat" );
    up.print(     output, "  up"     );
    side.print(   output, "  side"   );
    forward.print(output, "  forward");
    mtx.print(    output, "  mtx"    );
}
#endif
