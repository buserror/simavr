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
#include <math.h>

#include "reprap.h"
#include "reprap_gl.h"

#include "c3/c3.h"
#include "c3/c3camera.h"
#include "c3/c3arcball.h"
#include "c3/c3driver_context.h"

int _w = 800, _h = 600;
c3cam cam;
c3arcball arcball;
c3context_p c3;
c3object_p head;

extern reprap_t reprap;

static int dumpError(const char * what)
{
	GLenum e;
	int count = 0;
	while ((e = glGetError()) != GL_NO_ERROR) {
		printf("%s: %s\n", what, gluErrorString(e));
		count++;
	}
	return count;
}

static void
_gl_key_cb(
		unsigned char key,
		int x,
		int y)	/* called on key press */
{
	switch (key) {
		case 'q':
		//	avr_vcd_stop(&vcd_file);
			c3context_dispose(c3);
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
_c3_geometry_prepare(
		c3context_p c,
		const struct c3driver_context_t *d,
		c3geometry_p g)
{
	switch(g->type.type) {
		case C3_TEXTURE_TYPE: {
			c3texture_p t = (c3texture_p)g;
			g->type.subtype = GL_TRIANGLE_FAN;
			g->mat.color = c3vec4f(0.0, 1.0, 0.0, 0.5);
			printf("_c3_geometry_prepare xrure %d!\n", g->textures.count);
			if (!g->texture) {
				GLuint texID = 0;
				dumpError("cp_gl_texture_load_argb flush");

				glEnable(GL_TEXTURE_RECTANGLE_ARB);
				dumpError("cp_gl_texture_load_argb GL_TEXTURE_RECTANGLE_ARB");

				glGenTextures(1, &texID);
				dumpError("cp_gl_texture_load_argb glBindTexture GL_TEXTURE_RECTANGLE_ARB");

				glPixelStorei(GL_UNPACK_ROW_LENGTH, t->pixels.row / 4);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

				g->mat.texture = texID;
				g->texture = 1;
			}
			glBindTexture(GL_TEXTURE_RECTANGLE_ARB, g->mat.texture);
			glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8,
					t->pixels.w, t->pixels.h, 0,
					GL_RGBA, GL_UNSIGNED_BYTE,
					t->pixels.base);
		}	break;
		default:
		    break;
	}
}

static void
_c3_geometry_draw(
		c3context_p c,
		const struct c3driver_context_t *d,
		c3geometry_p g )
{
	glColor4fv(g->mat.color.n);
	glVertexPointer(3, GL_FLOAT, 0,
			g->projected.count ? g->projected.e : g->vertice.e);
	glEnableClientState(GL_VERTEX_ARRAY);
	if (g->textures.count && g->texture) {
		glDisable(GL_TEXTURE_2D);
		glEnable(GL_TEXTURE_RECTANGLE_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, g->mat.texture);
		glTexCoordPointer(2, GL_FLOAT, 0,
				g->textures.e);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	} else
		glDisable(GL_TEXTURE_RECTANGLE_ARB);

	glDrawArrays(g->type.subtype, 0, g->vertice.count);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

const c3driver_context_t c3context_driver = {
		.geometry_prepare = _c3_geometry_prepare,
		.geometry_draw = _c3_geometry_draw,
};

/*
 * Computes the distance from the eye, sort by this value
 */
static int
_c3_z_sorter(
		const void *_p1,
		const void *_p2)
{
	c3geometry_p g1 = *(c3geometry_p*)_p1;
	c3geometry_p g2 = *(c3geometry_p*)_p2;
	// get center of bboxes
	c3vec3 c1 = c3vec3_add(g1->bbox.min, c3vec3_divf(c3vec3_sub(g1->bbox.max, g1->bbox.min), 2));
	c3vec3 c2 = c3vec3_add(g2->bbox.min, c3vec3_divf(c3vec3_sub(g2->bbox.max, g2->bbox.min), 2));

	c3f d1 = c3vec3_length2(c3vec3_sub(c1, cam.eye));
	c3f d2 = c3vec3_length2(c3vec3_sub(c2, cam.eye));

	return d1 < d2 ? 1 : d1 > d2 ? -1 : 0;
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

	if (c3->root->dirty) {
	//	printf("reproject\n");
		c3context_prepare(c3);

		qsort(c3->projected.e, c3->projected.count,
				sizeof(c3->projected.e[0]), _c3_z_sorter);
	}
	c3context_draw(c3);

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
		    c3->root->dirty = 1;	// resort the array
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

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

	glEnable(GL_LINE_SMOOTH);

	glEnable(GL_BLEND);
	// Works for the UI !!
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	cam = c3cam_new();
	cam.lookat = c3vec3f(100.0, 100.0, 0.0);
    cam.eye = c3vec3f(100.0, -100.0, 100.0);
    c3cam_update_matrix(&cam);

    c3arcball_init_center(&arcball, c3vec2f(_w/2, _h/2), 100);
//	hd44780_gl_init();

    c3 = c3context_new(_w, _h);
    static const c3driver_context_t * list[] = { &c3context_driver, NULL };
    c3->driver = list;

    c3object_p grid = c3object_new(c3->root);
    {
        for (int x = 0; x < 20; x++) {
        	for (int y = 0; y < 20; y++) {
        		c3vec3 p[4] = {
        			c3vec3f(-1+x*10,y*10,0), c3vec3f(1+x*10,y*10,0),
        			c3vec3f(x*10,-1+y*10,0), c3vec3f(x*10,1+y*10,0),
        		};
            	c3geometry_p g = c3geometry_new(
            			c3geometry_type(C3_RAW_TYPE, GL_LINES), grid);
            	g->mat.color = c3vec4f(1.0, 1.0, 1.0, 1.0);
        		c3vertex_array_insert(&g->vertice,
        				g->vertice.count, p, 4);
        	}
        }
    }
    head = c3object_new(c3->root);
    c3transform_new(head);
    {
    	c3geometry_p g = c3geometry_new(
    			c3geometry_type(C3_RAW_TYPE, GL_LINES), head);
    	g->mat.color = c3vec4f(1.0, 0.0, 0.0, 1.0);
		c3vec3 p[4] = {
			c3vec3f(-1, 0, 0), c3vec3f(1, 0, 0),
			c3vec3f(0, -1, 0), c3vec3f(0, 1, 0),
		};
        c3vertex_array_insert(&g->vertice,
        		g->vertice.count, p, 4);
    }
    c3texture_p b = c3texture_new(head);
    c3pixels_init(&b->pixels, 64, 64, 4, 4 * 64, NULL);
    b->geometry.dirty = 1;
    memset(b->pixels.base, 0xff, 10 * b->pixels.row);

	return 1;
}

void
gl_dispose()
{
	c3context_dispose(c3);
}

int
gl_runloop()
{
	glutMainLoop();
	gl_dispose();
	return 0;
}
