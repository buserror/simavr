#include "hd44780_glut.h"

#include "SDL.h"
#include "SDL_image.h"
#include <GL/gl.h>
#include <GL/glu.h>

static GLuint font_texture;
static int charwidth = 5;
static int charheight = 7;

void hd44780_gl_init(){
    SDL_Surface *image;
    image = IMG_Load("font.tiff");
    if( !image) {
        printf("Problem loading texture\n");
        return;
    }
    glGenTextures(1,&font_texture);
    glBindTexture(GL_TEXTURE_2D, font_texture);
/*    printf("imagew %i, imageh %i, bytesperpixel %i\n", image->w, image->h, image->format->BytesPerPixel); */
    glTexImage2D( GL_TEXTURE_2D, 0, 4, image->w, image->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image->pixels);
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();
    glScalef( 1.0f/(GLfloat)image->w, 1.0f/(GLfloat)image->h,1.0f);

    SDL_FreeSurface(image);
    glMatrixMode(GL_MODELVIEW);
}

void glputchar(char c){
    int index = c;
    int left = index * charwidth;
    int right = index*charwidth+charwidth;
    int top = 0;
    int bottom =7;
    
    glDisable(GL_TEXTURE_2D);
    glColor3f( 0.0f, 1.0f, 0.0f);
    glBegin( GL_QUADS);
       glVertex3i(  5, 7, 0 ); 
       glVertex3i(  0, 7, 0 ); 
       glVertex3i(  0, 0, 0 ); 
      glVertex3i(   5, 0, 0 ); 
    glEnd( );                           

    glEnable(GL_TEXTURE_2D);
    glColor3f( 1.0f, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glBegin( GL_QUADS);
      glTexCoord2i(right,top);      glVertex3i(  5, 7, 0 ); 
      glTexCoord2i(left, top);      glVertex3i(  0, 7, 0 ); 
      glTexCoord2i(left, bottom);   glVertex3i(  0, 0, 0 ); 
      glTexCoord2i(right,bottom);  glVertex3i(   5, 0, 0 ); 
    glEnd( );                           

}

void glputstr( char *str){
    while( *(++str) != 0 ){
        glputchar(*str);
        glTranslatef(6,0,0);
    }
}

void hd44780_gl_draw( hd44780_t *b){
    int rows = 16;
    int lines = 2;
    int border = 3;
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.0f,0.4f,0.0f);
    glTranslatef(0,-8,0);
    glBegin(GL_QUADS);
        glVertex3f( rows*charwidth + (rows-1) + border, -border,0);
        glVertex3f( - border, -border,0);
        glVertex3f( - border, lines*charheight + (lines-1)+border,0);
        glVertex3f( rows*charwidth + (rows-1) + border, lines*charheight + (lines-1)+border,0);
    glEnd();
    glTranslatef(0,8,0);
    glColor3f(1.0f,1.0f,1.0f);
    for( int i=0; i<16; i++) {
        glputchar( b->ddram[i] );
        glTranslatef(6,0,0);
    }
    glTranslatef(-96,-8,0);
    for( int i=0; i<16; i++) {
        glputchar( b->ddram[i+0x40] );
        glTranslatef(6,0,0);
    }
    
}
