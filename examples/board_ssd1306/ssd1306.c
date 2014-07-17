/* ----------------------------------------------------------------------------
 128x64 Graphic LCD management for SSD1306 driver

 Copyright Gabriel Anzziani
 Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>

 */
#include <string.h>

#include <avr/io.h>
#include <util/delay.h>

#include "ssd1306.h"

uint8_t display_buffer[SSD1306_PIXEL_BYTES];

ssd1306_cursor_t cursor_g = { };

void
ssd1306_reset_display (void)
{
  PORTB |= (1 << SSD1306_RESET_PIN);
  _delay_us (3);
  PORTB &= ~(1 << SSD1306_RESET_PIN);
  _delay_us (3);
  PORTB |= (1 << SSD1306_RESET_PIN);
}

static inline void ssd1306_tx_spi_byte(const uint8_t byte)
{
  SPDR = byte;
  // Wait for transmission complete
  while (!(SPSR & (1 << SPIF)));
}

void
ssd1306_write_data (const uint8_t byte)
{
  PORTB |= (1 << SSD1306_DATA_INST);
  PORTB &= ~(1 << SSD1306_CHIP_SELECT);
  ssd1306_tx_spi_byte(byte);
  PORTB |= (1 << SSD1306_CHIP_SELECT);
}

void
ssd1306_write_instruction (const uint8_t byte)
{
  PORTB &= ~((1 << SSD1306_DATA_INST) | (1 << SSD1306_CHIP_SELECT));
  ssd1306_tx_spi_byte(byte);
  PORTB |= (1 << SSD1306_CHIP_SELECT);
}

void
ssd1306_init_display (void)
{
  ssd1306_reset_display ();

  // Recommended init sequence
  ssd1306_set_power_state(SLEEP);

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

  ssd1306_write_instruction (SSD1306_RESUME_TO_RAM_CONTENT);

  ssd1306_set_display_mode(NORMAL);

  ssd1306_write_instruction (SSD1306_MEM_ADDRESSING);
  ssd1306_write_instruction (0x00); 			// Horizontal Addressing mode

  ssd1306_set_power_state(ON);
}

void
ssd1306_set_contrast (const uint8_t contrast)
{
  ssd1306_write_instruction (SSD1306_SET_CONTRAST);
  ssd1306_write_instruction (contrast);
}

void
ssd1306_set_display_mode(const display_mode_t display_mode)
{
  switch (display_mode) {
    case NORMAL:
      ssd1306_write_instruction (SSD1306_DISP_NORMAL);
      break;
    case INVERTED:
      ssd1306_write_instruction (SSD1306_DISP_INVERTED);
      break;
    default:
      ssd1306_write_instruction (SSD1306_DISP_NORMAL);
      break;
  }
}

void
ssd1306_set_power_state (const power_state_t power_state)
{
  switch (power_state)
    {
    case ON:
      ssd1306_write_instruction (SSD1306_DISP_ON);
      break;
    case SLEEP:
      ssd1306_write_instruction (SSD1306_DISP_SLEEP);
      break;
    default:
      break;
    }
}

void
ssd1306_set_byte (const uint8_t x, const uint8_t page, const uint8_t byte)
{
  ssd1306_write_instruction (SSD1306_SET_PAGE_START_ADDR | page);
  ssd1306_write_instruction (SSD1306_SET_COL_LO_NIBBLE | (x & 0xF));
  ssd1306_write_instruction (SSD1306_SET_COL_HI_NIBBLE | (x >> 4));
  ssd1306_write_data(byte);
}

/*  Transfer display buffer to LCD */
void
ssd1306_display_fb (void)
{
  ssd1306_write_instruction (SSD1306_SET_PAGE_START_ADDR);
  ssd1306_write_instruction (SSD1306_SET_COL_HI_NIBBLE);
  ssd1306_write_instruction (SSD1306_SET_COL_LO_NIBBLE);

  const uint8_t * display_cursor = display_buffer;

  for (uint8_t page = 0; page < SSD1306_PIXEL_PAGES; page++)
    {
      for (uint8_t column = 0; column < SSD1306_X_PIXELS; column++)
	{
	  ssd1306_write_data (*display_cursor++);
	}
    }
}

void
ssd1306_clear_fb (void)
{
  memset(display_buffer, 0, SSD1306_PIXEL_BYTES);
}

void
ssd1306_set_pixel_fb (const uint8_t x, const uint8_t y)
{
  /* The assembly generated from the below is slower. Use the cryptic version */
  //display_buffer[(y / SSD1306_PIXEL_PAGES) * SSD1306_X_PIXELS + x] |= (1 << y % SSD1306_PIXEL_PAGES);
  display_buffer[((y >> 3) << 7) + x] |= (1 << y % SSD1306_PIXEL_PAGES);
  //display_buffer[((y << 4) & 0xFF80) + x] |= (1 << y % SSD1306_PIXEL_PAGES);
}

void
ssd1306_set_byte_fb (const uint8_t byte)
{
  display_buffer[(cursor_g.disp_page << 7) + cursor_g.disp_x++] |= byte;
}
