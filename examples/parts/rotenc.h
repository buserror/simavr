/*
 rotenc.h

 Copyright 2018 Doug Szumski <d.s.szumski@gmail.com>

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
  This 'part' is based on a Panasonic EVEP rotary encoder with a push
  button. It could easily be extended to support modelling other
  rotary encoders.
 */

#ifndef __ROTENC_H__
#define __ROTENC_H__

#include "rotenc.h"

#define ROTENC_CLICK_DURATION_US 100000UL
#define ROTENC_BUTTON_DURATION_US 100000UL
#define ROTENC_STATE_COUNT 4

typedef enum {
	ROTENC_CW_CLICK = 0,
	ROTENC_CCW_CLICK
} rotenc_twist_t;

typedef struct rotenc_pin_state_t {
	uint8_t a_pin;
	uint8_t b_pin;
} rotenc_pins_t;

enum {
	IRQ_ROTENC_OUT_A_PIN = 0,
	IRQ_ROTENC_OUT_B_PIN,
	IRQ_ROTENC_OUT_BUTTON_PIN,
	IRQ_ROTENC_COUNT
};

typedef struct rotenc_t {
	avr_irq_t * irq;		// output irq
	struct avr_t * avr;
	uint8_t verbose;
	rotenc_twist_t direction;
	int phase;			// current position
} rotenc_t;

void
rotenc_init(
	struct avr_t * avr,
	rotenc_t * rotenc);

void
rotenc_twist(
	rotenc_t * rotenc,
	rotenc_twist_t direction);

void
rotenc_button_press(rotenc_t * rotenc);

#endif /* __ROTENC_H__*/
