#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>

#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_elf.h"
#include "sim_gdb.h"
#include "sim_vcd_file.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <pthread.h>

#include "SDL.h"
#include "SDL_image.h"

#include "ac_input.h"
#include "hd44780.h"
#include "hd44780_glut.h"

/* screen width, height, and bit depth */
#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 600
#define SCREEN_BPP     16

int window;
avr_t * avr = NULL;
avr_vcd_t vcd_file;
ac_input_t ac_input;
hd44780_t hd44780;

SDL_Surface *surface;

static void * avr_run_thread( ){
    while(1){
        avr_run(avr);
    }
}

void Quit( int returnCode )
{
    SDL_Quit( );
    exit( returnCode );
}

int resizeWindow( int width, int height )
{
    if ( height == 0 ) height = 1;
    glViewport( 0, 0, ( GLsizei )width, ( GLsizei )height );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity( );
	glOrtho(0, 800, 0, 600, 0, 10);

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity( );

    return 1;
}

void handleKeyPress( SDL_keysym *keysym )
{
    switch ( keysym->sym )
	{
	case SDLK_ESCAPE:
	    Quit( 0 );
	    break;
	case SDLK_F1:
	    SDL_WM_ToggleFullScreen( surface );
	    break;
	default:
	    break;
	}
}

int initGL( GLvoid )
{
    glEnable( GL_TEXTURE_2D );
    glShadeModel( GL_SMOOTH );

    glClearColor( 0.8f, 0.8f, 0.8f, 1.0f );
    glColor4f( 1.0f,1.0f,1.0f,1.0f);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glEnable( GL_BLEND);

    glClearDepth( 1.0f );

    glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

    hd44780_gl_init();

    return 1;
}

int drawGLScene( GLvoid )
{
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glMatrixMode(GL_MODELVIEW); // Select modelview matrix
    glPushMatrix();
	glLoadIdentity(); // Start with an identity matrix
    glScalef(3,3,1);
    glTranslatef( 89.66f,150,0);
    hd44780_gl_draw( &hd44780 );
    glPopMatrix();
    SDL_GL_SwapBuffers();
    return 1;
}


int main(int argc, char *argv[])
{
	elf_firmware_t f;
	const char * fname =  "atmega48_charlcd.axf";
	char path[256];
	sprintf(path, "%s/%s", dirname(argv[0]), fname);
	printf("Firmware pathname is %s\n", path);
	elf_read_firmware(path, &f);

	printf("firmware %s f=%d mmcu=%s\n", fname, (int)f.frequency, f.mmcu);

	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' now known\n", argv[0], f.mmcu);
		exit(1);
	}

    avr_init(avr);
    avr_load_firmware(avr,&f);
    ac_input_init(avr, &ac_input);
    avr_connect_irq(
        ac_input.irq + IRQ_AC_OUT,
        avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'),2));

    hd44780_init(avr, &hd44780);
/*    hd44780_print2x16(&hd44780); */

    /* Connect Data Lines to Port B, 0-3 */
    for( int i =0; i<4;i++){ 
        avr_irq_register_notify(
            avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), i),
            hd44780_data_changed_hook,
            &hd44780);
    }
    /* Connect Cmd Lines */
    for( int i =4; i<7;i++){ 
        avr_irq_register_notify(
            avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), i),
            hd44780_cmd_changed_hook,
            &hd44780);
    }

	avr_vcd_init(avr, "gtkwave_output.vcd", &vcd_file, 100 /* usec */);
	avr_vcd_add_signal(&vcd_file, 
		avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_PIN_ALL), 8 /* bits */ ,
		"portb" );

    avr_vcd_add_signal(&vcd_file,
        ac_input.irq + IRQ_AC_OUT, 1, "ac_input");

    avr_vcd_start(&vcd_file);

    //avr_gdb_init(avr);
    //avr->state = cpu_Stopped;


    avr_vcd_stop(&vcd_file);


    if ( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
	    fprintf( stderr, "Video initialization failed: %s\n",SDL_GetError( ) );
	    Quit( 1 );
	}
    int done =0, isActive=1;
    SDL_Event event;
    int videoFlags = SDL_OPENGL | SDL_GL_DOUBLEBUFFER | SDL_HWPALETTE | SDL_RESIZABLE | SDL_HWSURFACE | SDL_HWACCEL;
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    surface = SDL_SetVideoMode( SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, videoFlags );
    SDL_WM_SetCaption("HD44780 Simulation","");
    if( !surface ) {
        fprintf( stderr,  "Video mode set failed: %s\n", SDL_GetError( ) );
	    Quit( 1 );
	}
    initGL( );
    resizeWindow( SCREEN_WIDTH, SCREEN_HEIGHT);

    pthread_t run;
    pthread_create(&run, NULL, avr_run_thread, NULL);

    while(!done){
	    while ( SDL_PollEvent( &event ) )
		{
		    switch( event.type )
			{
			case SDL_ACTIVEEVENT:
			    if ( event.active.gain == 0 )
				isActive = 0;
			    else
				isActive = 1;
			    break;			    
			case SDL_VIDEORESIZE:
			    /* handle resize event */
			    surface = SDL_SetVideoMode( event.resize.w,
							event.resize.h,
							16, videoFlags );
			    if ( !surface )
				{
				    fprintf( stderr, "Could not get a surface after resize: %s\n", SDL_GetError( ) );
				    Quit( 1 );
				}
			    resizeWindow( event.resize.w, event.resize.h );
			    break;
			case SDL_KEYDOWN:
			    /* handle key presses */
			    handleKeyPress( &event.key.keysym );
			    break;
			case SDL_QUIT:
			    /* handle quit requests */
			    done = 1;
			    break;
			default:
			    break;
			}
		}
        drawGLScene();
    }
    return 0;

}
