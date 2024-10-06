#include <stdio.h>
#include <pthread.h>
#include "ayab_display.h"
#include "queue.h"

#if __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#define WINDOW_WIDTH  300
#define WINDOW_HEIGHT 100

pthread_t avr_thread;
int avr_thread_running;

event_queue_t event_queue = {.index_read=0, .index_write=0};

shield_t *_shield;
machine_t *_machine;

void
timerCB (int i)
{
	// restart timer
	glutTimerFunc (1000 / 64, timerCB, 0);
    // TODO: Check if dirty/redisplay really required
	glutPostRedisplay ();
}

void
displayCB (void)
{
	// OpenGL rendering
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Set up projection matrix
	glMatrixMode (GL_PROJECTION);
	// Start with an identity matrix
	glLoadIdentity ();
	glOrtho (0, WINDOW_WIDTH, 0, WINDOW_HEIGHT, 0, 10);

    glBegin(GL_QUADS);
    // Draw LEDs
    float x = 10;
    float y = 10;
    int   size = 20;
    for (int i=0; i < 2; i++) {
        x += i * 30;
        if (_shield->led[i]) {
            if (i == 0) {
                glColor3f(1,1,0);
            } else {
                glColor3f(0,1,0);
            }
        } else {
	        glColor3f(0.8,.8,.8);
        }
        glVertex2f(x + size, y + size);
        glVertex2f(x, y + size);
        glVertex2f(x, y);
        glVertex2f(x + size, y);
    }

    // Draw carriage
    x = _machine->carriage.position + 50;
    glColor3f(0.7,0.7,0.7);
    glVertex2f(x - 25, 50);
    glVertex2f(x + 25, 50);
    glVertex2f(x + 25, 70);
    glVertex2f(x - 25, 70);

    glEnd();

    glBegin(GL_LINES);
    glColor3f(1,1,0);
    glVertex2f(x, 50);
    glVertex2f(x, 70);
    glColor3f(1,1,1);
    glVertex2f(50, 70);
    glVertex2f(50, WINDOW_HEIGHT);
    glVertex2f(50+199, 70);
    glVertex2f(50+199, WINDOW_HEIGHT);
    glEnd();

    // Swap display buffer
	glutSwapBuffers ();
}

void
keyCB (unsigned char key, int x, int y)
{
	switch (key)
	{
		case 0x1b:
		case 'q':
            // Terminate the AVR thread ...
            avr_thread_running = 0;
            pthread_join(avr_thread, NULL); 
            // ... and exit
            exit(0);
			break;
		case 'v':
            queue_push(&event_queue, VCD_DUMP, 0);
			break;
		default:
			break;
	}
}

void
specialkeyCB (int key, int x, int y)
{
	switch (key)
    {
		case GLUT_KEY_LEFT:
            queue_push(&event_queue, CARRIAGE_LEFT, 0);
            break;
		case GLUT_KEY_RIGHT:
            queue_push(&event_queue, CARRIAGE_RIGHT, 0);
            break;
		default:
			break;
    }
}

void ayab_display(int argc, char *argv[], void *(*avr_run_thread)(void *), machine_t *machine, shield_t *shield) {

    _shield = shield;
    _machine = machine;

	// initialize GLUT system
	glutInit(&argc, argv);

	// Double buffered, RGB disp mode.
	glutInitDisplayMode (GLUT_RGB | GLUT_DOUBLE);
	glutInitWindowSize (WINDOW_WIDTH, WINDOW_HEIGHT);
	glutCreateWindow ("AYAB Shield");

	// Set window's display callback
	glutDisplayFunc (displayCB);
	// Set window's key callback
	glutKeyboardFunc (keyCB);
    glutSpecialFunc (specialkeyCB);

    glutIgnoreKeyRepeat(0);

	glutTimerFunc (1000 / 24, timerCB, 0);

    // Run avr thread
    avr_thread_running = 1;
	pthread_create(&avr_thread, NULL, avr_run_thread, &avr_thread_running);

	glutMainLoop();
}
