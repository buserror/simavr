/* ----------------------------------------------------------------------------
	128x64 Graphic LCD management for SSD1306 driver

	FILE NAME 	: SSD1306.h
	
	DESCRIPTION	: The purpose of this function is to manage a graphic LCD
			  by providing function for control and display text and graphic 
			  
	AUTHOR		: Gabriel Anzziani
    www.gabotronics.com

    MODIFIED BY  : Doug Szumski

*/

#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

#define SSD1306_PIXELS 		1024
#define SSD1306_X_PIXEL_ROWS 	128
#define SSD1306_Y_PIXEL_PAGES 	8

// Pin config
#define SSD1306_RESET_PIN   	PB3
#define	SSD1306_DATA_INST   	PB1
#define	SSD1306_SELECT	    	PB4

// Commands
#define SSD1306_SET_COL_HI	0x10
#define SSD1306_SET_COL_LO	0x00
#define SSD1306_SET_LINE	0x40
#define SSD1306_SET_CONTRAST	0x81  // I_seg = contrast/(256*ref*scale_fac)
#define SSD1306_SET_SEG_REMAP0  0xA0
#define SSD1306_SET_SEG_REMAP1	0xA1
#define SSD1306_EON_OFF		0xA4
#define SSD1306_EON_ON		0xA5
#define SSD1306_DISP_NOR	0xA6
#define SSD1306_DISP_REV	0xA7
#define SSD1306_MULTIPLEX       0xA8
#define SSD1306_CHARGE_PUMP    	0x8D
#define SSD1306_PUMP_OFF    	0x10
#define SSD1306_PUMP_ON     	0x14
#define SSD1306_DISP_OFF 	0xAE
#define SSD1306_DISP_ON		0xAF
#define SSD1306_SET_PAGE	0xB0
#define SSD1306_SET_SCAN_FLIP	0xC0
#define SSD1306_SET_SCAN_NOR	0xC8
#define SSD1306_SET_OFFSET	0xD3
#define SSD1306_SET_RATIO_OSC	0xD5
#define SSD1306_SET_CHARGE  	0xD9 //p32
#define SSD1306_SET_PADS    	0xDA
#define SSD1306_SET_VCOM    	0xDB
#define SSD1306_NOP     	0xE3
#define SSD1306_SCROLL_RIGHT	0x26
#define SSD1306_SCROLL_LEFT	0x27
#define SSD1306_SCROLL_VR	0x29
#define SSD1306_SCROLL_VL	0x2A
#define SSD1306_SCROLL_OFF	0x2E
#define SSD1306_SCROLL_ON   	0x2F
#define SSD1306_VERT_SCROLL_A  	0xA3
#define SSD1306_MEM_ADDRESSING 	0x20
#define SSD1306_SET_COL_ADDR	0x21
#define SSD1306_SET_PAGE_ADDR	0x22

#define SSD1306_INVERT_TRUE 	1
#define SSD1306_INVERT_FALSE 	0

typedef struct SSD1306_CURSOR
{
	uint8_t dispx;
	uint8_t dispPage;
	uint8_t u8CursorX;
	uint8_t u8CursorY;
} SSD1306_CURSOR;

typedef struct SSD1306_FONT_DEF
{
	uint8_t u8Width;     			// Character width for storage
	uint8_t u8Height;  			    // Character height for storage
	const uint8_t *au8FontTable;	// Font table start address in memory
} SSD1306_FONT_DEF;

// Default settings
#define SSD1306_DEFAULT_CONTRAST 0x00

#define SSD1306_CURSOR_GOTO(x,y) { cursor.u8CursorX=(x); cursor.u8CursorY=(y); }

// Hardware specific
void ssd1306_write_data(uint8_t);
void ssd1306_write_instruction(uint8_t);
void ssd1306_reset_display(void);
void ssd1306_init_display (void);
void ssd1306_set_contrast(uint8_t contrast);
void ssd1306_clr_display(void);
void ssd1306_show_display(void);

// Graphics on buffer
void ssd1306_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void ssd1306_byte_to_buffer(uint8_t data);
void ssd1306_reset_buffer();
void ssd1306_set_pixel(uint8_t x, uint8_t y);
void ssd1306_image_to_buffer(const uint8_t *pointer);
void ssd1306_display_random(void);

// Fonts on buffer
void ssd1306_clock_char_to_buffer (uint8_t u8Char, uint8_t x, uint8_t width, uint8_t page_span);
void ssd1306_char_to_buffer(char u8Char, uint8_t Negative);
void ssd1306_libmono_char_to_buffer(char ch, uint8_t Negative);
uint8_t ssd1306_tiny_printp(uint8_t x, uint8_t y, const char *ptr);
uint8_t ssd1306_medium_printp(const uint8_t x, const uint8_t y, const char *text, const uint8_t invert_char);
void  ssd1306_padded_double_digit(uint8_t x, uint8_t y, uint8_t digit);

#endif
