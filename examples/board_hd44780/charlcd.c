/*
	charlcd.c

	Copyright Luki <humbell@ethz.ch>
	Copyright 2011 Michel Pollet <buserror@gmail.com>

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
#include "sim_vcd_file.h"

#if __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <pthread.h>

#include "ac_input.h"
#include "hd44780_glut.h"


//float pixsize = 16;
int window;

avr_t * avr = NULL;
avr_vcd_t vcd_file;
ac_input_t ac_input;
hd44780_t hd44780;

int color = 0;
uint32_t colors[][4] = {
		{ 0x00aa00ff, 0x00cc00ff, 0x000000ff, 0x00000055 },	// fluo green
		{ 0xaa0000ff, 0xcc0000ff, 0x000000ff, 0x00000055 },	// red
};

static void *
avr_run_thread(
		void * ignore)
{
	while (1) {
		avr_run(avr);
	}
	return NULL;
}

void keyCB(
		unsigned char key, int x, int y)	/* called on key press */
{
	switch (key) {
		case 'q':
			avr_vcd_stop(&vcd_file);
			exit(0);
			break;
		case 'r':
			printf("Starting VCD trace; press 's' to stop\n");
			avr_vcd_start(&vcd_file);
			break;
		case 's':
			printf("Stopping VCD trace\n");
			avr_vcd_stop(&vcd_file);
			break;
	}
}


void displayCB(void)		/* function called whenever redisplay needed */
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW); // Select modelview matrix
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
	glutSwapBuffers();
}

// gl timer. if the lcd is dirty, refresh display
void timerCB(int i)
{
	//static int oldstate = -1;
	// restart timer
	glutTimerFunc(1000/64, timerCB, 0);
	glutPostRedisplay();
}

int
initGL(int w, int h)
{
	// Set up projection matrix
	glMatrixMode(GL_PROJECTION); // Select projection matrix
	glLoadIdentity(); // Start with an identity matrix
	glOrtho(0, w, 0, h, 0, 10);
	glScalef(1,-1,1);
	glTranslatef(0, -1 * h, 0);

	glutDisplayFunc(displayCB);		/* set window's display callback */
	glutKeyboardFunc(keyCB);		/* set window's key callback */
	glutTimerFunc(1000 / 24, timerCB, 0);

	glEnable(GL_TEXTURE_2D);
	glShadeModel(GL_SMOOTH);

	glClearColor(0.8f, 0.8f, 0.8f, 1.0f);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	hd44780_gl_init();

	return 1;
}

int
main(
		int argc,
		char *argv[])
{
	elf_firmware_t f = {{0}};
	const char *fname = argc > 1 ? argv[1] : "atmega48_charlcd.axf";
//	char path[256];
//	sprintf(path, "%s/%s", dirname(argv[0]), fname);
//	printf("Firmware pathname is %s\n", path);
	if (elf_read_firmware(fname, &f) == -1)
	{
		fprintf(stderr, "Unable to load firmware from file %s\n", fname);
		exit(EXIT_FAILURE);
	};

	printf("firmware %s f=%d mmcu=%s\n", fname, (int) f.frequency, f.mmcu);

	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' not known\n", argv[0], f.mmcu);
		exit(1);
	}

	avr_init(avr);
	avr_load_firmware(avr, &f);
	ac_input_init(avr, &ac_input);
	avr_connect_irq(ac_input.irq + IRQ_AC_OUT, avr_io_getirq(avr,
			AVR_IOCTL_IOPORT_GETIRQ('D'), 2));

	hd44780_init(avr, &hd44780, 20, 4);

	hd44780_setup_mutex_for_gl(&hd44780);

	/* Connect Data Lines to Port B, 0-3 */
	/* These are bidirectional too */
	for (int i = 0; i < 4; i++) {
		avr_irq_t * iavr = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), i);
		avr_irq_t * ilcd = hd44780.irq + IRQ_HD44780_D4 + i;
		// AVR -> LCD
		avr_connect_irq(iavr, ilcd);
		// LCD -> AVR
		avr_connect_irq(ilcd, iavr);
	}
	avr_connect_irq(
			avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 4),
			hd44780.irq + IRQ_HD44780_RS);
	avr_connect_irq(
			avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 5),
			hd44780.irq + IRQ_HD44780_E);
	avr_connect_irq(
			avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 6),
			hd44780.irq + IRQ_HD44780_RW);


	avr_vcd_init(avr, "gtkwave_output.vcd", &vcd_file, 10 /* usec */);
	avr_vcd_add_signal(&vcd_file,
			avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_PIN_ALL),
			4 /* bits */, "D4-D7");
	avr_vcd_add_signal(&vcd_file,
			avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 4),
			1 /* bits */, "RS");
	avr_vcd_add_signal(&vcd_file,
			avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 5),
			1 /* bits */, "E");
	avr_vcd_add_signal(&vcd_file,
			avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 6),
			1 /* bits */, "RW");
	avr_vcd_add_signal(&vcd_file,
			hd44780.irq + IRQ_HD44780_BUSY,
			1 /* bits */, "LCD_BUSY");
	avr_vcd_add_signal(&vcd_file,
			hd44780.irq + IRQ_HD44780_ADDR,
			7 /* bits */, "LCD_ADDR");
	avr_vcd_add_signal(&vcd_file,
			hd44780.irq + IRQ_HD44780_DATA_IN,
			8 /* bits */, "LCD_DATA_IN");
	avr_vcd_add_signal(&vcd_file,
			hd44780.irq + IRQ_HD44780_DATA_OUT,
			8 /* bits */, "LCD_DATA_OUT");

	avr_vcd_add_signal(&vcd_file, ac_input.irq + IRQ_AC_OUT, 1, "ac_input");

	avr_vcd_start(&vcd_file);

	printf( "Demo : This is HD44780 LCD demo\n"
			"   You can configure the width&height of the LCD in the code\n"
			"   Press 'r' to start recording a 'wave' file - with a LOT of data\n"
			"   Press 's' to stop recording\n"
			);

	/*
	 * OpenGL init, can be ignored
	 */
	glutInit(&argc, argv);		/* initialize GLUT system */

	int w = 5 + hd44780.w * 6;
	int h = 5 + hd44780.h * 8;
	int pixsize = 3;

	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(w * pixsize, h * pixsize);		/* width=400pixels height=500pixels */
	window = glutCreateWindow("Press 'q' to quit");	/* create window */

	initGL(w * pixsize, h * pixsize);

	pthread_t run;
	pthread_create(&run, NULL, avr_run_thread, NULL);

	glutMainLoop();
}
