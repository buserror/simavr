/*
	keypress.c
	
	Copyright 2017 Al Popov

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
#if __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <pthread.h>

#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_elf.h"
#include "sim_gdb.h"
//#include "sim_vcd_file.h"

#include "button.h"

button_t button1;
button_t button2;
int do_button_press1 = 0;
int do_button_press2 = 0;
avr_t * avr = NULL;
//avr_vcd_t vcd_file;
uint8_t	pin_state = 0;

float pixsize = 64;
int window;

/*
 * called when some button is pressed (i.e. pin state on the port D is changed)
 * so lets update our buffer
 */
void output_pin_changed_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	int pin_no = irq->irq;
	if (pin_no == 5)
		pin_no = 0;
	else if (pin_no == 7)
		pin_no = 1;
	pin_state = (pin_state & ~(1 << pin_no)) | (value << pin_no);
}

void displayCB(void)		/* function called whenever redisplay needed */
{
	// OpenGL rendering goes here...
	glClear(GL_COLOR_BUFFER_BIT);

	// Set up modelview matrix
	glMatrixMode(GL_MODELVIEW); // Select modelview matrix
	glLoadIdentity(); // Start with an identity matrix

	float grid = pixsize;
	float size = grid * 0.8;
    glBegin(GL_QUADS);
	glColor3f(1,0,0);

	for (int di = 0; di < 8; di++) {
		char on = (pin_state & (1 << di)) != 0;
		if (on) {
			float x = (di) * grid;
			float y = 0; //(si * grid * 8) + (di * grid);
			glVertex2f(x + size, y + size);
			glVertex2f(x, y + size);
			glVertex2f(x, y);
			glVertex2f(x + size, y);
		}
	}

    glEnd();
    glutSwapBuffers();
    //glFlush();				/* Complete any pending operations */
}

void keyCB(unsigned char key, int x, int y)	/* called on key press */
{
	if (key == 'q')
		exit(0);
	//static uint8_t buf[64];
	switch (key) {
		case 'q':
		case 0x1f: // escape
			exit(0);
			break;
		case '1':
			do_button_press1++; // pass the message to the AVR thread
			break;
		case '2':
			do_button_press2++; // pass the message to the AVR thread
			break;
	}
}

// gl timer. if the pin have changed states, refresh display
void timerCB(int i)
{
	static uint8_t oldstate = 0xff;
	// restart timer
	glutTimerFunc(1000/64, timerCB, 0);

	if (oldstate != pin_state) {
		oldstate = pin_state;
		glutPostRedisplay();
	}
}

//#undef AVR_IOPORT_INTRN_PULLUP_IMP
static void * avr_run_thread(void * oaram)
{
	int b_press1 = do_button_press1;
	int b_press2 = do_button_press2;
	int b_buttons_hooked_up = 0;

	while (1) {
		int b_user_activity = 0;
		avr_run(avr);
		if (do_button_press1 != b_press1) {
			b_press1 = do_button_press1;
			printf("Button 1 pressed\n");
			button_press(&button1, 1000000);
			b_user_activity = 1;
		}
		if (do_button_press2 != b_press2) {
			b_press2 = do_button_press2;
			printf("Button 2 pressed\n");
			button_press(&button2, 1000000);
			b_user_activity = 2;
		}
#ifdef AVR_IOPORT_INTRN_PULLUP_IMP
		// If we wish to take a control over internally pulled-up input pin,
		// that is, we have a "low output impedance source" connected to the pin,
		// we must explicitly inform simavr about it.
		if (!b_buttons_hooked_up && b_user_activity) {
			avr_raise_irq_float(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'),
									IOPORT_IRQ_PIN0_SRC_IMP), 0, 1);
			avr_raise_irq_float(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'),
									IOPORT_IRQ_PIN0_SRC_IMP + 1), 0, 1);
			b_buttons_hooked_up = 1;
		}
		// Otherwise simavr internall pull-ups handling is active and will "override"
		// the pin state in some situations.
#endif //AVR_IOPORT_INTRN_PULLUP_IMP
	}
	return NULL;
}


int main(int argc, char *argv[])
{
	elf_firmware_t f;
	const char * fname =  "atmega48_keypress.axf";
	//char path[256];

//	sprintf(path, "%s/%s", dirname(argv[0]), fname);
//	printf("Firmware pathname is %s\n", path);
	elf_read_firmware(fname, &f);

	printf("firmware %s f=%d mmcu=%s\n", fname, (int)f.frequency, f.mmcu);

	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' not known\n", argv[0], f.mmcu);
		exit(1);
	}
	avr_init(avr);
	avr_load_firmware(avr, &f);

	// initialize our 'peripherals'
	button_init(avr, &button1, "button0");
	button_init(avr, &button2, "button1");
	// "connect" the output irq of our buttons to the AVR input pins of port D
	avr_connect_irq(
		button1.irq + IRQ_BUTTON_OUT,
		avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 0));
	avr_connect_irq(
		button2.irq + IRQ_BUTTON_OUT,
		avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 1));

	// connect output pins of the AVR to our callback
	avr_irq_register_notify(
			avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 5),
			output_pin_changed_hook,
			NULL);
	avr_irq_register_notify(
			avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 7),
			output_pin_changed_hook,
			NULL);

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (0) {
		//avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	// 'raise' it, it's a "pullup"
	//avr_raise_irq(button0.irq + IRQ_BUTTON_OUT, 1);

	printf( "Demo launching:\n"
			"   Press 'q' to quit\n\n"
			"   Press '1' to extinguish the first LED\n"
			"   Press '2' to extinguish the second LED\n"
			);

	/*
	 * OpenGL init, can be ignored
	 */
	glutInit(&argc, argv);		/* initialize GLUT system */

	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(8 * pixsize, 1 * pixsize);		/* width=400pixels height=500pixels */
	window = glutCreateWindow("Simavr key press test");	/* create window */

	// Set up projection matrix
	glMatrixMode(GL_PROJECTION); // Select projection matrix
	glLoadIdentity(); // Start with an identity matrix
	glOrtho(0, 8 * pixsize, 0, 1 * pixsize, 0, 10);
	glScalef(1,-1,1);
	glTranslatef(0, -1 * pixsize, 0);

	glutDisplayFunc(displayCB);		/* set window's display callback */
	glutKeyboardFunc(keyCB);		/* set window's key callback */
	glutTimerFunc(1000 / 24, timerCB, 0);

	// the AVR run on it's own thread. it even allows for debugging!
	pthread_t run;
	pthread_create(&run, NULL, avr_run_thread, NULL);

	glutMainLoop();
}
