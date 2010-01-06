/*
	sim_megax4.c

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
#include <stdio.h>
#include "sim_avr.h"

#include "sim_megax4.h"

void mx4_init(struct avr_t * avr)
{
	struct mcu_t * mcu = (struct mcu_t*)avr;

	printf("%s init\n", avr->mmcu);
	
	avr_eeprom_init(avr, &mcu->eeprom);
	avr_flash_init(avr, &mcu->selfprog);
	avr_extint_init(avr, &mcu->extint);
	avr_watchdog_init(avr, &mcu->watchdog);
	avr_ioport_init(avr, &mcu->porta);
	avr_ioport_init(avr, &mcu->portb);
	avr_ioport_init(avr, &mcu->portc);
	avr_ioport_init(avr, &mcu->portd);
	avr_uart_init(avr, &mcu->uart0);
	avr_uart_init(avr, &mcu->uart1);
	avr_adc_init(avr, &mcu->adc);
	avr_timer_init(avr, &mcu->timer0);
	avr_timer_init(avr, &mcu->timer1);
	avr_timer_init(avr, &mcu->timer2);
	avr_spi_init(avr, &mcu->spi);
	avr_twi_init(avr, &mcu->twi);
}

void mx4_reset(struct avr_t * avr)
{
//	struct mcu_t * mcu = (struct mcu_t*)avr;
}
