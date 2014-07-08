/* ----------------------------------------------------------------------------
 128x64 Graphic LCD management for SSD1306 driver

 Copyright Gabriel Anzziani
 Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>

 */

#include <avr/io.h>
#include <util/delay.h>

#include "ssd1306.h"

ssd1306_cursor_t cursor_g = { };
uint8_t display_buffer[SSD1306_PIXELS];

void
ssd1306_reset_display (void)
{
  _delay_us (3);
  PORTB &= ~(1 << SSD1306_RESET_PIN);
  _delay_us (3);
  PORTB |= (1 << SSD1306_RESET_PIN);
}

void
ssd1306_init_display (void)
{
  ssd1306_reset_display ();

  // Recommended init sequence
  ssd1306_write_instruction (SSD1306_DISP_OFF);
  ssd1306_write_instruction (SSD1306_SET_RATIO_OSC);
  ssd1306_write_instruction (0x80);
  ssd1306_write_instruction (SSD1306_MULTIPLEX);
  ssd1306_write_instruction (0x3F);
  ssd1306_write_instruction (SSD1306_SET_OFFSET);
  ssd1306_write_instruction (0x00);
  ssd1306_write_instruction (SSD1306_SET_LINE);
  ssd1306_write_instruction (SSD1306_CHARGE_PUMP);
  ssd1306_write_instruction (SSD1306_PUMP_ON);
  ssd1306_write_instruction (SSD1306_SET_SEG_REMAP1);
  ssd1306_write_instruction (SSD1306_SET_SCAN_NOR);
  ssd1306_write_instruction (SSD1306_SET_PADS);
  ssd1306_write_instruction (0x12);
  ssd1306_set_contrast (SSD1306_DEFAULT_CONTRAST);
  ssd1306_write_instruction (SSD1306_SET_CHARGE);
  ssd1306_write_instruction (0xF1);
  ssd1306_write_instruction (SSD1306_SET_VCOM);
  ssd1306_write_instruction (0x40);
  ssd1306_write_instruction (SSD1306_EON_OFF);
  ssd1306_write_instruction (SSD1306_DISP_NOR);
  ssd1306_write_instruction (SSD1306_MEM_ADDRESSING);
  // Horizontal Addressing mode
  ssd1306_write_instruction (0x00);
  ssd1306_write_instruction (SSD1306_DISP_ON);
}

void
ssd1306_set_contrast (uint8_t contrast)
{
  ssd1306_write_instruction (SSD1306_SET_CONTRAST);
  ssd1306_write_instruction (contrast);
}

void
ssd1306_write_data (unsigned char byte)
{
  PORTB |= (1 << SSD1306_DATA_INST);
  PORTB &= ~(1 << SSD1306_SELECT);
  SPDR = byte;
  while (!(SPSR & (1 << SPIF)))
    ; 	// Wait for transmission complete
  PORTB |= (1 << SSD1306_SELECT);
}

void
ssd1306_write_instruction (unsigned char byte)
{
  PORTB &= ~((1 << SSD1306_DATA_INST) | (1 << SSD1306_SELECT));
  SPDR = byte;
  while (!(SPSR & (1 << SPIF)))
    ; 	// Wait for transmission complete
  PORTB |= (1 << SSD1306_SELECT);
}

void
ssd1306_clr_display (void)
{
  // TODO Use memset
  uint16_t i;
  for (i = 0; i < SSD1306_PIXELS; i++)
    display_buffer[i] = 0;
}

// Transfer display buffer to LCD
void
ssd1306_show_display (void)
{
  uint8_t page, column;

  ssd1306_write_instruction (SSD1306_SET_PAGE);
  ssd1306_write_instruction (SSD1306_SET_COL_HI);
  ssd1306_write_instruction (SSD1306_SET_COL_LO);

  uint8_t * display_cursor = display_buffer;

  for (page = 0; page < 8; page++)
    {
      for (column = 0; column < SSD1306_X_PIXEL_ROWS; column++)
	{
	  ssd1306_write_data (*display_cursor++);
	}
    }
}

void
ssd1306_set_pixel (uint8_t x, uint8_t y)
{
  display_buffer[((uint16_t) (y << 4) & 0xFF80) + x] |= (uint8_t) (0x01
      << (y & 0x07));
}

// Write byte on display buffer
void
ssd1306_byte_to_buffer (uint8_t data)
{
  display_buffer[((uint16_t) (cursor_g.dispPage << 7)) + (cursor_g.dispx++)] |=
      data;
}
// Write 0 on display buffer
void
ssd1306_reset_buffer ()
{
  display_buffer[((uint16_t) (cursor_g.dispPage << 7)) + (cursor_g.dispx++)] = 0;
}
