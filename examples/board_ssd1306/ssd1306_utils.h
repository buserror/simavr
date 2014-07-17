/* ----------------------------------------------------------------------------
 128x64 Graphic LCD management for SSD1306 driver

 Copyright Gabriel Anzziani
 Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>

 */
#ifndef SSD1306_UTILS_H_
#define SSD1306_UTILS_H_

#include "ssd1306.h"
#include "fonts.h"

#define SSD1306_INVERT_TRUE 	1
#define SSD1306_INVERT_FALSE 	0

typedef struct
{
	uint8_t width;
	uint8_t height;
	const uint8_t *table;
} ssd1306_font_t;

extern ssd1306_cursor_t cursor_g;

// Graphics on buffer
void ssd1306_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
void ssd1306_image_to_buffer(const uint8_t *pointer);
void ssd1306_display_random(void);

// Fonts on buffer
void ssd1306_clock_char_to_buffer (uint8_t u8Char, uint8_t x, uint8_t width, uint8_t page_span);
void ssd1306_char_to_buffer(char u8Char, uint8_t Negative);
void ssd1306_libmono_char_to_buffer(char ch, uint8_t Negative);
uint8_t ssd1306_tiny_printp(uint8_t x, uint8_t y, const char *ptr);
uint8_t ssd1306_medium_printp(const uint8_t x, const uint8_t y, const char *text, const uint8_t invert_char);
void  ssd1306_padded_double_digit(uint8_t x, uint8_t y, uint8_t digit);

#endif /* SSD1306_UTILS_H_ */
