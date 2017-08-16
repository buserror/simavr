/*
	avr_eeprom.h

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

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

#ifndef __AVR_EEPROM_H__
#define __AVR_EEPROM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_avr.h"

typedef struct avr_eeprom_t {
	avr_io_t	io;

	uint8_t *	eeprom;	// actual bytes
	uint16_t	size;	// size for this MCU
	
	uint8_t r_eearh;
	uint8_t r_eearl;
	uint8_t r_eedr;

	// eepm -- eeprom write mode
	uint8_t	r_eecr;	// shortcut, assumes these bits fit in that register
	avr_regbit_t	eepm[4];
	avr_regbit_t 	eempe;	// eeprom master program enable
	avr_regbit_t 	eepe;	// eeprom program enable
	avr_regbit_t 	eere;	// eeprom read enable
	
	avr_int_vector_t ready;	// EERIE vector
} avr_eeprom_t;

void avr_eeprom_init(avr_t * avr, avr_eeprom_t * port);

typedef struct avr_eeprom_desc_t {
	uint8_t * 	ee;
	uint16_t	offset;
	uint32_t	size;
} avr_eeprom_desc_t;

#define AVR_IOCTL_EEPROM_GET	AVR_IOCTL_DEF('e','e','g','p')
#define AVR_IOCTL_EEPROM_SET	AVR_IOCTL_DEF('e','e','s','p')


/*
 * the eeprom block seems to be very similar across AVRs, 
 * so here is a macro to declare a "typical" one in a core.
 */

#define AVR_EEPROM_DECLARE(_vector) \
	.eeprom = {\
		.size = E2END+1,\
		.r_eearh = EEARH,\
		.r_eearl = EEARL,\
		.r_eedr = EEDR,\
		.r_eecr = EECR,\
		.eepm = { AVR_IO_REGBIT(EECR, EEPM0), AVR_IO_REGBIT(EECR, EEPM1) },\
		.eempe = AVR_IO_REGBIT(EECR, EEMPE),\
		.eepe = AVR_IO_REGBIT(EECR, EEPE),\
		.eere = AVR_IO_REGBIT(EECR, EERE),\
		.ready = {\
			.enable = AVR_IO_REGBIT(EECR, EERIE),\
			.vector = _vector,\
		},\
	}

/*
 * no EEPM registers in atmega128
 */
#define AVR_EEPROM_DECLARE_NOEEPM(_vector)		\
	.eeprom = {\
		.size = E2END+1,\
		.r_eearh = EEARH,\
		.r_eearl = EEARL,\
		.r_eedr = EEDR,\
		.r_eecr = EECR,\
		.eepm = { },		\
		.eempe = AVR_IO_REGBIT(EECR, EEMWE),\
		.eepe = AVR_IO_REGBIT(EECR, EEWE),\
		.eere = AVR_IO_REGBIT(EECR, EERE),\
		.ready = {\
			.enable = AVR_IO_REGBIT(EECR, EERIE),\
			.vector = _vector,\
		},\
	}


/*
 * macro definition without a high address bit register,
 * which is not implemented in some tiny AVRs.
 */

#define AVR_EEPROM_DECLARE_8BIT(_vector) \
	.eeprom = {\
		.size = E2END+1,\
		.r_eearl = EEAR,\
		.r_eedr = EEDR,\
		.r_eecr = EECR,\
		.eepm = { AVR_IO_REGBIT(EECR, EEPM0), AVR_IO_REGBIT(EECR, EEPM1) },\
		.eempe = AVR_IO_REGBIT(EECR, EEMPE),\
		.eepe = AVR_IO_REGBIT(EECR, EEPE),\
		.eere = AVR_IO_REGBIT(EECR, EERE),\
		.ready = {\
			.enable = AVR_IO_REGBIT(EECR, EERIE),\
			.vector = _vector,\
		},\
	}

#ifdef __cplusplus
};
#endif

#endif /* __AVR_EEPROM_H__ */
