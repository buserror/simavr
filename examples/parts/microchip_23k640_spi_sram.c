/*
	microchip_23k640_spi_sram.c

	Copyright 2014 Michael Hughes <squirmyworms@embarqmail.com>

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

/*
 *	Implementation of Microchip 8K SPI SRAM, part 23K640.
 *		Based on data sheet specifications.
 */

#include <string.h>

#include "sim_avr.h"
#include "sim_core.h"
#include "avr_spi.h"

#include "microchip_23k640_spi_sram.h"

#define MICROCHIP_INSTRUCTION_READ 0X03
#define MICROCHIP_INSTRUCTION_WRITE 0X02

#define MICROCHIP_INSTRUCTION_RDSR 0X05
#define MICROCHIP_INSTRUCTION_WRSR 0X01

#define MICROCHIP_MODE_PAGED 0x2
#define MICROCHIP_MODE_SEQUENTIAL 0x1

enum {
	MICROCHIP_STATE_IDLE = 0,
	MICROCHIP_STATE_ADDRESS_FETCH_HOB,
	MICROCHIP_STATE_ADDRESS_FETCH_LOB,
	MICROCHIP_STATE_PROCESS_INSTRUCTION
};

static void
microchip_23k640_in(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	microchip_23k640_t * part = param;
	
	if(!part->cs || (part->hold & !part->hold_disable)) {
		return;
	}
	
	switch(part->state) {
		case	MICROCHIP_STATE_IDLE:
			part->instruction = value;
			switch(part->instruction) {
				case MICROCHIP_INSTRUCTION_RDSR:
				case MICROCHIP_INSTRUCTION_WRSR:
					part->state = MICROCHIP_STATE_PROCESS_INSTRUCTION;
					break;
				case MICROCHIP_INSTRUCTION_READ:
				case MICROCHIP_INSTRUCTION_WRITE:
					part->state = MICROCHIP_STATE_ADDRESS_FETCH_HOB;
					break;
			}
			break;
		case	MICROCHIP_STATE_ADDRESS_FETCH_HOB:
			part->address = (value & 0xff) << 8;
			part->state = MICROCHIP_STATE_ADDRESS_FETCH_LOB;
			break;
		case	MICROCHIP_STATE_ADDRESS_FETCH_LOB:
			part->address |= (value & 0xff);
			part->state = MICROCHIP_STATE_PROCESS_INSTRUCTION;
			break;
		case	MICROCHIP_STATE_PROCESS_INSTRUCTION:
			switch(part->instruction) {
				case	MICROCHIP_INSTRUCTION_READ: {
						uint8_t data = part->data[part->address & (8192 -1)];
						avr_raise_irq(part->irq + IRQ_MICROCHIP_23K640_SPI_BYTE_OUT, data);
					} break;
				case	MICROCHIP_INSTRUCTION_WRITE:
					part->data[part->address & (8192 - 1)] = value;
					break;
				case	MICROCHIP_INSTRUCTION_RDSR: {
						/* data sheet says bit 2 is set on reading status register. */
						uint8_t status = ((part->mode & 0x03) << (7 - 2)) | 0x02 | (part->hold_disable ? 1 : 0);
						avr_raise_irq(part->irq + IRQ_MICROCHIP_23K640_SPI_BYTE_OUT, status);
						part->state = MICROCHIP_STATE_IDLE;
					} break;
				case	MICROCHIP_INSTRUCTION_WRSR:
					part->mode = (value >> (8 - 2)) & 3;
					part->hold_disable = value & 1;
					part->state = MICROCHIP_STATE_IDLE;
					break;
				default:
					part->state = MICROCHIP_STATE_IDLE;
					break;
		}			
	}
	
	if((MICROCHIP_STATE_PROCESS_INSTRUCTION == part->state) &&
		((MICROCHIP_INSTRUCTION_READ == part->instruction) ||
			(MICROCHIP_INSTRUCTION_WRITE == part->instruction))) {
		switch(part->mode) {
			case MICROCHIP_MODE_SEQUENTIAL: /* sequential read/write operation */
				part->address++;
				break;
			case MICROCHIP_MODE_PAGED: /* paged read/write operation */
				part->state = ((part->address & 0x1f) ? part->state : 0);
				part->address = (part->address & 0xffe0) | ((part->address + 1) & 0x1f);
				break;
			default: /* single byte read/write */
				part->state = MICROCHIP_STATE_IDLE;
				break;
		}
	}
}

static void
microchip_23k640_cs(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	microchip_23k640_t * part = param;
	
	part->cs = !value;
	if(!part->cs) {
		part->state = 0;
		part->instruction = 0;
	}
}

static void
microchip_23k640_hold(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	microchip_23k640_t * part = param;
	
	part->hold = value;
}

static const char * microchip_23k640_irq_names[IRQ_MICROCHIP_23K640_COUNT] = {
	[IRQ_MICROCHIP_23K640_SPI_BYTE_IN] = "8<microchip_23k640.in",
	[IRQ_MICROCHIP_23K640_SPI_BYTE_OUT] = "8>microchip_23k640.out",
	[IRQ_MICROCHIP_23K640_CS] = "1>microchip_23k640.cs",
	[IRQ_MICROCHIP_23K640_HOLD] = "1>microchip_23k640.hold"
};

void
microchip_23k640_connect(
	microchip_23k640_t * part,
	avr_irq_t * cs_irq)
{
	if(!part || !cs_irq)
		return;

	avr_connect_irq(avr_io_getirq(part->avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT), part->irq + IRQ_MICROCHIP_23K640_SPI_BYTE_IN);
	avr_connect_irq(part->irq + IRQ_MICROCHIP_23K640_SPI_BYTE_OUT, avr_io_getirq(part->avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT));
	avr_connect_irq(cs_irq, part->irq + IRQ_MICROCHIP_23K640_CS);
}

void
microchip_23k640_init(
		struct avr_t * avr,
		microchip_23k640_t * part)
{
	if(!avr || !part)
		return;

	memset(part, 0, sizeof(*part));

	part->avr = avr;
	part->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_MICROCHIP_23K640_COUNT, microchip_23k640_irq_names);
	avr_irq_register_notify(part->irq + IRQ_MICROCHIP_23K640_SPI_BYTE_IN, microchip_23k640_in, part);
	avr_irq_register_notify(part->irq + IRQ_MICROCHIP_23K640_CS, microchip_23k640_cs, part);
	avr_irq_register_notify(part->irq + IRQ_MICROCHIP_23K640_HOLD, microchip_23k640_hold, part);

	part->cs = 0;
	part->hold = 0;
	part->hold_disable = 0;
	part->instruction = 0;
	part->mode = 0;
	part->state = 0;
}
