/*
    sim_tinyx4.c

    Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
                         Jon Escombe <lists@dresco.co.uk>

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

#include "sim_tinyx4.h"

void tx4_init(struct avr_t * avr)
{
    struct mcu_t * mcu = (struct mcu_t*)avr;

    avr_eeprom_init(avr, &mcu->eeprom);
    avr_watchdog_init(avr, &mcu->watchdog);
    avr_extint_init(avr, &mcu->extint);
    avr_ioport_init(avr, &mcu->porta);
    avr_ioport_init(avr, &mcu->portb);
    avr_acomp_init(avr, &mcu->acomp);
    avr_adc_init(avr, &mcu->adc);
    avr_timer_init(avr, &mcu->timer0);
    avr_timer_init(avr, &mcu->timer1);
}

void tx4_reset(struct avr_t * avr)
{
//	struct mcu_t * mcu = (struct mcu_t*)avr;
}
