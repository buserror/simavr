/*
 c3camera.h

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

#ifndef __C3VIEW_H___
#define __C3VIEW_H___

#include "c3algebra.h"

typedef struct c3cam {
	c3vec3 eye, lookat;
	c3vec3 up, side, forward;
	c3mat4 mtx;
	c3f distance;
} c3cam, *c3camp;

/******************************* set_distance() ***********/
/* This readjusts the distance from the eye to the lookat */
/* (changing the eye point in the process)                */
/* The lookat point is unaffected                         */
void
c3cam_set_distance(
		c3camp c,
		const c3f new_distance);

/******************************* set_up() ***************/
void
c3cam_set_upv(
		c3camp c,
		const c3vec3 new_up);
void
c3cam_set_upf(
		c3camp c,
		const c3f x,
		const c3f y,
		const c3f z);

/******************************* set_eye() ***************/
void
c3cam_set_eyev(
		c3camp c,
		const c3vec3 new_eye);
void
c3cam_set_eyef(
		c3camp c,
		const c3f x,
		const c3f y,
		const c3f z);

/******************************* set_lookat() ***************/
void
c3cam_set_lookatv(
		c3camp c,
		const c3vec3 new_lookat);
void
c3cam_set_lookatf(
		c3camp c,
		const c3f x,
		const c3f y,
		const c3f z);

/******************************* roll() *****************/
/* Rotates about the forward vector                     */
/* eye and lookat remain unchanged                      */
void
c3cam_roll(
		c3camp c,
		const c3f angle);

/******************************* eye_yaw() *********************/
/* Rotates the eye about the lookat point, using the up vector */
/* Lookat is unaffected                                        */
void
c3cam_eye_yaw(
		c3camp c,
		const c3f angle);

/******************************* eye_yaw_abs() ******************/
/* Rotates the eye about the lookat point, with a specific axis */
/* Lookat is unaffected                                         */
void
c3cam_eye_yaw_abs(
		c3camp c,
		const c3f angle,
		const c3vec3 axis);

/******************************* eye_pitch() ************/
/* Rotates the eye about the side vector                */
/* Lookat is unaffected                                 */
void
c3cam_eye_pitch(
		c3camp c,
		const c3f angle);

/******************************* lookat_yaw()************/
/* This assumes the up vector is correct.               */
/* Rotates the lookat about the side vector             */
/* Eye point is unaffected                              */
void
c3cam_lookat_yaw(
		c3camp c,
		const c3f angle);

/******************************* lookat_pitch() *********/
/* Rotates the lookat point about the side vector       */
/* This assumes the side vector is correct.             */
/* Eye point is unaffected                              */
void
c3cam_lookat_pitch(
		c3camp c,
		const c3f angle);

/******************************* reset_up() ******************/
/* Resets the up vector to a specified axis (0=X, 1=Y, 2=Z)  */
/* Also sets the eye point level with the lookat point,      */
/* along the specified axis                                  */
void
c3cam_reset_up_axis(
		c3camp c,
		const int axis_num);
void
c3cam_reset_up(
		c3camp c);

/******************************* move() ********************/
/* Moves a specified distance in the forward, side, and up */
/* directions.  This function does NOT move by world       */
/* coordinates.  To move by world coords, use the move_abs */
/* function.                                               */
void
c3cam_movef(
		c3camp c,
		const c3f side_move,
		const c3f up_move,
		const c3f forw_move);
void
c3cam_movev(
		c3camp c,
		const c3vec3 v); /* A vector version of the above command */

/******************************* move_by_eye() ***********/
/* Sets the eye point, AND moves the lookat point by the */
/* same amount as the eye is moved.                      */
void
c3cam_move_by_eye(
		c3camp c,
		const c3vec3 new_eye);

/******************************* move_by_lookat() *********/
/* Sets the lookat point, AND moves the eye point by the  */
/* same amount as the lookat is moved.                    */
void
c3cam_move_by_lookat(
		c3camp c,
		const c3vec3 new_lookat);

/******************************* move_abs() *****************/
/* Move the eye and lookat in world coordinates             */
void
c3cam_move_abs(
		c3camp c,
		const c3vec3 v);

/****************************** rot_about_eye() ************/
/* Rotates the lookat point about the eye, based on a 4x4  */
/* (pure) rotation matrix                                  */
void
c3cam_rot_about_eye(
		c3camp c,
		const c3mat4p rot);

/****************************** rot_about_lookat() ************/
/* Rotates the lookat point about the lookat, based on a 4x4  */
/* (pure) rotation matrix                                  */
void
c3cam_rot_about_lookat(
		c3camp c,
		const c3mat4p rot);

/******************************* make_mtx() *************/
/* Constructs a 4x4 matrix - used by load_to_openGL()   */
void
c3cam_update_matrix(
		c3camp c);

/******************************* load_to_openGL() ********/
/* Sets the OpenGL modelview matrix based on the current */
/* camera coordinates                                    */
//void c3cam_load_to_openGL();

/******************************* load_to_openGL_noident() ******/
/* Multiplies the current camera matrix by the existing openGL */
/* modelview matrix.  This is same as above function, but      */
/* does not set the OpenGL matrix to identity first            */
//void c3cam_load_to_openGL_noident();

/******************************* reset() ****************/
/* Resets the parameters of this class                  */
void
c3cam_reset(
		c3camp c);

/******************************* ViewModel() ************/
/* Constructor                                          */
c3cam c3cam_new();

/******************************* update() ****************/
/* updates the view params.  Call this after making      */
/* direct changes to the vectors or points of this class */
void c3cam_update(
		c3camp c);

/******************************* dump() *******************/
/* Prints the contents of this class to a file, typically */
/* stdin or stderr                                        */
//void c3cam_dump(FILE *output);

#endif /* __C3VIEW_H___ */
