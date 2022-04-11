/*
	timer_64led.c
	
	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

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
#include "avr_spi.h"
#include "avr_timer.h"
#include "sim_elf.h"
#include "sim_gdb.h"
#include "sim_vcd_file.h"

#include "button.h"
#include "hc595.h"

enum {
	B_START = 0, B_STOP, B_RESET,
	B_MAX
};
button_t button[B_MAX]; // Start/Stop/Reset
volatile int do_button_press[B_MAX] = {0};
avr_t * avr = NULL;
avr_vcd_t vcd_file;
hc595_t shifter;

int display_flag = 0;
volatile uint32_t	display_bits = 0;
volatile uint8_t	display_pwm = 0;

float pixsize = 16;
int window;

/*
 * called when the AVR has latched the 595
 */
void hc595_changed_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	display_bits = value;
	display_flag++;
}

/*
 * called when the AVR has changed the display brightness
 */
void pwm_changed_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	display_pwm = value;
	display_flag++;
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
	float color_on = (float)(0xff - display_pwm) / 15.0f;
	float color_off = 0.1;
	if (color_on < color_off)
		color_on = color_off;

	glTranslatef(pixsize / 2.25f, pixsize / 1.8f, 0);
	
    glBegin(GL_QUADS);
	
	for (int di = 0; di < 4; di++) {
		uint8_t digit = display_bits >> (di * 8);
		
		for (int i = 0; i < 8; i++) {	
			glColor3f(0,0, digit & (1 << i) ? color_on : color_off);
			float dx = ((di * 5.5)) * pixsize, dy = 0*pixsize;
			switch (i) {
				case 3:
					dy += 3.0f * pixsize;
					FALLTHROUGH
				case 6:
					dy += 3.0f * pixsize;
					FALLTHROUGH
				case 0:
					dx += 1.0f * pixsize;
					glVertex2f(dx + size, dy + size); glVertex2f(dx, dy + size); glVertex2f(dx, dy); glVertex2f(dx + size, dy);
					dx += 1.0f * pixsize;
					glVertex2f(dx + size, dy + size); glVertex2f(dx, dy + size); glVertex2f(dx, dy); glVertex2f(dx + size, dy);
					break;
				case 7:	// dot!
					dx += 4.25 * pixsize;
					switch (di) {
						case 0:
						case 3:
							dy += 6.25 * pixsize;
							break;
						case 1:
							dy += 2 * pixsize;
							break;
						case 2:
							dy += 4 * pixsize;
							dx -= 5.50 * pixsize;
							break;
					}
							
					glVertex2f(dx + size, dy + size); glVertex2f(dx, dy + size); glVertex2f(dx, dy); glVertex2f(dx + size, dy);					
					break;
				default:
					if (i == 1 || i == 2)
						dx += 3.0f * pixsize;
					if (i == 4 || i == 2)
						dy += 4.0f * pixsize;
					else
						dy += 1.0f * pixsize;					
					glVertex2f(dx + size, dy + size); glVertex2f(dx, dy + size); glVertex2f(dx, dy); glVertex2f(dx + size, dy);
					dy += 1.0f * pixsize;
					glVertex2f(dx + size, dy + size); glVertex2f(dx, dy + size); glVertex2f(dx, dy); glVertex2f(dx + size, dy);
					break;
			}
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
		case '1' ... '3':
			printf("Press %d\n", key-'1');
			do_button_press[key-'1']++; // pass the message to the AVR thread
			break;
		case 'r':
			printf("Starting VCD trace\n");
			avr_vcd_start(&vcd_file);
			break;
		case 's':
			printf("Stopping VCD trace\n");
			avr_vcd_stop(&vcd_file);
			break;
	}
}

// gl timer. if the pin have changed states, refresh display
void timerCB(int i)
{
	static int oldstate = -1;
	// restart timer
	glutTimerFunc(1000/64, timerCB, 0);

	if (oldstate != display_flag) {
		oldstate = display_flag;
		glutPostRedisplay();
	}
}

static void * avr_run_thread(void * ignore)
{
	int b_press[3] = {0};
	
	while (1) {
		avr_run(avr);

		for (int i = 0; i < 3; i++) {
			if (do_button_press[i] != b_press[i]) {
				b_press[i] = do_button_press[i];
				printf("Button pressed %d\n", i);
				button_press(&button[i], 100000);
			}
		}
	}
	return NULL;
}


int main(int argc, char *argv[])
{
	elf_firmware_t f = {{0}};
	const char * fname =  "atmega168_timer_64led.axf";
	//char path[256];

//	sprintf(path, "%s/%s", dirname(argv[0]), fname);
	//printf("Firmware pathname is %s\n", path);
	elf_read_firmware(fname, &f);

	printf("firmware %s f=%d mmcu=%s\n", fname, (int)f.frequency, f.mmcu);

	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' not known\n", argv[0], f.mmcu);
		exit(1);
	}
	avr_init(avr);
	avr_load_firmware(avr, &f);

	//
	// initialize our 'peripherals'
	//
	hc595_init(avr, &shifter);
	
	button_init(avr, &button[B_START], "button.start");
	avr_connect_irq(
		button[B_START].irq + IRQ_BUTTON_OUT,
		avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('C'), 0));
	button_init(avr, &button[B_STOP], "button.stop");
	avr_connect_irq(
		button[B_STOP].irq + IRQ_BUTTON_OUT,
		avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 1));
	button_init(avr, &button[B_RESET], "button.reset");
	avr_connect_irq(
		button[B_RESET].irq + IRQ_BUTTON_OUT,
		avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 0));

	// connects the fake 74HC595 array to the pins
	avr_irq_t * i_mosi = avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT),
			* i_reset = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 4),
			* i_latch = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 7);
	avr_connect_irq(i_mosi, shifter.irq + IRQ_HC595_SPI_BYTE_IN);
	avr_connect_irq(i_reset, shifter.irq + IRQ_HC595_IN_RESET);
	avr_connect_irq(i_latch, shifter.irq + IRQ_HC595_IN_LATCH);

	avr_irq_t * i_pwm = avr_io_getirq(avr, AVR_IOCTL_TIMER_GETIRQ('0'), TIMER_IRQ_OUT_PWM0);
	avr_irq_register_notify(
		i_pwm,
		pwm_changed_hook, 
		NULL);	
	avr_irq_register_notify(
		shifter.irq + IRQ_HC595_OUT,
		hc595_changed_hook, 
		NULL);

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (0) {
		//avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	/*
	 *	VCD file initialization
	 *	
	 *	This will allow you to create a "wave" file and display it in gtkwave
	 *	Pressing "r" and "s" during the demo will start and stop recording
	 *	the pin changes
	 */
	avr_vcd_init(avr, "gtkwave_output.vcd", &vcd_file, 10000 /* usec */);

	avr_vcd_add_signal(&vcd_file, 
		avr_get_interrupt_irq(avr, 7), 1 /* bit */ ,
		"TIMER2_COMPA" );
	avr_vcd_add_signal(&vcd_file, 
		avr_get_interrupt_irq(avr, 17), 1 /* bit */ ,
		"SPI_INT" );
	avr_vcd_add_signal(&vcd_file, 
		i_mosi, 8 /* bits */ ,
		"MOSI" );

	avr_vcd_add_signal(&vcd_file, 
		i_reset, 1 /* bit */ ,
		"595_RESET" );
	avr_vcd_add_signal(&vcd_file, 
		i_latch, 1 /* bit */ ,
		"595_LATCH" );
	avr_vcd_add_signal(&vcd_file, 
		button[B_START].irq + IRQ_BUTTON_OUT, 1 /* bits */ ,
		"start" );
	avr_vcd_add_signal(&vcd_file, 
		button[B_STOP].irq + IRQ_BUTTON_OUT, 1 /* bits */ ,
		"stop" );
	avr_vcd_add_signal(&vcd_file, 
		button[B_RESET].irq + IRQ_BUTTON_OUT, 1 /* bits */ ,
		"reset" );

	avr_vcd_add_signal(&vcd_file, 
		shifter.irq + IRQ_HC595_OUT, 32 /* bits */ ,
		"HC595" );
	avr_vcd_add_signal(&vcd_file, 
		i_pwm, 8 /* bits */ ,
		"PWM" );

	// 'raise' it, it's a "pullup"
	avr_raise_irq(button[B_START].irq + IRQ_BUTTON_OUT, 1);
	avr_raise_irq(button[B_STOP].irq + IRQ_BUTTON_OUT, 1);
	avr_raise_irq(button[B_RESET].irq + IRQ_BUTTON_OUT, 1);

	printf( "Demo : This is a real world firmware, a 'stopwatch'\n"
			"   timer that can count up to 99 days. It features a PWM control of the\n"
			"   brightness, blinks the dots, displays the number of days spent and so on.\n\n"
			"   Press '1' to press the 'start' button\n"
			"   Press '2' to press the 'stop' button\n"
			"   Press '3' to press the 'reset' button\n"
			"   Press 'q' to quit\n\n"
			"   Press 'r' to start recording a 'wave' file - with a LOT of data\n"
			"   Press 's' to stop recording\n"
			"  + Make sure to watch the brightness dim once you stop the timer\n\n"
			);

	/*
	 * OpenGL init, can be ignored
	 */
	glutInit(&argc, argv);		/* initialize GLUT system */


	int w = 22, h = 8;
	
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize(w * pixsize, h * pixsize);		/* width=400pixels height=500pixels */
	window = glutCreateWindow("Press 0, 1, 2 or q");	/* create window */

	// Set up projection matrix
	glMatrixMode(GL_PROJECTION); // Select projection matrix
	glLoadIdentity(); // Start with an identity matrix
	glOrtho(0, w * pixsize, 0, h * pixsize, 0, 10);
	glScalef(1,-1,1);
	glTranslatef(0, -1 * h * pixsize, 0);

	glutDisplayFunc(displayCB);		/* set window's display callback */
	glutKeyboardFunc(keyCB);		/* set window's key callback */
	glutTimerFunc(1000 / 24, timerCB, 0);

	// the AVR run on it's own thread. it even allows for debugging!
	pthread_t run;
	pthread_create(&run, NULL, avr_run_thread, NULL);

	glutMainLoop();
}
