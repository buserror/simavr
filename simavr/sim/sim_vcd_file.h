/*
	sim_vcd_file.c

	Implements a Value Change Dump file outout to generate
	traces & curves and display them in gtkwave.

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

#ifndef __SIM_VCD_FILE_H__
#define __SIM_VCD_FILE_H__

#include <stdio.h>
#include "sim_irq.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Value Change dump module for simavr.
 * 
 * This structure registers IRQ change hooks to various "source" IRQs
 * and dumps their values (if changed) at certains intervals into the VCD file
 */

#define AVR_VCD_MAX_SIGNALS 32
#define AVR_VCD_LOG_SIZE	5120

typedef struct avr_vcd_signal_t {
	avr_irq_t 	irq;		// receiving IRQ
	char	alias;			// vcd one character alias
	int		size;			// in bits
	char	name[32];		// full human name	
} avr_vcd_signal_t;

typedef struct avr_vcd_log_t {
	uint64_t 	when;
	avr_vcd_signal_t * signal;
	uint32_t value;
} avr_vcd_log_t;

typedef struct avr_vcd_t {
	struct avr_t *	avr;	// AVR we are attaching timers to..
	
	char filename[74];		// output filename
	FILE * output;

	int signal_count;
	avr_vcd_signal_t	signal [AVR_VCD_MAX_SIGNALS];	

	uint64_t period;
	uint64_t start;

	uint32_t		logindex;
	avr_vcd_log_t	log[AVR_VCD_LOG_SIZE];
} avr_vcd_t;

// initializes a new VCD trace file, and returns zero if all is well
int avr_vcd_init(struct avr_t * avr, 
	const char * filename, 	// filename to write
	avr_vcd_t * vcd,		// vcd struct to initialize
	uint32_t	period );	// file flushing period is in usec
void avr_vcd_close(avr_vcd_t * vcd);

// Add a trace signal to the vcd file. Must be called before avr_vcd_start()
int avr_vcd_add_signal(avr_vcd_t * vcd, 
	avr_irq_t * signal_irq,
	int signal_bit_size,
	const char * name );

// Starts recording the signal value into the file
int avr_vcd_start(avr_vcd_t * vcd);
// stops recording signal values into the file
int avr_vcd_stop(avr_vcd_t * vcd);

#ifdef __cplusplus
};
#endif

#endif /* __SIM_VCD_FILE_H__ */
