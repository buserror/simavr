/*
	ssd1306.c

	Copyright Luki <humbell@ethz.ch>
	Copyright 2011 Michel Pollet <buserror@gmail.com>

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sim_time.h"

#include "ssd1306_virt.h"

void
ssd1306_print(
		ssd1306_t *b)
{
	printf("/******************\\\n");
	uint16_t page_offset = 0;
	for (int i = 0; i < b->pages; i++) {
		printf("| ");
		fwrite(b->vram + page_offset, 1, b->w, stdout);
		printf(" |\n");
		page_offset += 128;
	}
	printf("\\******************/\n");
}


static void
_ssd1306_reset_cursor(
    ssd1306_t *b)
{
  printf(">> RESET CURSOR\n");
	b->cursor = 0;
	//ssd1306_set_flag(b, HD44780_FLAG_DIRTY, 1);
	//avr_raise_irq(b->irq + IRQ_HD44780_ADDR, b->cursor);
}

static void
_ssd1306_clear_screen (ssd1306_t *b)
{
  printf(">> CLEAR SCREEN\n");
  // TODO: Get rid of these numbers
  memset (b->vram, ' ', 128 * 64);
  //hd44780_set_flag(b, HD44780_FLAG_DIRTY, 1);
  //avr_raise_irq(b->irq + IRQ_HD44780_ADDR, b->cursor);
}

/*
 * current data byte is ready in b->datapins
 */
static uint32_t
ssd1306_write_data(
		ssd1306_t *b)
{
  printf(">> WRITE DATA\n");
	uint32_t delay = 37; // uS
	/*
	b->vram[b->cursor] = b->datapins;
	printf("hd44780_write_data %02x\n", b->datapins);
	if (hd44780_get_flag(b, HD44780_FLAG_S_C)) {	// display shift ?
		// TODO display shift
	} else {
		hd44780_kick_cursor(b);
	}
	hd44780_set_flag(b, HD44780_FLAG_DIRTY, 1); */
	return delay;
}

/*
 * current command is ready in b->datapins
 */
static uint32_t
ssd1306_write_command(
		ssd1306_t *b)
{
	uint32_t delay = 37; // uS
	printf("ssd1306_write_command\n");
	/*
	int top = 7;	// get highest bit set'm
	while (top)
		if (b->datapins & (1 << top))
			break;
		else top--;
	printf("ssd1306_write_command %02x\n", b->datapins);
	switch (top) {
		// Set	DDRAM address
		case 7:		// 1 ADD ADD ADD ADD ADD ADD ADD
			b->cursor = b->datapins & 0x7f;
			break;
		// Set	CGRAM address
		case 6:		// 0 1 ADD ADD ADD ADD ADD ADD ADD
			b->cursor = 64 + (b->datapins & 0x3f);
			break;
		// Function	set
		case 5:	{	// 0 0 1 DL N F x x
			int four = !hd44780_get_flag(b, HD44780_FLAG_D_L);
			hd44780_set_flag(b, HD44780_FLAG_D_L, b->datapins & 16);
			hd44780_set_flag(b, HD44780_FLAG_N, b->datapins & 8);
			hd44780_set_flag(b, HD44780_FLAG_F, b->datapins & 4);
			if (!four && !hd44780_get_flag(b, HD44780_FLAG_D_L)) {
				printf("%s activating 4 bits mode\n", __FUNCTION__);
				hd44780_set_flag(b, HD44780_FLAG_LOWNIBBLE, 0);
			}
		}	break;
		// Cursor display shift
		case 4:		// 0 0 0 1 S/C R/L x x
			hd44780_set_flag(b, HD44780_FLAG_S_C, b->datapins & 8);
			hd44780_set_flag(b, HD44780_FLAG_R_L, b->datapins & 4);
			break;
		// Display on/off control
		case 3:		// 0 0 0 0 1 D C B
			hd44780_set_flag(b, HD44780_FLAG_D, b->datapins & 4);
			hd44780_set_flag(b, HD44780_FLAG_C, b->datapins & 2);
			hd44780_set_flag(b, HD44780_FLAG_B, b->datapins & 1);
			hd44780_set_flag(b, HD44780_FLAG_DIRTY, 1);
			break;
		// Entry mode set
		case 2:		// 0 0 0 0 0 1 I/D S
			hd44780_set_flag(b, HD44780_FLAG_I_D, b->datapins & 2);
			hd44780_set_flag(b, HD44780_FLAG_S, b->datapins & 1);
			break;
		// Return home
		case 1:		// 0 0 0 0 0 0 1 x
			_hd44780_reset_cursor(b);
			delay = 1520;
			break;
		// Clear display
		case 0:		// 0 0 0 0 0 0 0 1
			_hd44780_clear_screen(b);
			break;
	} */
	return delay;
}

/*
 * the E pin went low, and it's a write
 */
static uint32_t
ssd1306_process_write(
    ssd1306_t *b )
{
      printf(">> PROCESS WRITE\n");
	uint32_t delay = 0; // uS
	/*
	int four = !hd44780_get_flag(b, HD44780_FLAG_D_L);
	int comp = four && hd44780_get_flag(b, HD44780_FLAG_LOWNIBBLE);
	int write = 0;

	if (four) { // 4 bits !
		if (comp)
			b->datapins = (b->datapins & 0xf0) | ((b->pinstate >>  IRQ_HD44780_D4) & 0xf);
		else
			b->datapins = (b->datapins & 0xf) | ((b->pinstate >>  (IRQ_HD44780_D4-4)) & 0xf0);
		write = comp;
		b->flags ^= (1 << HD44780_FLAG_LOWNIBBLE);
	} else {	// 8 bits
		b->datapins = (b->pinstate >>  IRQ_HD44780_D0) & 0xff;
		write++;
	}
	avr_raise_irq(b->irq + IRQ_HD44780_DATA_IN, b->datapins);

	// write has 8 bits to process
	if (write) {
		if (hd44780_get_flag(b, HD44780_FLAG_BUSY)) {
			printf("%s command %02x write when still BUSY\n", __FUNCTION__, b->datapins);
		}
		if (b->pinstate & (1 << IRQ_HD44780_RS))	// write data
			delay = hd44780_write_data(b);
		else										// write command
			delay = hd44780_write_command(b);
	}*/
	return delay;
}


static avr_cycle_count_t
_ssd1306_process_e_pinchange(
		struct avr_t * avr,
        avr_cycle_count_t when, void * param)
{
  printf(">> PROCESS PIN CHANGE\n");
  /*ssd1306_t *b = (ssd1306_t *) param;


  ssd1306_set_flag(b, HD44780_FLAG_REENTRANT, 1);

#if 0
	uint16_t touch = b->oldstate ^ b->pinstate;
	printf("LCD: %04x %04x %c %c %c %c\n", b->pinstate, touch,
			b->pinstate & (1 << IRQ_HD44780_RW) ? 'R' : 'W',
			b->pinstate & (1 << IRQ_HD44780_RS) ? 'D' : 'C',
			hd44780_get_flag(b, HD44780_FLAG_LOWNIBBLE) ? 'L' : 'H',
			hd44780_get_flag(b, HD44780_FLAG_BUSY) ? 'B' : ' ');
#endif
	int delay = 0; // in uS

	if (b->pinstate & (1 << IRQ_HD44780_RW))	// read !?!
		delay = hd44780_process_read(b);
	else										// write
		delay = hd44780_process_write(b);

	if (delay) {
		hd44780_set_flag(b, HD44780_FLAG_BUSY, 1);
		avr_raise_irq(b->irq + IRQ_HD44780_BUSY, 1);
		avr_cycle_timer_register_usec(b->avr, delay,
			_hd44780_busy_timer, b);
	}
//	b->oldstate = b->pinstate;
	hd44780_set_flag(b, HD44780_FLAG_REENTRANT, 0); */
	return 0;
}

/*
 * Called when a SPI byte is sent
 */
static void ssd1306_spi_in_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
  printf(">> SPI IN\n");
	//ssd1306_t * p = (ssd1306_t*)param;
	// send "old value" to any chained one..
	//avr_raise_irq(p->irq + IRQ_HC595_SPI_BYTE_OUT, p->value);
	//p->value = (p->value << 8) | (value & 0xff);
}

/*
 * Called when a RESET signal is sent
 */
static void ssd1306_reset_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
  printf(">> RESET\n");
	//ssd1306_t * p = (ssd1306_t*)param;
	//if (irq->value && !value) 	// falling edge
	  //memset(p, 0, sizeof(*p));????
}

static const char * irq_names[IRQ_SSD1306_COUNT] =
  {   [IRQ_SSD1306_ALL] = "7=ssd1306.pins",
      [IRQ_SSD1306_RESET ] = "<ssd1306.RS",
      [IRQ_SSD1306_DATA_INSTRUCTION] = "<ssd1306.RW",
      [IRQ_SSD1306_ENABLE] = "<ssd1306.E",
      [IRQ_SSD1306_SPI_BYTE_IN] = "=ssd1306.SDIN"};

void
ssd1306_init(
		struct avr_t *avr,
		struct ssd1306_t * b,
		int width,
		int height )
{
	memset(b, 0, sizeof(*b));
	b->avr = avr;
	b->w = width;
	b->h = height;
	b->pages = 8;
	/*
	 * Register callbacks on all our IRQs
	 */
	b->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_SSD1306_COUNT, irq_names);

	avr_irq_register_notify(b->irq + IRQ_SSD1306_SPI_BYTE_IN, ssd1306_spi_in_hook, b);
	avr_irq_register_notify(b->irq + IRQ_SSD1306_RESET, ssd1306_reset_hook, b);

	_ssd1306_reset_cursor(b);
	_ssd1306_clear_screen(b);

	printf("SSD1306: %duS is %d cycles for your AVR\n",
			37, (int)avr_usec_to_cycles(avr, 37));
	printf("SSD1306: %duS is %d cycles for your AVR\n",
			1, (int)avr_usec_to_cycles(avr, 1));
}

