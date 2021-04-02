/*
 charlcd.c

 Copyright Luki <humbell@ethz.ch>
 Copyright 2011 Michel Pollet <buserror@gmail.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>

#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_elf.h"
#include "sim_gdb.h"

#if __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <pthread.h>

#include "ssd1306_glut.h"

int window_identifier;

avr_t * avr = NULL;
ssd1306_t ssd1306;

int win_width, win_height;

static void *
avr_run_thread (void * ignore)
{
	while (1)
	{
		avr_run (avr);
	}
	return NULL;
}

/* Called on a key press */
void
keyCB (unsigned char key, int x, int y)
{
	switch (key)
	{
		case 'q':
			exit (0);
			break;
	}
}

/* Function called whenever redisplay needed */
void
displayCB (void)
{
	const uint8_t seg_remap_default = ssd1306_get_flag (
	                &ssd1306, SSD1306_FLAG_SEGMENT_REMAP_0);
	const uint8_t seg_comscan_default = ssd1306_get_flag (
	                &ssd1306, SSD1306_FLAG_COM_SCAN_NORMAL);

	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Set up projection matrix
	glMatrixMode (GL_PROJECTION);
	// Start with an identity matrix
	glLoadIdentity ();
	glOrtho (0, win_width, 0, win_height, 0, 10);
	// Apply vertical and horizontal display mirroring
	glScalef (seg_remap_default ? 1 : -1, seg_comscan_default ? -1 : 1, 1);
	glTranslatef (seg_remap_default ? 0 : -win_width, seg_comscan_default ? -win_height : 0, 0);

	// Select modelview matrix
	glMatrixMode (GL_MODELVIEW);
	glPushMatrix ();
	// Start with an identity matrix
	glLoadIdentity ();
	ssd1306_gl_draw (&ssd1306);
	glPopMatrix ();
	glutSwapBuffers ();
}

// gl timer. if the lcd is dirty, refresh display
void
timerCB (int i)
{
	// restart timer
	glutTimerFunc (1000 / 64, timerCB, 0);
	glutPostRedisplay ();
}

int
initGL (int w, int h, float pix_size)
{
	win_width = w * pix_size;
	win_height = h * pix_size;

	// Double buffered, RGB disp mode.
	glutInitDisplayMode (GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize (win_width, win_height);
	window_identifier = glutCreateWindow ("SSD1306 128x64 OLED");

	// Set window's display callback
	glutDisplayFunc (displayCB);
	// Set window's key callback
	glutKeyboardFunc (keyCB);

	glutTimerFunc (1000 / 24, timerCB, 0);

	ssd1306_gl_init (pix_size, SSD1306_GL_WHITE);

	return 1;
}

int
main (int argc, char *argv[])
{
	elf_firmware_t f = {{0}};
	const char * fname = "atmega32_ssd1306.axf";
	char path[256];
	sprintf (path, "%s/%s", dirname (argv[0]), fname);
	printf ("Firmware pathname is %s\n", path);
	elf_read_firmware (fname, &f);

	printf ("firmware %s f=%d mmcu=%s\n", fname, (int) f.frequency, f.mmcu);

	avr = avr_make_mcu_by_name (f.mmcu);
	if (!avr)
	{
		fprintf (stderr, "%s: AVR '%s' not known\n", argv[0], f.mmcu);
		exit (1);
	}

	avr_init (avr);
	avr_load_firmware (avr, &f);

	ssd1306_init (avr, &ssd1306, 128, 64);

	// SSD1306 wired to the SPI bus, with the following additional pins:
	ssd1306_wiring_t wiring =
	{
		.chip_select.port = 'B',
		.chip_select.pin = 4,
		.data_instruction.port = 'B',
		.data_instruction.pin = 1,
		.reset.port = 'B',
		.reset.pin = 3,
	};

	ssd1306_connect (&ssd1306, &wiring);

	printf ("SSD1306 display demo\n   Press 'q' to quit\n");

	// Initialize GLUT system
	glutInit (&argc, argv);
	initGL (ssd1306.columns, ssd1306.rows, 2.0);

	pthread_t run;
	pthread_create (&run, NULL, avr_run_thread, NULL);

	glutMainLoop ();
}
