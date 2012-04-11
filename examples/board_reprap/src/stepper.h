/*
	stepper.h

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


#ifndef __STEPPER_H___
#define __STEPPER_H___

#include "sim_irq.h"

enum {
	IRQ_STEPPER_DIR_IN = 0,
	IRQ_STEPPER_STEP_IN,
	IRQ_STEPPER_ENABLE_IN,
	IRQ_STEPPER_POSITION_OUT,
	IRQ_STEPPER_ENDSTOP_OUT,
	IRQ_STEPPER_COUNT
};

typedef struct stepper_t {
	avr_irq_t *	irq;		// irq list
	struct avr_t *avr;		// keep it around so we can pause it
	char name[32];
	int enable : 1, dir : 1, trace : 1;
	double steps_per_mm;
	double position;
	double max_position;
	double endstop;
} stepper_t, *stepper_p;

void
stepper_init(
		struct avr_t * avr,
		stepper_p p,
		char * name,
		float steps_per_mm,
		float start_position,
		float max_position,
		float endstop_position);

enum {
	stepper_endstop_inverted = (1 << 0),
};
void
stepper_connect(
		stepper_p p,
		avr_irq_t *	step,
		avr_irq_t *	dir,
		avr_irq_t *	enable,
		avr_irq_t *	endstop,
		uint16_t flags);

#endif /* __STEPPER_H___ */
