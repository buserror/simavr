/*
	atmega644_hd44780.c

	Firmware side of the HD44780 read-back regression test.

	Drives the LCD in 4-bit mode and polls the busy flag before every
	byte, i.e. it actually uses the read path of the part. Wiring is
	the one from examples/board_hd44780: D4..D7 on PB0..PB3
	(bidirectional), RS on PB4, E on PB5, R/W on PB6.

	Copyright 2026 Felix Mertins

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

#ifndef F_CPU
#define F_CPU 8000000
#endif

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega644");

#define LCD_DATA	0x0F	/* D4..D7 on PB0..PB3 */
#define LCD_RS		(1 << 4)
#define LCD_E		(1 << 5)
#define LCD_RW		(1 << 6)

static void
lcd_strobe(void)
{
	PORTB |= LCD_E;
	_delay_us(2);
	PORTB &= (uint8_t)~LCD_E;
	_delay_us(2);
}

/* One status read: returns the high nibble (busy flag in bit 3 of the
 * port reading = DB7), then strobes a second time to discard the low
 * nibble, as every 4-bit driver has to. */
static uint8_t
lcd_status_high(void)
{
	uint8_t v;

	DDRB &= (uint8_t)~LCD_DATA;		/* release the bus */
	PORTB = (uint8_t)((PORTB & ~(LCD_RS | LCD_RW | LCD_DATA))
			| LCD_RW | 0x07);	/* RS=0, R/W=1, pull-ups on D4..D6 */
	PORTB |= LCD_E;
	_delay_us(2);
	v = (uint8_t)(PINB & LCD_DATA);
	PORTB &= (uint8_t)~LCD_E;
	_delay_us(2);
	lcd_strobe();				/* discard low nibble */
	return v;
}

static void
lcd_wait_busy(void)
{
	while (lcd_status_high() & 0x08)
		;
}

static void
lcd_out4(uint8_t rs, uint8_t nibble)
{
	uint8_t v = (uint8_t)(nibble & LCD_DATA);

	if (rs)
		v |= LCD_RS;
	/* drive first, then set the data: the order real drivers use.
	 * Writing PORT while the pins are still inputs would toggle the
	 * pull-ups and generate extra edges that mask the stale-cache
	 * defect this test exists to catch. */
	DDRB |= LCD_DATA;
	PORTB = (uint8_t)((PORTB & ~(LCD_DATA | LCD_RS | LCD_RW)) | v);
	lcd_strobe();
}

static void
lcd_write(uint8_t rs, uint8_t byte)
{
	lcd_wait_busy();
	lcd_out4(rs, (uint8_t)(byte >> 4));
	lcd_out4(rs, (uint8_t)(byte & 0x0F));
}

static void
lcd_puts(const char *s)
{
	while (*s)
		lcd_write(1, (uint8_t)*s++);
}

int
main(void)
{
	DDRB = LCD_DATA | LCD_RS | LCD_E | LCD_RW;
	PORTB = 0;

	/* datasheet power-on dance: three timed 8-bit function sets,
	 * then switch to 4-bit; only after that is busy readable */
	_delay_ms(50);
	lcd_out4(0, 0x3); _delay_ms(5);
	lcd_out4(0, 0x3); _delay_ms(5);
	lcd_out4(0, 0x3); _delay_ms(5);
	lcd_out4(0, 0x2); _delay_ms(5);

	lcd_write(0, 0x28);	/* function set: 4 bit, 2 lines */
	lcd_write(0, 0x0C);	/* display on */
	lcd_write(0, 0x01);	/* clear */
	lcd_write(0, 0x06);	/* entry mode: increment */

	lcd_write(0, 0x80);	/* DDRAM line 1 */
	lcd_puts("READBACK 4BIT");
	lcd_write(0, 0xC0);	/* DDRAM line 2 */
	lcd_puts("SOFT RESET");

	/* done: sleep with interrupts off ends the simulation */
	cli();
	sleep_mode();
	return 0;
}
