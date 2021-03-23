/*
   sim_mega2560.c

   Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
   Copyright 2013 Yann GOUY <yann_gouy@yahoo.fr>

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

#include "sim_avr.h"
#include "sim_mega2560.h"

static avr_t * make()
{
	return avr_core_allocate(&mcu_mega2560.core, sizeof(struct mcu_t));
}

avr_kind_t mega2560 = {
		.names = { "atmega2560", "atmega2561" },
		.make = make
};

void m2560_init(struct avr_t * avr)
{
	struct mcu_t * mcu = (struct mcu_t*)avr;
	
	avr_eeprom_init(avr, &mcu->eeprom);
	avr_flash_init(avr, &mcu->selfprog);
	avr_extint_init(avr, &mcu->extint);
	avr_watchdog_init(avr, &mcu->watchdog);
	avr_ioport_init(avr, &mcu->porta);
	avr_ioport_init(avr, &mcu->portb);
	avr_ioport_init(avr, &mcu->portc);
	avr_ioport_init(avr, &mcu->portd);
	avr_ioport_init(avr, &mcu->porte);
	avr_ioport_init(avr, &mcu->portf);
	avr_ioport_init(avr, &mcu->portg);
	avr_ioport_init(avr, &mcu->porth);
	avr_ioport_init(avr, &mcu->portj);
	avr_ioport_init(avr, &mcu->portk);
	avr_ioport_init(avr, &mcu->portl);

	avr_uart_init(avr, &mcu->uart0);
	avr_uart_init(avr, &mcu->uart1);
	avr_uart_init(avr, &mcu->uart2);
	avr_uart_init(avr, &mcu->uart3);
        avr_acomp_init(avr, &mcu->acomp);
	avr_adc_init(avr, &mcu->adc);
	avr_timer_init(avr, &mcu->timer0);
	avr_timer_init(avr, &mcu->timer1);
	avr_timer_init(avr, &mcu->timer2);
	avr_timer_init(avr, &mcu->timer3);
	avr_timer_init(avr, &mcu->timer4);
	avr_timer_init(avr, &mcu->timer5);
	avr_spi_init(avr, &mcu->spi);
	avr_twi_init(avr, &mcu->twi);
}

void m2560_reset(struct avr_t * avr)
{
// struct mcu_t * mcu = (struct mcu_t*)avr;
}


