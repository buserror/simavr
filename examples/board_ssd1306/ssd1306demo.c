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
#include "ssd1306_glut.h"

uint32_t colors[][2] =
  {
    { 0x00aa00ff, 0x00cc00ff},	// fluo green
    { 0xaa0000ff, 0xcc0000ff},	// red
    { 0x00000000, 0xffffffff},	// black/white
  };

enum {green, red, black} disp_color = black;

//float pixsize = 16;
int window_identifier;

avr_t * avr = NULL;
avr_vcd_t vcd_file;
ac_input_t ac_input;
ssd1306_t ssd1306;

void
keyCB (unsigned char key, int x, int y) /* called on key press */
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

  glMatrixMode (GL_MODELVIEW); // Select modelview matrix
  glPushMatrix ();
  glLoadIdentity (); // Start with an identity matrix
  //glScalef (1, 1, 1);

  ssd1306_gl_draw (&ssd1306, colors[disp_color][0], colors[disp_color][1]);
  glPopMatrix ();
  glutSwapBuffers ();
}

// gl timer. if the lcd is dirty, refresh display
void
timerCB (int i)
{
  //static int oldstate = -1;
  // restart timer
  glutTimerFunc (1000 / 64, timerCB, 0);
  glutPostRedisplay ();
}

int
initGL (int w, int h, float pix_size)
{
  // See: http://www.lighthouse3d.com/tutorials/glut-tutorial/initialization/

  w *= pix_size;
  h *= pix_size;

  // Double buffered, RGB disp mode.
  glutInitDisplayMode (GLUT_RGB | GLUT_DOUBLE);
  glutInitWindowSize (w, h);
  window_identifier = glutCreateWindow ("SSD1306");

  // Set up projection matrix
  glMatrixMode (GL_PROJECTION); // Select projection matrix
  glLoadIdentity (); // Start with an identity matrix
  glOrtho (0, w, 0, h, 0, 10);
  glScalef (1, -1, 1);
  glTranslatef (0, -1 * h, 0);

  // Set window's display callback
  glutDisplayFunc (displayCB);
  // Set window's key callback
  glutKeyboardFunc (keyCB);

  glutTimerFunc (1000 / 24, timerCB, 0);

  glEnable (GL_TEXTURE_2D);
  glShadeModel (GL_SMOOTH);

  glClearColor (0.8f, 0.8f, 0.8f, 1.0f);
  glColor4f (1.0f, 1.0f, 1.0f, 1.0f);

  glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable (GL_BLEND);

  ssd1306_gl_init(pix_size);

  return 1;
}

int
main (int argc, char *argv[])
{
  elf_firmware_t f;
  const char * fname = "atmega32_ssd1306.axf";
  char path[256];
  sprintf (path, "%s/%s", dirname (argv[0]), fname);
  elf_read_firmware (fname, &f);
  avr = avr_make_mcu_by_name (f.mmcu);
  ssd1306_init (avr, &ssd1306, 128, 64);

  /*  SSD 1306 CONNECTIONS
  LCD_RES       PB3	Reset
  LCD_RS	PB1	Data / Instruction (DC)
  LCD_E		PB4	Slave select (CS)
  SCK 		PB7	System clock
  MOSI 		PB5	Master out, slave in
  MISO 		NC	Master in, slave out
  */

  /*
  avr_connect_irq (avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 3),
		   ssd1306.irq + IRQ_SSD1306_RESET);
  avr_connect_irq (avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 4),
		   ssd1306.irq + IRQ_SSD1306_ENABLE);
  avr_connect_irq (avr_io_getirq (avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 1),
		   ssd1306.irq + IRQ_SSD1306_DATA_INSTRUCTION); */


  printf ("Demo : This is SSD1306 display demo v0.01\n"
	  "   Press 'q' to quit\n");

  // Screen
  int w = ssd1306.w;
  int h = ssd1306.h;
  glutInit (&argc, argv); /* initialize GLUT system */
  initGL (w, h, 0.5);

  glutMainLoop ();
}
