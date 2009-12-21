/*
	avr_timer.h

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

#ifndef AVR_TIMER_H_
#define AVR_TIMER_H_

#include "sim_avr.h"

enum {
	TIMER_IRQ_OUT_PWM0 = 0,
	TIMER_IRQ_OUT_PWM1,
	TIMER_IRQ_COUNT
};

// Get the internal IRQ corresponding to the INT
#define AVR_IOCTL_TIMER_GETIRQ(_name) AVR_IOCTL_DEF('t','m','r',(_name))

enum {
	avr_timer_wgm_none = 0,	// invalid mode
	avr_timer_wgm_normal,
	avr_timer_wgm_ctc,
	avr_timer_wgm_fast_pwm,
};
typedef struct avr_timer_wgm_t {
	uint32_t top: 8, bottom: 8, size : 8, kind : 8;
} avr_timer_wgm_t;

#define AVR_TIMER_WGM_NORMAL8() { .kind = avr_timer_wgm_normal, .size=8 }
#define AVR_TIMER_WGM_NORMAL16() { .kind = avr_timer_wgm_normal, .size=16 }
#define AVR_TIMER_WGM_CTC() { .kind = avr_timer_wgm_ctc }
#define AVR_TIMER_WGM_FASTPWM() { .kind = avr_timer_wgm_fast_pwm }

typedef struct avr_timer_t {
	avr_io_t	io;
	char name;
	avr_regbit_t	disabled;	// bit in the PRR

	avr_io_addr_t	r_ocra, r_ocrb, r_ocrc, r_tcnt, r_icr;
	avr_io_addr_t	r_ocrah, r_ocrbh, r_tcnth, r_icrh;
	
	avr_regbit_t	wgm[4];
	avr_timer_wgm_t	wgm_op[16];

	avr_regbit_t	cs[4];
	uint8_t			cs_div[16];
	avr_regbit_t	as2;		// asynchronous clock 32khz

	avr_int_vector_t compa;	// comparator A
	avr_int_vector_t compb;	// comparator A
	avr_int_vector_t overflow;	// overflow
	avr_int_vector_t icr;	// input capture

	uint64_t		compa_cycles;
	uint64_t		compb_cycles;
	uint64_t		tov_cycles;
	uint64_t		tov_base;	// we we last were called
	uint16_t		tov_top;	// current top value to calculate tnct
} avr_timer_t;

void avr_timer_init(avr_t * avr, avr_timer_t * port);

#endif /* AVR_TIMER_H_ */
