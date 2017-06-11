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

#ifndef __AVR_TIMER_H__
#define __AVR_TIMER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_avr.h"

enum {
	AVR_TIMER_COMPA = 0,
	AVR_TIMER_COMPB,
	AVR_TIMER_COMPC,

	AVR_TIMER_COMP_COUNT
};

enum {
	TIMER_IRQ_OUT_PWM0 = 0,
	TIMER_IRQ_OUT_PWM1,
	TIMER_IRQ_IN_ICP,	// input capture
	TIMER_IRQ_OUT_COMP,	// comparator pins output IRQ

	TIMER_IRQ_COUNT = TIMER_IRQ_OUT_COMP + AVR_TIMER_COMP_COUNT
};

// Get the internal IRQ corresponding to the INT
#define AVR_IOCTL_TIMER_GETIRQ(_name) AVR_IOCTL_DEF('t','m','r',(_name))

// add timer number/name (character) to set tracing flags
#define AVR_IOCTL_TIMER_SET_TRACE(_number) AVR_IOCTL_DEF('t','m','t',(_number))
// enforce using virtual clock generator when external clock is chosen by firmware
#define AVR_IOCTL_TIMER_SET_VIRTCLK(_number) AVR_IOCTL_DEF('t','m','v',(_number))
// set frequency of the virtual clock generator
#define AVR_IOCTL_TIMER_SET_FREQCLK(_number) AVR_IOCTL_DEF('t','m','f',(_number))

// Waveform generation modes
enum {
	avr_timer_wgm_none = 0,	// invalid mode
	avr_timer_wgm_normal,
	avr_timer_wgm_ctc,
	avr_timer_wgm_pwm,
	avr_timer_wgm_fast_pwm,
	avr_timer_wgm_fc_pwm,
};

// Compare output modes
enum {
	avr_timer_com_normal = 0,// Normal mode, OCnx disconnected
	avr_timer_com_toggle,   // Toggle OCnx on compare match
	avr_timer_com_clear,    // clear OCnx on compare match
	avr_timer_com_set,      // set OCnx on compare match
	
};

enum {
	avr_timer_wgm_reg_constant = 0,
	avr_timer_wgm_reg_ocra,
	avr_timer_wgm_reg_icr,
};

typedef struct avr_timer_wgm_t {
	uint32_t top: 8, bottom: 8, size : 8, kind : 8;
} avr_timer_wgm_t;

#define AVR_TIMER_EXTCLK_CHOOSE 0x80		// marker value for cs_div specifying ext clock selection
#define AVR_TIMER_EXTCLK_FLAG_TN 0x80		// Tn external clock chosen
#define AVR_TIMER_EXTCLK_FLAG_STARTED 0x40	// peripheral started
#define AVR_TIMER_EXTCLK_FLAG_REVDIR 0x20	// reverse counting (decrement)
#define AVR_TIMER_EXTCLK_FLAG_AS2 0x10		// asynchronous external clock chosen
#define AVR_TIMER_EXTCLK_FLAG_VIRT 0x08		// don't use the input pin, generate clock internally
#define AVR_TIMER_EXTCLK_FLAG_EDGE 0x01		// use the rising edge

#define AVR_TIMER_WGM_NORMAL8() { .kind = avr_timer_wgm_normal, .size=8 }
#define AVR_TIMER_WGM_NORMAL16() { .kind = avr_timer_wgm_normal, .size=16 }
#define AVR_TIMER_WGM_CTC() { .kind = avr_timer_wgm_ctc, .top = avr_timer_wgm_reg_ocra }
#define AVR_TIMER_WGM_ICCTC() { .kind = avr_timer_wgm_ctc, .top = avr_timer_wgm_reg_icr }
#define AVR_TIMER_WGM_FASTPWM8() { .kind = avr_timer_wgm_fast_pwm, .size=8 }
#define AVR_TIMER_WGM_FASTPWM9() { .kind = avr_timer_wgm_fast_pwm, .size=9 }
#define AVR_TIMER_WGM_FASTPWM10() { .kind = avr_timer_wgm_fast_pwm, .size=10 }
#define AVR_TIMER_WGM_FCPWM8() { .kind = avr_timer_wgm_fc_pwm, .size=8 }
#define AVR_TIMER_WGM_FCPWM9() { .kind = avr_timer_wgm_fc_pwm, .size=9 }
#define AVR_TIMER_WGM_FCPWM10() { .kind = avr_timer_wgm_fc_pwm, .size=10 }
#define AVR_TIMER_WGM_OCPWM() { .kind = avr_timer_wgm_pwm, .top = avr_timer_wgm_reg_ocra }
#define AVR_TIMER_WGM_ICPWM() { .kind = avr_timer_wgm_pwm, .top = avr_timer_wgm_reg_icr }

typedef struct avr_timer_comp_t {
		avr_int_vector_t	interrupt;		// interrupt vector
		struct avr_timer_t	*timer;			// parent timer
		avr_io_addr_t		r_ocr;			// comparator register low byte
		avr_io_addr_t		r_ocrh;			// comparator register hi byte
		avr_regbit_t		com;			// comparator output mode registers
		avr_regbit_t		com_pin;		// where comparator output is connected
		uint64_t			comp_cycles;
} avr_timer_comp_t, *avr_timer_comp_p;

enum {
	avr_timer_trace_ocr		= (1 << 0),
	avr_timer_trace_tcnt	= (1 << 1),

	avr_timer_trace_compa 	= (1 << 8),
	avr_timer_trace_compb 	= (1 << 9),
	avr_timer_trace_compc 	= (1 << 10),
};

typedef struct avr_timer_t {
	avr_io_t		io;
	char 			name;
	uint32_t		trace;		// debug trace

	avr_regbit_t	disabled;	// bit in the PRR

	avr_io_addr_t	r_tcnt, r_icr;
	avr_io_addr_t	r_tcnth, r_icrh;

	avr_regbit_t	wgm[4];
	avr_timer_wgm_t	wgm_op[16];
	avr_timer_wgm_t	mode;
	int				wgm_op_mode_kind;
	uint32_t		wgm_op_mode_size;

	avr_regbit_t	as2;		// asynchronous clock 32khz
	avr_regbit_t	cs[4];		// specify control register bits choosing clock sourcre
	uint8_t			cs_div[16];	// translate control register value to clock prescaler (orders of 2 exponent)
	uint32_t		cs_div_value;

	avr_regbit_t	ext_clock_pin;	// external clock input pin, to link IRQs
	uint8_t			ext_clock_flags;	// holds AVR_TIMER_EXTCLK_FLAG_ON, AVR_TIMER_EXTCLK_FLAG_EDGE and other ext. clock mode flags
	float			ext_clock;	// external clock frequency, e.g. 32768Hz

	avr_regbit_t	icp;		// input capture pin, to link IRQs
	avr_regbit_t	ices;		// input capture edge select

	avr_timer_comp_t comp[AVR_TIMER_COMP_COUNT];

	avr_int_vector_t overflow;	// overflow
	avr_int_vector_t icr;	// input capture

	uint64_t		tov_cycles;	// number of cycles from zero to overflow
	float			tov_cycles_fract; // fractional part for external clock with non int ratio to F_CPU
	float			phase_accumulator;
	uint64_t		tov_base;	// MCU cycle when the last overflow occured; when clocked externally holds external clock count
	uint16_t		tov_top;	// current top value to calculate tnct
} avr_timer_t;

void avr_timer_init(avr_t * avr, avr_timer_t * port);

#ifdef __cplusplus
};
#endif

#endif /*__AVR_TIMER_H__*/
