/* ----------------------------------------------------------------------------
 128x64 Graphic LCD management for SSD1306 driver

 AUTHOR		: Gabriel Anzziani
 MODIFED BY  : Doug Szumski
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "ssd1306.h"
#include "fonts.h"

uint8_t display_buffer[SSD1306_PIXELS];

/* Global variables */
struct SSD1306_FONT_DEF Font_3x6 =
{ 3, 6, tiny_font };
struct SSD1306_FONT_DEF Font_11x16 =
{ 11, 16, liberation_mono };
struct SSD1306_FONT_DEF *font;

SSD1306_CURSOR cursor = {};

void ssd1306_reset_display(void)
{
	_delay_us(3);
	PORTB &= ~(1 << SSD1306_RESET_PIN);
	_delay_us(3);
	PORTB |= (1 << SSD1306_RESET_PIN);
}

void ssd1306_init_display(void)
{
	ssd1306_reset_display();

	// Recommended init sequence
	ssd1306_write_instruction(SSD1306_DISP_OFF);
	ssd1306_write_instruction(SSD1306_SET_RATIO_OSC);
	ssd1306_write_instruction(0x80);
	ssd1306_write_instruction(SSD1306_MULTIPLEX);
	ssd1306_write_instruction(0x3F);
	ssd1306_write_instruction(SSD1306_SET_OFFSET);
	ssd1306_write_instruction(0x00);
	ssd1306_write_instruction(SSD1306_SET_LINE);
	ssd1306_write_instruction(SSD1306_CHARGE_PUMP);
	ssd1306_write_instruction(SSD1306_PUMP_ON);
	ssd1306_write_instruction(SSD1306_SET_SEG_REMAP1);
	ssd1306_write_instruction(SSD1306_SET_SCAN_NOR);
	ssd1306_write_instruction(SSD1306_SET_PADS);
	ssd1306_write_instruction(0x12);
	ssd1306_set_contrast(SSD1306_DEFAULT_CONTRAST);
	ssd1306_write_instruction(SSD1306_SET_CHARGE);
	ssd1306_write_instruction(0xF1);
	ssd1306_write_instruction(SSD1306_SET_VCOM);
	ssd1306_write_instruction(0x40);
	ssd1306_write_instruction(SSD1306_EON_OFF);
	ssd1306_write_instruction(SSD1306_DISP_NOR);
	ssd1306_write_instruction(SSD1306_MEM_ADDRESSING);
	// Horizontal Addressing mode
	ssd1306_write_instruction(0x00);
	ssd1306_write_instruction(SSD1306_DISP_ON);
}

void ssd1306_set_contrast(uint8_t contrast)
{
	ssd1306_write_instruction(SSD1306_SET_CONTRAST);
	ssd1306_write_instruction(contrast);
}

void ssd1306_image_to_buffer(const uint8_t * image_data)
{
	uint8_t page, column;
	uint8_t image_byte = 0;
	uint8_t count = 0;
	uint16_t buf_indx = 0;

	for (page = 0; page < 8; page++)
	{
		for (column = 0; column < SSD1306_X_PIXEL_ROWS; column++)
		{
			if (count == 0)
			{
				image_byte = pgm_read_byte_near( image_data++);
				if (image_byte == pgm_read_byte_near( image_data++))
				{
					count = pgm_read_byte_near( image_data++);
				}
				else
				{
					count = 1;
					 image_data--;
				}
			}
			count--;
			buf_indx = page * SSD1306_X_PIXEL_ROWS + column;
			display_buffer[buf_indx] = image_byte;
		}
	}
}

void ssd1306_clock_char_to_buffer(uint8_t large_char, uint8_t x, uint8_t width,
		uint8_t page_span)
{
	uint8_t page, column;
	uint8_t data = 0;
	uint8_t count = 0;

	const uint8_t *pointer;
	pointer = time_font[large_char];
	for (page = 0; page < page_span; page++)
	{
		for (column = x; column < width + x; column++)
		{
			if (count == 0)
			{
				data = pgm_read_byte_near(pointer++);
				if (data == pgm_read_byte_near(pointer++))
				{
					count = pgm_read_byte_near(pointer++);
				}
				else
				{
					count = 1;
					pointer--;
				}
			}
			count--;
			display_buffer[page * SSD1306_X_PIXEL_ROWS + column] = ~data;
		}
	}
}

void ssd1306_display_random(void)
{
	uint16_t i;
	uint8_t *p;
	p = &display_buffer[0];
	for (i = 0; i < SSD1306_PIXELS; i++)
	{
		*p++ = rand() % 255;
	}
}

void ssd1306_write_data(unsigned char byte)
{
	//Pull Data/Command control pin high
	PORTB |= (1 << SSD1306_DATA_INST);
	//Pull down CS
	PORTB &= ~(1 << SSD1306_SELECT);
	SPDR = byte;					//Load byte to Data register
	while (!(SPSR & (1 << SPIF)))
		; 	// Wait for transmission complete
	//Set CS high again
	PORTB |= (1 << SSD1306_SELECT);
}

void ssd1306_write_instruction(unsigned char byte)
{
	//Pull Data/Command control and CS pins low
	PORTB &= ~((1 << SSD1306_DATA_INST) | (1 << SSD1306_SELECT));
	SPDR = byte;					//Load byte to Data register
	while (!(SPSR & (1 << SPIF)))
		; 	// Wait for transmission complete
	//Set CS high again
	PORTB |= (1 << SSD1306_SELECT);
}

// Clear display buffer
void ssd1306_clr_display(void)
{
	uint16_t i;
	for (i = 0; i < SSD1306_PIXELS; i++)
		display_buffer[i] = 0;
}

// Transfer display buffer to LCD
void ssd1306_show_display(void)
{
	uint8_t page, column;

	ssd1306_write_instruction(SSD1306_SET_PAGE);
	ssd1306_write_instruction(SSD1306_SET_COL_HI);
	ssd1306_write_instruction(SSD1306_SET_COL_LO);

	uint8_t * display_cursor = display_buffer;

	for (page = 0; page < 8; page++)
	{
		for (column = 0; column < SSD1306_X_PIXEL_ROWS; column++)
		{
			ssd1306_write_data(*display_cursor++);
		}
	}
}

// Set pixel on display buffer
void ssd1306_set_pixel(uint8_t x, uint8_t y)
{
	display_buffer[((uint16_t) (y << 4) & 0xFF80) + x] |= (uint8_t) (0x01
			<< (y & 0x07));
}

// Write byte on display buffer
void ssd1306_byte_to_buffer(uint8_t data)
{
	display_buffer[((uint16_t) (cursor.dispPage << 7)) + (cursor.dispx++)] |= data;
}
// Write 0 on display buffer
void ssd1306_reset_buffer()
{
	display_buffer[((uint16_t) (cursor.dispPage << 7)) + (cursor.dispx++)] = 0;
}

void ssd1306_line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
	uint8_t i, dxabs, dyabs, x, y;
	int8_t dx, dy, stepx, stepy;
	dx = (int8_t) x2 - x1;      // the horizontal distance of the line
	dy = (int8_t) y2 - y1;      // the vertical distance of the line
	if (dy < 0)
	{
		dyabs = -dy;
		stepy = -1;
	}
	else
	{
		dyabs = dy;
		stepy = 1;
	}
	if (dx < 0)
	{
		dxabs = -dx;
		stepx = -1;
	}
	else
	{
		dxabs = dx;
		stepx = 1;
	}
	x = (uint8_t) (dyabs >> 1);
	y = (uint8_t) (dxabs >> 1);
	ssd1306_set_pixel(x1, y1);
	if (dxabs >= dyabs)
	{ // the line is more horizontal than vertical
		for (i = 0; i < dxabs; i++)
		{
			y += dyabs;
			if (y >= dxabs)
			{
				y -= dxabs;
				y1 += stepy;
			}
			x1 += stepx;
			ssd1306_set_pixel(x1, y1);
		}
	}
	else
	{  // the line is more vertical than horizontal
		for (i = 0; i < dyabs; i++)
		{
			x += dxabs;
			if (x >= dyabs)
			{
				x -= dyabs;
				x1 += stepx;
			}
			y1 += stepy;
			ssd1306_set_pixel(x1, y1);
		}
	}
}


void ssd1306_char_to_buffer(char character, uint8_t invert_char)
{
	uint16_t pointer;
	uint8_t data, char_column = 0;

	//Use the small font
	font = &Font_3x6;
	cursor.dispx = cursor.u8CursorX;
	if (character == '\n')
	{	// New line
		cursor.u8CursorX = 0;
		cursor.u8CursorY++;
		return;
	}
	if (character == '.' && font != &Font_3x6)
	{		// Small point to Save space
		ssd1306_byte_to_buffer(96);
		ssd1306_byte_to_buffer(96);
		cursor.u8CursorX += 2;
	}
	else
	{
		if (character >= 'a')
			character -= ('a' - 'A');
		pointer = (unsigned int) (font->au8FontTable)
				+ (character - 32) * (font->u8Width);
		/* Draw a char */
		while (char_column < (font->u8Width))
		{
			cursor.dispPage = cursor.u8CursorY;
			data = pgm_read_byte_near(pointer++);
			if (invert_char)
				data = ~data;
			ssd1306_byte_to_buffer(data);
			char_column++;
			cursor.u8CursorX++;
		}
	}
	if (character == '/')
	{
		ssd1306_byte_to_buffer(0x70);
		cursor.u8CursorX++;
	}
	else if (character == '(')
	{
		ssd1306_byte_to_buffer(0x70);
		cursor.u8CursorX += 2;
	}
	else if (character == '&')
	{
		ssd1306_byte_to_buffer(0x10);
		cursor.u8CursorX++;
	}
	else if (cursor.u8CursorX < 128)
	{
		cursor.dispPage = cursor.u8CursorY; // Select the page of the LCD
		cursor.dispx = cursor.u8CursorX;
		data = 0;
		if (invert_char)
			data = 255;
		ssd1306_byte_to_buffer(data); 	// if not then insert a space before next letter
		cursor.u8CursorX++;
	}
}

void ssd1306_libmono_char_to_buffer(char character, uint8_t Negative)
{
	//Draw a character spanning two pages
	uint8_t i = 0;
	uint8_t data;
	uint16_t pointer;
	//Use medium font
	font = &Font_11x16;
	//'CAPS LOCK' as font table doesn't have lowercase
	if (character >= 'a' && character <= 'z')
	{
		character -= ('a' - 'A');
	}
	//Offset ASCII char to match font table segment start (32)
	if (character >= ' ' && character <= '}')
	{
		character -= 32;
	}
	//Deal with the non-contiguous chars
	else if (character == '~')
	{
		//Shifting formula is ASCII position of tilda (126) -
		//font segment length (font_segment_end - start + 1)
		character -= (126 - (93 - 32 + 1));
	}
	//Start address of font table, 22 (font->u8Width*2) is length of each character
	pointer = (unsigned int) (font->au8FontTable)
			+ (character) * (font->u8Width * 2);
	//Draw the character in two halves as it spans two pages
	cursor.dispPage = cursor.u8CursorY;
	cursor.dispx = cursor.u8CursorX;
	while (i < font->u8Width)
	{
		data = pgm_read_byte_near(pointer++);
		if (Negative)
			data = ~data;
		ssd1306_byte_to_buffer(data);
		i++;
	}
	//Draw the second half
	i = 0;
	cursor.dispPage = cursor.u8CursorY + 1;
	cursor.dispx = cursor.u8CursorX;
	while (i < font->u8Width)
	{
		data = pgm_read_byte_near(pointer++);
		if (Negative)
			data = ~data;
		ssd1306_byte_to_buffer(data);
		i++;
	}
	cursor.u8CursorX += font->u8Width;
}


// Print small font text from program memory
uint8_t ssd1306_tiny_printp(uint8_t x, uint8_t y, const char *ptr)
{
	SSD1306_CURSOR_GOTO(x * 8, y / 8);
	uint8_t n = 0;
	while (pgm_read_byte(ptr) != 0x00)
	{
		ssd1306_char_to_buffer(pgm_read_byte(ptr++), SSD1306_INVERT_FALSE);
		n++;
	}
	return n;
}

uint8_t ssd1306_medium_printp(const uint8_t x, const uint8_t y, const char *text, const uint8_t invert_char)
{
	SSD1306_CURSOR_GOTO(x * font->u8Width, y);
	uint8_t n = 0;
	while (pgm_read_byte(text) != 0x00)
	{
		ssd1306_libmono_char_to_buffer(pgm_read_byte(text++), invert_char);
		n++;
	}

	return n;
}

/* Prints a number in format ## */
void ssd1306_padded_double_digit(uint8_t x, uint8_t y, uint8_t digit)
{
	uint8_t hundreds = 0x30;
	uint8_t tens = 0x30; //0x32;
	uint8_t units = 0x30;
	//Shift digit to match ASCII position in font table
	while (digit > 99)
	{
		digit -= 100;
		hundreds++;
	}
	while (digit > 9)
	{
		digit -= 10;
		tens++;
	}
	while (digit > 0)
	{
		digit--;
		units++;
	}

	SSD1306_CURSOR_GOTO(x, y);
	if (hundreds > 0x30)
	{
		ssd1306_libmono_char_to_buffer(hundreds, SSD1306_INVERT_FALSE);
		x += font->u8Width;
		SSD1306_CURSOR_GOTO(x, y);
	}
	ssd1306_libmono_char_to_buffer(tens, SSD1306_INVERT_FALSE);
	x += font->u8Width;
	SSD1306_CURSOR_GOTO(x, y);
	ssd1306_libmono_char_to_buffer(units, SSD1306_INVERT_FALSE);

}

