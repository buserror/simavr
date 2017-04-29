/*
 pcd8544demo.c

 Copyright Luki <humbell@ethz.ch>
 Copyright 2011 Michel Pollet <buserror@gmail.com>
 Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>
 Copyright 2017 Francisco Demartino <demartino.francisco@gmail.com>

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

#include <fcntl.h>
#include <libgen.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "avr_ioport.h"
#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_gdb.h"
#include "sim_hex.h"
#include "sim_vcd_file.h"
#include "uart_pty.h"

#if __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "pcd8544_glut.h"

uart_pty_t uart_pty;
avr_t * avr = NULL;
avr_vcd_t vcd_file;
pcd8544_t pcd8544;

struct avr_flash {
	char avr_flash_path[1024];
	int avr_flash_fd;
};

// avr special flash initalization
// here: open and map a file to enable a persistent storage for the flash memory
void avr_special_init( avr_t * avr, void * data)
{
	struct avr_flash *flash_data = (struct avr_flash *)data;

	printf("%s\n", __func__);
	// open the file
	flash_data->avr_flash_fd = open(flash_data->avr_flash_path,
									O_RDWR|O_CREAT, 0644);
	if (flash_data->avr_flash_fd < 0) {
		perror(flash_data->avr_flash_path);
		exit(1);
	}
	// resize and map the file the file
	(void)ftruncate(flash_data->avr_flash_fd, avr->flashend + 1);
	ssize_t r = read(flash_data->avr_flash_fd, avr->flash, avr->flashend + 1);
	if (r != avr->flashend + 1) {
		fprintf(stderr, "unable to load flash memory\n");
		perror(flash_data->avr_flash_path);
		exit(1);
	}
}

// avr special flash deinitalization
// here: cleanup the persistent storage
void avr_special_deinit( avr_t* avr, void * data)
{
	struct avr_flash *flash_data = (struct avr_flash *)data;

	printf("%s\n", __func__);
	lseek(flash_data->avr_flash_fd, SEEK_SET, 0);
	ssize_t r = write(flash_data->avr_flash_fd, avr->flash, avr->flashend + 1);
	if (r != avr->flashend + 1) {
		fprintf(stderr, "unable to load flash memory\n");
		perror(flash_data->avr_flash_path);
	}
	close(flash_data->avr_flash_fd);
	uart_pty_stop(&uart_pty);
}


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
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// Select modelview matrix
	glMatrixMode (GL_MODELVIEW);
	glPushMatrix ();
	// Start with an identity matrix
	glLoadIdentity ();
	pcd8544_gl_draw (&pcd8544);
	glPopMatrix ();
	glutSwapBuffers ();
}

// gl timer. if the lcd is dirty, refresh display
void
timerCB (int i)
{
	// restart timer
	glutTimerFunc (1000 / 48, timerCB, 0);
	glutPostRedisplay ();
}

int
initGL (int w, int h, float pix_size)
{

	// Double buffered, RGB disp mode.
	glutInitDisplayMode (GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize (w * pix_size, h * pix_size);
	glutCreateWindow ("PCD8544 (Nokia 5110) 84x48 LED");

	// Set up projection matrix
	glMatrixMode (GL_PROJECTION);
	// Start with an identity matrix
	glLoadIdentity ();
	glOrtho (0, w, 0, h, 0, 1);

  // y flip
	glScalef (1, -1, 1);
	glTranslatef (0, -1 * h, 0);

	// Set window's display callback
	glutDisplayFunc (displayCB);
	// Set window's key callback
	glutKeyboardFunc (keyCB);

	glutTimerFunc (1000 / 24, timerCB, 0);

	return 1;
}

int
main (int argc, char *argv[])
{
	struct avr_flash flash_data;
	char boot_path[1024] = "ATmegaBOOT_168_atmega328.ihex";
	uint32_t boot_base, boot_size;
	char * mmcu = "atmega328p";
	uint32_t freq = 16000000;
	int debug = 0;
	int verbose = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i] + strlen(argv[i]) - 4, ".hex"))
			strncpy(boot_path, argv[i], sizeof(boot_path));
		else if (!strcmp(argv[i], "-d"))
			debug++;
		else if (!strcmp(argv[i], "-v"))
			verbose++;
		else {
			fprintf(stderr, "%s: invalid argument %s\n", argv[0], argv[i]);
			exit(1);
		}
	}

	avr = avr_make_mcu_by_name(mmcu);
	if (!avr) {
		fprintf(stderr, "%s: Error creating the AVR core\n", argv[0]);
		exit(1);
	}

	uint8_t * boot = read_ihex_file(boot_path, &boot_size, &boot_base);
	if (!boot) {
		fprintf(stderr, "%s: Unable to load %s\n", argv[0], boot_path);
		exit(1);
	}
	if (boot_base > 32*1024*1024) {
		mmcu = "atmega2560";
		freq = 20000000;
	}
	printf("%s booloader 0x%05x: %d bytes\n", mmcu, boot_base, boot_size);

	snprintf(flash_data.avr_flash_path, sizeof(flash_data.avr_flash_path),
			"simduino_%s_flash.bin", mmcu);
	flash_data.avr_flash_fd = 0;
	// register our own functions
	avr->custom.init = avr_special_init;
	avr->custom.deinit = avr_special_deinit;
	avr->custom.data = &flash_data;
	avr_init(avr);
	avr->frequency = freq;

	memcpy(avr->flash + boot_base, boot, boot_size);
	free(boot);
	avr->pc = boot_base;
	/* end of flash, remember we are writing /code/ */
	avr->codeend = avr->flashend;
	avr->log = 1 + verbose;

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (debug) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	pcd8544_init (avr, &pcd8544, 84, 48);

	uart_pty_init(avr, &uart_pty);
	uart_pty_connect(&uart_pty, '0');

	// PCD8544 wired to the SPI bus, with the following additional pins:
	pcd8544_wiring_t wiring =
	{
		.chip_select.port = 'D',
		.chip_select.pin = 7,
		.data_command.port = 'D',
		.data_command.pin = 5,
		.reset.port = 'D',
		.reset.pin = 6,
	};

	pcd8544_connect (&pcd8544, &wiring);

	printf ("PCD8544 display demo\n   Press 'q' to quit\n");

	// Initialize GLUT system
	glutInit (&argc, argv);
	initGL (pcd8544.columns, pcd8544.rows, 10);

	pthread_t run;
	pthread_create (&run, NULL, avr_run_thread, NULL);

	glutMainLoop ();
}
