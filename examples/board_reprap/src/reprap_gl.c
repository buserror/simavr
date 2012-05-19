/*
	reprap_gl.c

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

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

#if __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <stdio.h>

#include "reprap.h"
#include "reprap_gl.h"

#include "c3/c3.h"
#include "c3/c3camera.h"
#include "c3/c3arcball.h"

int _w = 800, _h = 600;
c3cam cam;
c3arcball arcball;
c3object_p root;
c3object_p head;
c3geometry_array_t geo_sorted = C_ARRAY_NULL;

extern reprap_t reprap;

static void
_gl_key_cb(
		unsigned char key,
		int x,
		int y)	/* called on key press */
{
	switch (key) {
		case 'q':
		//	avr_vcd_stop(&vcd_file);
			exit(0);
			break;
		case 'r':
			printf("Starting VCD trace; press 's' to stop\n");
		//	avr_vcd_start(&vcd_file);
			break;
		case 's':
			printf("Stopping VCD trace\n");
		//	avr_vcd_stop(&vcd_file);
			break;
	}
}


static void
_gl_display_cb(void)		/* function called whenever redisplay needed */
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Set up projection matrix
	glMatrixMode(GL_PROJECTION); // Select projection matrix
	glLoadIdentity(); // Start with an identity matrix

	gluPerspective(45, _w / _h, 0, 10000);

//	glEnable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);

	glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);                         // Enable Blending
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);          // Type Of Blending To Use

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
   // glMultMatrixf(arcball.rot.n);
    glMultMatrixf(cam.mtx.n);
    glTranslatef( -cam.eye.n[VX], -cam.eye.n[VY], -cam.eye.n[VZ] );
  //  glMultMatrixf(arcball.rot.n);

	c3vec3 headp = c3vec3f(
			stepper_get_position_mm(&reprap.step_x),
			stepper_get_position_mm(&reprap.step_y),
			stepper_get_position_mm(&reprap.step_z));
	c3mat4 headmove = translation3D(headp);
	c3transform_set(head->transform.e[0], &headmove);

	if (root->dirty) {
		printf("reproject\n");
		c3mat4 m = identity3D();
		c3object_project(root, &m);
		c3geometry_array_clear(&geo_sorted);
		c3object_get_geometry(root, &geo_sorted);
	}

	for (int gi = 0; gi < geo_sorted.count; gi++) {
		c3geometry_p g = geo_sorted.e[gi];
		glColor4fv(g->mat.color.n);
	    glVertexPointer(3, GL_FLOAT, 0, g->projected.count ? g->projected.e : g->vertice.e);
	    glEnableClientState(GL_VERTEX_ARRAY);

	    glDrawArrays(g->type, 0, g->vertice.count);

	    glDisableClientState(GL_VERTEX_ARRAY);

	}

	glMatrixMode(GL_PROJECTION); // Select projection matrix
	glLoadIdentity(); // Start with an identity matrix
	glOrtho(0, _w, 0, _h, 0, 10);
	glScalef(1,-1,1);
	glTranslatef(0, -1 * _h, 0);

	glMatrixMode(GL_MODELVIEW); // Select modelview matrix

	#if 0
	glPushMatrix();
	glLoadIdentity(); // Start with an identity matrix
	glScalef(3, 3, 1);
	hd44780_gl_draw(
		&hd44780,
			colors[color][0], /* background */
			colors[color][1], /* character background */
			colors[color][2], /* text */
			colors[color][3] /* shadow */ );
	glPopMatrix();
#endif
    glutSwapBuffers();
}

int button;
c3vec2 move;
c3cam startcam;

static
void _gl_button_cb(
		int b,
		int s,
		int x,
		int y)
{
	button = s == GLUT_DOWN ? b : 0;
	startcam = cam;
	move = c3vec2f(x, y);
	if (s == GLUT_DOWN)
		c3arcball_mouse_down(&arcball, x, y);
	else
		c3arcball_mouse_up(&arcball);
}

void
_gl_motion_cb(
		int x,
		int y)
{
	c3vec2 m = c3vec2f(x, y);
	c3vec2 delta = c3vec2_sub(move, m);

//	printf("%s b%d click %.1f,%.1f now %d,%d\n",
//			__func__, button, move.n[0], move.n[1], x, y);

	switch (button) {
		case GLUT_LEFT_BUTTON: {

		//	c3cam_eye_yaw(&cam, delta.n[0] / 4);
		//	c3cam_eye_pitch(&cam, delta.n[1] / 4);

			c3mat4 rotx = rotation3D(c3vec3f(1.0, 0, 0), delta.n[1] / 4);
			c3mat4 roty = rotation3D(c3vec3f(0.0, 0.0, 1.0), delta.n[0] / 4);
			rotx = c3mat4_mul(&rotx, &roty);
			c3cam_rot_about_lookat(&cam, &rotx);

		    c3cam_update_matrix(&cam);
//		    c3arcball_mouse_motion(&arcball, x, y, 0,0,0);
		}	break;
		case GLUT_RIGHT_BUTTON: {

		}	break;
	}
	move = m;
}

// gl timer. if the lcd is dirty, refresh display
static void
_gl_timer_cb(
		int i)
{
	//static int oldstate = -1;
	// restart timer
	c3arcball_idle(&arcball);
	glutTimerFunc(1000 / 24, _gl_timer_cb, 0);
	glutPostRedisplay();
}

int
gl_init(
		int argc,
		char *argv[] )
{
	glutInit(&argc, argv);		/* initialize GLUT system */

	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(_w, _h);		/* width=400pixels height=500pixels */
	/*window =*/ glutCreateWindow("Press 'q' to quit");	/* create window */

	glutDisplayFunc(_gl_display_cb);		/* set window's display callback */
	glutKeyboardFunc(_gl_key_cb);		/* set window's key callback */
	glutTimerFunc(1000 / 24, _gl_timer_cb, 0);

	glutMouseFunc(_gl_button_cb);
	glutMotionFunc(_gl_motion_cb);

	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_LINE_SMOOTH);
	cam = c3cam_new();
	cam.lookat = c3vec3f(100.0, 100.0, 0.0);
    cam.eye = c3vec3f(100.0, -100.0, 100.0);
    c3cam_update_matrix(&cam);

    c3arcball_init_center(&arcball, c3vec2f(_w/2, _h/2), 100);
//	hd44780_gl_init();

    root = c3object_new(NULL);

    c3object_p grid = c3object_new(root);
    {
    	c3geometry_p g = c3geometry_new(GL_LINES, grid);
    	g->mat.color = c3vec4f(1.0, 1.0, 1.0, 1.0);
        for (int x = 0; x < 20; x++) {
        	for (int y = 0; y < 20; y++) {
        		c3vec3 p[4] = {
        			c3vec3f(-1+x*10,y*10,0), c3vec3f(1+x*10,y*10,0),
        			c3vec3f(x*10,-1+y*10,0), c3vec3f(x*10,1+y*10,0),
        		};
        		c3vertex_array_insert(&g->vertice,
        				g->vertice.count, p, 4);
        	}
        }
    }
    head = c3object_new(root);
    c3transform_new(head);
    {
    	c3geometry_p g = c3geometry_new(GL_LINES, head);
    	g->mat.color = c3vec4f(1.0, 0.0, 0.0, 1.0);
		c3vec3 p[4] = {
			c3vec3f(-1, 0, 0), c3vec3f(1, 0, 0),
			c3vec3f(0, -1, 0), c3vec3f(0, 1, 0),
		};
        c3vertex_array_insert(&g->vertice,
        		g->vertice.count, p, 4);
    }
	return 1;
}

int
gl_runloop()
{
	glutMainLoop();
	return 0;
}
