/*
	reprap.h

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

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


#ifndef __REPRAP_H___
#define __REPRAP_H___

#include "sim_avr.h"
#include "thermistor.h"
#include "heatpot.h"
#include "stepper.h"
#include "uart_pty.h"
#include "sim_vcd_file.h"

typedef struct reprap_t {
	struct avr_t *	avr;
	thermistor_t	therm_hotend;
	thermistor_t	therm_hotbed;
	thermistor_t	therm_spare;
	heatpot_t		hotend;
	heatpot_t		hotbed;

	stepper_t		step_x, step_y, step_z, step_e;

	uart_pty_t		uart_pty;
	avr_vcd_t		vcd_file;
} reprap_t, *reprap_p;

#endif /* __REPRAP_H___ */
