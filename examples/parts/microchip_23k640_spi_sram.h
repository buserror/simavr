/*
	microchip_23k640_spi_sram.h

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

#ifndef __MICROCHIP_23K640_SPI_SRAM_H__
#define __MICROCHIP_23K640_SPI_SRAM_H__

#ifdef __cplusplus
extern "C" {
#endif

enum {
	IRQ_MICROCHIP_23K640_SPI_BYTE_IN = 0,
	IRQ_MICROCHIP_23K640_SPI_BYTE_OUT,
	IRQ_MICROCHIP_23K640_CS,
	IRQ_MICROCHIP_23K640_HOLD,
	IRQ_MICROCHIP_23K640_COUNT
};

typedef struct microchip_23k640_t {
	avr_irq_t	*irq;
	struct avr_t	*avr;

	int		cs;
	int		hold;
	int		hold_disable;
	int		instruction;
	int		mode;
	int		state;
	
	uint16_t	address;
	uint8_t		data[8192];
}microchip_23k640_t;

/*
 * Default connect going directly to avr spi in/out pins.
 *
 * NOTE: Each part requires separate chip select from avr.
 */
void
microchip_23k640_connect(
		microchip_23k640_t * part,
		avr_irq_t * cs_irq);

void
microchip_23k640_init(
		struct avr_t * avr,
		microchip_23k640_t * part);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef __MICROCHIP_23K640_SPI_SRAM_H__ */

