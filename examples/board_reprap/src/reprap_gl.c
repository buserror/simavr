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
#define GL_GLEXT_PROTOTYPES
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glut.h>
#include <GL/glext.h>
#endif

#include <stdio.h>
#include <math.h>

#include "reprap.h"
#include "reprap_gl.h"

#include "c3.h"
#include "c3camera.h"
#include "c3driver_context.h"
#include "c3stl.h"
#include "c3lines.h"
#include "c3sphere.h"
#include "c3light.h"
#include "c3program.h"
#include "c3gl.h"
#include "c3gl_fbo.h"

#include <cairo/cairo.h>

struct cairo_surface_t;

int _w = 800, _h = 600;

c3context_p c3 = NULL;
c3context_p hud = NULL;

c3object_p 	head = NULL; 	// hotend
c3texture_p fbo_c3;			// frame buffer object texture
c3program_p fxaa = NULL;	// full screen antialias shader
c3program_p scene = NULL;
c3gl_fbo_t 	fbo;
c3gl_fbo_t 	shadow;


enum {
	uniform_ShadowMap = 0,
	uniform_pixelOffset,
	uniform_tex0,
	uniform_shadowMatrix
};
const char *uniforms_scene[] = {
		"shadowMap",
		"pixelOffset",
		"tex0",
		"shadowMatrix",
		NULL
};

int glsl_version = 110;

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

#define GLCHECK(_w) {_w; dumpError(#_w);}


static void
_gl_reshape_cb(int w, int h)
{
    _w  = w;
    _h = h;

	c3vec2 size = c3vec2f(_w, _h);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, _w, _h);
    c3gl_fbo_resize(&fbo, size);
    c3texture_resize(fbo_c3, size);
    c3context_view_get_at(c3, 0)->size = size;

    if (fxaa) {
    	glUseProgram((GLuint)fxaa->pid);
    	GLCHECK(glUniform2fv((GLuint)fxaa->params.e[0].pid, 1, size.n));
    	glUseProgram(0);
    }

    glutPostRedisplay();
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
		case '1':
			if (fbo_c3->geometry.mat.program)
				fbo_c3->geometry.mat.program = NULL;
			else
				fbo_c3->geometry.mat.program = fxaa;
			glutPostRedisplay();
			break;
	}
}

static void
_gl_display_cb(void)		/* function called whenever redisplay needed */
{
	int drawIndexes[] = { 1, 0 };
	int drawViewStart = c3->root->dirty ? 0 : 1;

	c3vec3 headp = c3vec3f(
			stepper_get_position_mm(&reprap.step_x),
			stepper_get_position_mm(&reprap.step_y),
			stepper_get_position_mm(&reprap.step_z));
	c3mat4 headmove = translation3D(headp);
	c3transform_set(head->transform.e[0], &headmove);

	for (int vi = drawViewStart; vi < 2; vi++) {
		c3context_view_set(c3, drawIndexes[vi]);

		/*
		 * Draw in FBO object
		 */
		c3context_view_p view = c3context_view_get(c3);
		glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)view->bid);
		// draw (without glutSwapBuffers)
		dumpError("glBindFramebuffer fbo");
		glViewport(0, 0, view->size.x, view->size.y);

		c3context_project(c3);

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Set up projection matrix
		glMatrixMode(GL_PROJECTION); // Select projection matrix
		glLoadMatrixf(view->projection.n);

		glEnable(GL_CULL_FACE);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_DEPTH_TEST);
	//	glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

		//glEnable(GL_ALPHA_TEST);
		//glAlphaFunc(GL_GREATER, 1.0f / 255.0f);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Type Of Blending To Use

		glMatrixMode(GL_MODELVIEW);


		if (view->type == C3_CONTEXT_VIEW_EYE) {
		//	glShadeModel(GL_SMOOTH);
		//	glEnable(GL_LIGHTING);
			glCullFace(GL_BACK);
			glEnable(GL_BLEND); // Enable Blending

			c3context_view_p light = c3context_view_get_at(c3, 1);

			// This is matrix transform every coordinate x,y,z
			// x = x* 0.5 + 0.5
			// y = y* 0.5 + 0.5
			// z = z* 0.5 + 0.5
			// Moving from unit cube [-1,1] to [0,1]
			const c3f bias[16] = {
				0.5, 0.0, 0.0, 0.0,
				0.0, 0.5, 0.0, 0.0,
				0.0, 0.0, 0.5, 0.0,
				0.5, 0.5, 0.5, 1.0};

			c3mat4 b = c3mat4_mul(&light->projection, (c3mat4p)bias);
			c3mat4 tex = c3mat4_mul(&light->cam.mtx, &b);

			GLCHECK(glUseProgram((GLuint)scene->pid));
			glUniformMatrix4fv(
					(GLuint)scene->params.e[uniform_shadowMatrix].pid,
					1, GL_FALSE, tex.n);
		} else {
			glCullFace(GL_FRONT);
			glShadeModel(GL_FLAT);
			glDisable(GL_LIGHTING);
			glDisable(GL_BLEND); // Disable Blending
		}

		c3context_draw(c3);
	}

	/*
	 * Draw back FBO over the screen
	 */
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	dumpError("glBindFramebuffer 0");
	glViewport(0, 0, _w, _h);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_CULL_FACE);

	glUseProgram(0);

	glMatrixMode(GL_PROJECTION); // Select projection matrix
	glLoadIdentity(); // Start with an identity matrix

	c3mat4 pro = screen_ortho3D(0, _w, 0, _h, 0, 10);
	glLoadMatrixf(pro.n);

	glMatrixMode(GL_MODELVIEW); // Select modelview matrix

	if (hud->root->dirty) {
	//	printf("reproject head %.2f,%.2f,%.2f\n", headp.x, headp.y,headp.z);
		c3context_project(hud);
	}
	c3context_draw(hud);

    glutSwapBuffers();
}

#if !defined(GLUT_WHEEL_UP)
#  define GLUT_WHEEL_UP   3
#  define GLUT_WHEEL_DOWN 4
#endif


int button;
c3vec2 move;

static
void _gl_button_cb(
		int b,
		int s,
		int x,
		int y)
{
	button = s == GLUT_DOWN ? b : 0;
	move = c3vec2f(x, y);
	c3context_view_p view = c3context_view_get_at(c3, 0);
//	printf("button %d: %.1f,%.1f\n", b, move.x, move.y);
	switch (b) {
		case GLUT_LEFT_BUTTON:
		case GLUT_RIGHT_BUTTON:	// call motion
			break;
		case GLUT_WHEEL_UP:
		case GLUT_WHEEL_DOWN:
			if (view->cam.distance > 10) {
				const float d = 0.004;
				c3cam_set_distance(&view->cam,
						view->cam.distance * ((b == GLUT_WHEEL_DOWN) ? (1.0+d) : (1.0-d)));
				c3cam_update_matrix(&view->cam);
				view->dirty = 1;	// resort the array
			}
			break;
	}
}

void
_gl_motion_cb(
		int x,
		int y)
{
	c3vec2 m = c3vec2f(x, y);
	c3vec2 delta = c3vec2_sub(move, m);
	c3context_view_p view = c3context_view_get_at(c3, 0);

//	printf("%s b%d click %.1f,%.1f now %d,%d delta %.1f,%.1f\n",
//			__func__, button, move.n[0], move.n[1], x, y, delta.x, delta.y);

	switch (button) {
		case GLUT_LEFT_BUTTON: {
			c3mat4 rotx = rotation3D(view->cam.side, delta.n[1] / 4);
			c3mat4 roty = rotation3D(c3vec3f(0.0, 0.0, 1.0), delta.n[0] / 4);
			rotx = c3mat4_mul(&rotx, &roty);
			c3cam_rot_about_lookat(&view->cam, &rotx);
			c3cam_update_matrix(&view->cam);

			view->dirty = 1;	// resort the array
		}	break;
		case GLUT_RIGHT_BUTTON: {
			// offset both points, but following the plane
			c3vec3 f = c3vec3_mulf(
					c3vec3f(-view->cam.side.y, view->cam.side.x, 0),
					-delta.n[1] / 4);
			view->cam.eye = c3vec3_add(view->cam.eye, f);
			view->cam.lookat = c3vec3_add(view->cam.lookat, f);
			c3cam_movef(&view->cam, delta.n[0] / 8, 0, 0);
			c3cam_update_matrix(&view->cam);

		    view->dirty = 1;	// resort the array
		}	break;
	}
	move = m;
}

// gl timer. if the lcd is dirty, refresh display
static void
_gl_timer_cb(
		int i)
{
	glutTimerFunc(1000 / 24, _gl_timer_cb, 0);
	glutPostRedisplay();
}

const c3driver_context_t * c3_driver_list[3] = { NULL, NULL };

int
gl_init(
		int argc,
		char *argv[] )
{
	glutInit(&argc, argv);		/* initialize GLUT system */

	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_ALPHA);
	glutInitWindowSize(_w, _h);		/* width=400pixels height=500pixels */
	/*window =*/ glutCreateWindow("Press 'q' to quit");	/* create window */

	glutDisplayFunc(_gl_display_cb);		/* set window's display callback */
	glutKeyboardFunc(_gl_key_cb);		/* set window's key callback */
	glutTimerFunc(1000 / 24, _gl_timer_cb, 0);

	glutMouseFunc(_gl_button_cb);
	glutMotionFunc(_gl_motion_cb);
    glutReshapeFunc(_gl_reshape_cb);

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);

	// enable color tracking
	glEnable(GL_COLOR_MATERIAL);
	// set material properties which will be assigned by glColor
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

	/* setup some lights */
	GLfloat global_ambient[] = { 0.5f, 0.5f, 0.5f, 1.0f };
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, global_ambient);

	if (0) {
		GLfloat specular[] = {1.0f, 1.0f, 1.0f , 0.8f};
		GLfloat position[] = { 250.0f, -50.0f, 100.0f, 1.0f };
		glLightfv(GL_LIGHT1, GL_SPECULAR, specular);
		glLightfv(GL_LIGHT1, GL_POSITION, position);
		glEnable(GL_LIGHT1);
	}

	/*
	 * Extract the GLSL version as a numeric value for later
	 */
	const char * glsl = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);
	{
		int M = 0, m = 0;
		if (sscanf(glsl, "%d.%d", &M, &m) == 2)
			glsl_version = (M * 100) + m;

	}
	printf("GL_SHADING_LANGUAGE_VERSION %s = %d\n", glsl, glsl_version);

	c3gl_fbo_create(&fbo, c3vec2f(_w, _h), (1 << C3GL_FBO_COLOR)|(1 << C3GL_FBO_DEPTH));
	// shadow buffer

	c3_driver_list[0] = c3gl_getdriver();

    c3 = c3context_new(_w, _h);
    c3->driver = c3_driver_list;

    c3cam_p cam = &c3context_view_get_at(c3, 0)->cam;
	cam->lookat = c3vec3f(100.0, 100.0, 0.0);
	cam->eye = c3vec3f(100.0, -100.0, 100.0);
	// associate the framebuffer object with this view
	c3context_view_get_at(c3, 0)->bid = fbo.fbo;
	/*
	 * Create a light, attach it to a movable object, and attach a sphere
	 * to it too so it's visible.
	 */
	{
		c3object_p ligthhook = c3object_new(c3->root);
	    c3transform_p pos = c3transform_new(ligthhook);

	    pos->matrix = translation3D(c3vec3f(-30.0f, -30.0f, 200.0f));

		c3light_p light = c3light_new(ligthhook);
		light->geometry.name = str_new("light0");
		light->color.specular = c3vec4f(1.0f, 1.0f, 1.0f , 0.8f);
		light->position = c3vec4f(0, 0, 0, 1.0f );

	    {	// light bulb
	    	c3geometry_p g = c3sphere_uv(ligthhook, c3vec3f(0, 0, 0), 3, 10, 10);
	    	g->mat.color = c3vec4f(1.0, 1.0, 0.0, 1.0);
	    	g->hidden = 0;	// hidden from light scenes
	    }
	}
	{
		c3vec2 size = c3vec2f(1024, 1024);
		c3gl_fbo_create(&shadow, size, (1 << C3GL_FBO_DEPTH_TEX));

		c3context_view_t v = {
				.type = C3_CONTEXT_VIEW_LIGHT,
				.size = size,
				.dirty = 1,
				.index = c3->views.count,
				.bid = shadow.fbo,
		};
		c3cam_init(&v.cam);
		c3vec3 listpos = c3vec3f(-30.0f, -30.0f, 200.0f);
		v.cam.eye = listpos;
		v.cam.lookat = c3vec3f(100.0, 100.0, 0.0);
		c3context_view_array_add(&c3->views, v);
	}

    {
    	const char *path = "gfx/hb.png";
        cairo_surface_t * image = cairo_image_surface_create_from_png (path);
        printf("image = %p %p\n", image, cairo_image_surface_get_data (image));
    	c3texture_p b = c3texture_new(c3->root);

    	c3pixels_p dst = c3pixels_new(
    			cairo_image_surface_get_width (image),
    			cairo_image_surface_get_height (image),
    			4, cairo_image_surface_get_stride(image),
    			cairo_image_surface_get_data (image));
		dst->name = str_new(path);
    	dst->normalize = 1;
    	b->geometry.mat.texture = dst;
    	b->size = c3vec2f(200, 200);
		b->geometry.mat.color = c3vec4f(1.0, 1.0, 1.0, 1.0);
//	    c3transform_new(head);
    }
    c3pixels_p brass_tex = NULL;
    {
    	const char *path = "gfx/brass.png";
        cairo_surface_t * image = cairo_image_surface_create_from_png (path);
        printf("image = %p %p\n", image, cairo_image_surface_get_data (image));

    	c3pixels_p dst = c3pixels_new(
    			cairo_image_surface_get_width (image),
    			cairo_image_surface_get_height (image),
    			4, cairo_image_surface_get_stride(image),
    			cairo_image_surface_get_data (image));
		dst->name = str_new(path);
    	dst->normalize = 1;
		c3pixels_array_add(&c3->pixels, dst);
//	    c3transform_new(head);
		brass_tex = dst;
    }
    c3pixels_p line_aa_tex = NULL;
    {
    	const char *path = "gfx/BlurryCircle.png";
        cairo_surface_t * image = cairo_image_surface_create_from_png (path);
        printf("image = %p %p\n", image, cairo_image_surface_get_data (image));

#if 0
    	c3pixels_p dst = &b->pixels;
    	c3pixels_init(dst,
    			cairo_image_surface_get_width (image),
    			cairo_image_surface_get_height (image),
    			1, cairo_image_surface_get_width (image),
    			NULL);
    	c3pixels_alloc(dst);
    	b->size = c3vec2f(32, 32);
    	b->normalized = 1;

    	c3pixels_p src = c3pixels_new(
    			cairo_image_surface_get_width (image),
    			cairo_image_surface_get_height (image),
    			4, cairo_image_surface_get_stride(image),
    			cairo_image_surface_get_data (image));

    	uint32_t * _s = (uint32_t *)src->base;
    	uint8_t * _d = (uint8_t *)dst->base;
    	int max = 0;
    	for (int i = 0; i < dst->h * dst->w; i++)
    		if ((_s[i] & 0xff) > max)
    			max = _s[i] & 0xff;
    	for (int i = 0; i < dst->h * dst->w; i++)
    		*_d++ = ((_s[i] & 0xff) * 255) / max;// + (0xff - max);
    	b->pixels.format = C3PIXEL_A;
#else
    	c3pixels_p dst = c3pixels_new(
    			cairo_image_surface_get_width (image),
    			cairo_image_surface_get_height (image),
    			4, cairo_image_surface_get_stride(image),
    			cairo_image_surface_get_data (image));
    	dst->format = C3PIXEL_ARGB;
    	dst->normalize = 1;
    	dst->name = str_new(path);
    	uint8_t * line = dst->base;
    	for (int y = 0; y < dst->h; y++, line += dst->row) {
    		uint32_t *p = (uint32_t *)line;
    		for (int x = 0; x < dst->w; x++, p++) {
    			uint8_t b = *p;
    			*p = ((0xff - b) << 24);//|(b << 16)|(b << 8)|(b);
    		}
    	}
#endif
    	line_aa_tex = dst;
#if 0
    	c3pixels_p p = dst;
    	printf("struct { int w, h, stride, size, format; uint8_t pix[] } img = {\n"
    			"%d, %d, %d, %d, %d\n",
    			p->w, p->h, (int)p->row, p->psize, cairo_image_surface_get_format(image));
    	for (int i = 0; i < 32; i++)
    		printf("0x%08x ", ((uint32_t*)p->base)[i]);
    	printf("\n");
#endif
    }
    c3object_p grid = c3object_new(c3->root);
    {
        for (int x = 0; x <= 20; x++) {
        	for (int y = 0; y <= 20; y++) {
        		c3vec3 p[4] = {
        			c3vec3f(-1+x*10,y*10,0.01), c3vec3f(1+x*10,y*10,0.01),
        			c3vec3f(x*10,-1+y*10,0.02), c3vec3f(x*10,1+y*10,0.02),
        		};
            	c3geometry_p g = c3geometry_new(
            			c3geometry_type(C3_LINES_TYPE, 0), grid);
            	g->mat.color = c3vec4f(0.0, 0.0, 0.0, 0.8);
            	g->mat.texture = line_aa_tex;
        		c3lines_init(g, p, 4, 0.2);
        	}
        }
    }

   if (0) {
		c3vec3 p[4] = {
			c3vec3f(-5,-5,1), c3vec3f(205,-5,1),
		};
    	c3geometry_p g = c3geometry_new(
    			c3geometry_type(C3_LINES_TYPE, 0), grid);
    	g->mat.color = c3vec4f(0.0, 0.0, 0.0, 1.0);
    	g->mat.texture = line_aa_tex;
    	g->line.width = 2;

		c3vertex_array_insert(&g->vertice,
				g->vertice.count, p, 2);

    }
    head = c3stl_load("gfx/buserror-nozzle-model.stl", c3->root);
    c3transform_new(head);
    if (head->geometry.count > 0) {
    	c3geometry_factor(head->geometry.e[0], 0.1, (20 * M_PI) / 180.0);
    	head->geometry.e[0]->mat.color = c3vec4f(0.6, 0.5, 0.0, 1.0);
    	head->geometry.e[0]->mat.texture = brass_tex;
    }

#if 0
    c3texture_p b = c3texture_new(head);
    c3pixels_init(&b->pixels, 64, 64, 4, 4 * 64, NULL);
    b->geometry.dirty = 1;
    memset(b->pixels.base, 0xff, 10 * b->pixels.row);
#endif


    hud = c3context_new(_w, _h);
    hud->driver = c3_driver_list;

    /*
     * This is the offscreen framebuffer where the 3D scene is drawn
     */
    {
    	/*
    	 * need to insert a header since there is nothing to detect the version number
    	 * reliably without it, and __VERSION__ returns idiocy
    	 */
    	char head[128];
    	sprintf(head, "#version %d\n#define GLSL_VERSION %d\n", glsl_version, glsl_version);

    	const char *uniforms[] = { "g_Resolution", NULL };
        fxaa = c3program_new("fxaa", uniforms);
        c3program_array_add(&hud->programs, fxaa);
        c3program_load_shader(fxaa, GL_VERTEX_SHADER, head,
        		"gfx/postproc.vs", C3_PROGRAM_LOAD_UNIFORM);
        c3program_load_shader(fxaa, GL_FRAGMENT_SHADER, head,
        		"gfx/postproc.fs", C3_PROGRAM_LOAD_UNIFORM);

        c3texture_p b = c3texture_new(hud->root);

    	c3pixels_p dst = c3pixels_new(_w, _h, 4, _w * 4, NULL);
		dst->name = str_new("fbo");
		dst->texture = fbo.buffers[C3GL_FBO_COLOR].bid;
		dst->normalize = 1;
		dst->dirty = 0;
	//	dst->trace = 1;
    	b->geometry.mat.texture = dst;
    	b->geometry.mat.program = fxaa;
    	b->size = c3vec2f(_w, _h);
		b->geometry.mat.color = c3vec4f(1.0, 1.0, 1.0, 1.0);
		fbo_c3 = b;
    }

    {
    	/*
    	 * need to insert a header since there is nothing to detect the version number
    	 * reliably without it, and __VERSION__ returns idiocy
    	 */
    	char head[128];
    	sprintf(head, "#version %d\n#define GLSL_VERSION %d\n", glsl_version, glsl_version);

        scene = c3program_new("scene", uniforms_scene);
        scene->verbose = 1;
        c3program_array_add(&c3->programs, scene);
        c3program_load_shader(scene, GL_VERTEX_SHADER, head,
        		"gfx/scene.vs", C3_PROGRAM_LOAD_UNIFORM);
        c3program_load_shader(scene, GL_FRAGMENT_SHADER, head,
        		"gfx/scene.fs", C3_PROGRAM_LOAD_UNIFORM);
        c3gl_program_load(scene);

		GLCHECK(glUseProgram((GLuint)scene->pid));
        GLCHECK(glUniform1i(
					(GLuint)scene->params.e[uniform_ShadowMap].pid, 7));
		GLCHECK(glUniform1i(
					(GLuint)scene->params.e[uniform_tex0].pid, 0));
		c3vec2 isize = c3vec2f(1.0f / c3->views.e[1].size.x,
					1.0f / c3->views.e[1].size.y);
		GLCHECK(glUniform2fv(
					(GLuint)scene->params.e[uniform_pixelOffset].pid, 1,
					isize.n));
		glActiveTexture(GL_TEXTURE7);
		GLCHECK(glBindTexture(GL_TEXTURE_2D,
					(GLuint)shadow.buffers[C3GL_FBO_DEPTH_TEX].bid));
		glActiveTexture(GL_TEXTURE0);
    }
    {
		c3vec3 p[4] = {
			c3vec3f(10,10,0), c3vec3f(800-10,10,0),
		};
    	c3geometry_p g = c3geometry_new(
    			c3geometry_type(C3_LINES_TYPE, 0), hud->root);
    	g->mat.color = c3vec4f(0.5, 0.5, 1.0, .3f);
    	g->mat.texture = line_aa_tex;
		c3lines_init(g, p, 2, 10);
    }
	return 1;
}

void
gl_dispose()
{
	c3context_dispose(c3);
	c3context_dispose(hud);
	c3gl_fbo_dispose(&fbo);
}

int
gl_runloop()
{
	glutMainLoop();
	gl_dispose();
	return 0;
}
