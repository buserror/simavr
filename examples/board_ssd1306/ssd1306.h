/* ----------------------------------------------------------------------------
 SPI SSD1306 driver

 Copyright Gabriel Anzziani
 Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>
 */

#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

// Pin config
#define SSD1306_RESET_PIN   	PB3
#define	SSD1306_DATA_INST   	PB1
#define	SSD1306_CHIP_SELECT	PB4

#define SSD1306_X_PIXELS 	128
#define SSD1306_Y_PIXELS 	64
#define SSD1306_PIXEL_BYTES	1024
#define SSD1306_PIXEL_PAGES 	8

// Default settings
#define SSD1306_DEFAULT_CONTRAST 0xD0

// Commands
#define SSD1306_SET_COL_LO_NIBBLE	0x00
#define SSD1306_SET_COL_HI_NIBBLE	0x10
#define SSD1306_SET_LINE		0x40
#define SSD1306_SET_CONTRAST		0x81  // I_seg = contrast/(256*ref*scale_fac)#define SSD1306_SET_SEG_REMAP0  	0xA0#define SSD1306_SET_SEG_REMAP1		0xA1
#define SSD1306_RESUME_TO_RAM_CONTENT	0xA4
#define SSD1306_IGNORE_RAM_CONTENT	0xA5
#define SSD1306_DISP_NORMAL		0xA6
#define SSD1306_DISP_INVERTED		0xA7
#define SSD1306_MULTIPLEX       	0xA8
#define SSD1306_CHARGE_PUMP    		0x8D
#define SSD1306_PUMP_OFF    		0x10
#define SSD1306_PUMP_ON     		0x14
#define SSD1306_DISP_SLEEP 		0xAE
#define SSD1306_DISP_ON			0xAF
#define SSD1306_SET_PAGE_START_ADDR	0xB0
#define SSD1306_SET_SCAN_FLIP		0xC0
#define SSD1306_SET_SCAN_NOR		0xC8
#define SSD1306_SET_OFFSET		0xD3
#define SSD1306_SET_RATIO_OSC		0xD5
#define SSD1306_SET_CHARGE  		0xD9 // p32#define SSD1306_SET_PADS    		0xDA#define SSD1306_SET_VCOM    		0xDB
#define SSD1306_NOP     		0xE3
#define SSD1306_SCROLL_RIGHT		0x26
#define SSD1306_SCROLL_LEFT		0x27
#define SSD1306_SCROLL_VR		0x29
#define SSD1306_SCROLL_VL		0x2A
#define SSD1306_SCROLL_OFF		0x2E
#define SSD1306_SCROLL_ON   		0x2F
#define SSD1306_VERT_SCROLL_A  		0xA3
#define SSD1306_MEM_ADDRESSING 		0x20
#define SSD1306_SET_COL_ADDR		0x21
#define SSD1306_SET_PAGE_ADDR		0x22

typedef struct
{
  uint8_t disp_x;
  uint8_t disp_page;
  uint8_t x;
  uint8_t y;
} ssd1306_cursor_t;

typedef enum {NORMAL, INVERTED} display_mode_t;
typedef enum {ON, SLEEP} power_state_t;

extern ssd1306_cursor_t cursor_g;
extern uint8_t display_buffer[SSD1306_PIXEL_BYTES];

void
ssd1306_write_data (const uint8_t);
void
ssd1306_write_instruction (const uint8_t);
void
ssd1306_reset_display (void);
void
ssd1306_init_display (void);
void
ssd1306_set_contrast (const uint8_t contrast);
void
ssd1306_display_fb (void);
void
ssd1306_set_display_mode (const display_mode_t display_mode);
void
ssd1306_set_power_state (const power_state_t power_state);
void
ssd1306_set_pixel_direct (const uint8_t x, const uint8_t y);
void
ssd1306_set_byte (const uint8_t x, const uint8_t page, const uint8_t byte);
void
ssd1306_set_byte_fb (const uint8_t data);
void
ssd1306_set_pixel_fb (const uint8_t x, const uint8_t y);
void
ssd1306_clear_fb (void);

#endif
