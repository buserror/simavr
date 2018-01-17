/* vim: ts=4
	history_avr.h

	Copyright 2018 Michel Pollet <buserror@gmail.com>

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

#ifndef _HISTORY_AVR_H_
#define _HISTORY_AVR_H_

#include "sim_avr.h"
#include "sim_elf.h"
#include "history.h"
#include "history_cmd.h"

extern avr_t * avr;
extern int history_redisplay;

void history_avr_init();
void history_avr_idle();

#endif /* _HISTORY_AVR_H_ */
