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

#include <stdio.h>
#include <string.h>
#include "sim_vcd_file.h"
#include "sim_avr.h"

void _avr_vcd_notify(struct avr_irq_t * irq, uint32_t value, void * param);

int avr_vcd_init(struct avr_t * avr, const char * filename, avr_vcd_t * vcd, uint32_t period)
{
	memset(vcd, 0, sizeof(avr_vcd_t));
	vcd->avr = avr;
	strncpy(vcd->filename, filename, sizeof(vcd->filename));
	vcd->period = avr_usec_to_cycles(vcd->avr, period);
	
	for (int i = 0; i < AVR_VCD_MAX_SIGNALS; i++) {
		avr_init_irq(&vcd->signal[i].irq, i, 1);
		avr_irq_register_notify(&vcd->signal[i].irq, _avr_vcd_notify, &vcd->signal[i]);
	}
	
	return 0;
}

void avr_vcd_close(avr_vcd_t * vcd)
{
	avr_vcd_stop(vcd);
}

void _avr_vcd_notify(struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_vcd_signal_t * s = param;
	s->touched++;
}

static char * _avr_vcd_get_signal_text(avr_vcd_signal_t * s, char * out)
{
	char * dst = out;
		
	if (s->size > 1)
		*dst++ = 'b';
	
	for (int i = s->size; i > 0; i--)
		*dst++ = s->irq.value & (1 << (i-1)) ? '1' : '0';
	if (s->size > 1)
		*dst++ = ' ';
	*dst++ = s->alias;
	*dst = 0;
	return out;
}

static avr_cycle_count_t _avr_vcd_timer(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_vcd_t * vcd = param;
	int done = 0;

	if (vcd->start == 0)
		vcd->start = when;

	for (int i = 0; i < vcd->signal_count; i++) {
		avr_vcd_signal_t * s = &vcd->signal[i];
		if (s->touched) {
			if (done == 0) {
				fprintf(vcd->output, "#%ld\n", 
					avr_cycles_to_usec(vcd->avr, when - vcd->start));
			}
			char out[32];
			fprintf(vcd->output, "%s\n", _avr_vcd_get_signal_text(s, out));
			
			s->touched = 0;
			done++;
		}
	}
	return when + vcd->period;
}

int avr_vcd_add_signal(avr_vcd_t * vcd, 
	avr_irq_t * signal_irq,
	int signal_bit_size,
	const char * name )
{
	if (vcd->signal_count == AVR_VCD_MAX_SIGNALS)
		return -1;
	avr_vcd_signal_t * s = &vcd->signal[vcd->signal_count++];
	strncpy(s->name, name, sizeof(s->name));
	s->size = signal_bit_size;
	s->alias = ' ' + vcd->signal_count ;
	s->touched = 0;	// want an initial dump...
	avr_connect_irq(signal_irq, &s->irq);
}


int avr_vcd_start(avr_vcd_t * vcd)
{
	if (vcd->output)
		avr_vcd_stop(vcd);
	vcd->output = fopen(vcd->filename, "w");
	if (vcd->output == NULL) {
		perror(vcd->filename);
		return -1;
	}
	fprintf(vcd->output, "$timescale 1us $end\n");
	fprintf(vcd->output, "$scope module logic $end\n");

	for (int i = 0; i < vcd->signal_count; i++) {
		fprintf(vcd->output, "$var wire %d %c %s $end\n",
			vcd->signal[i].size, vcd->signal[i].alias, vcd->signal[i].name);
	}

	fprintf(vcd->output, "$upscope $end\n");
	fprintf(vcd->output, "$enddefinitions $end\n");
	
	fprintf(vcd->output, "$dumpvars\n");
	for (int i = 0; i < vcd->signal_count; i++) {
		avr_vcd_signal_t * s = &vcd->signal[i];
		s->touched = 0;
		char out[32];
		fprintf(vcd->output, "%s\n", _avr_vcd_get_signal_text(s, out));
	}
	fprintf(vcd->output, "$end\n");
	avr_cycle_timer_register(vcd->avr, vcd->period, _avr_vcd_timer, vcd);	
}

int avr_vcd_stop(avr_vcd_t * vcd)
{
	avr_cycle_timer_cancel(vcd->avr, _avr_vcd_timer, vcd);

	if (vcd->output)
		fclose(vcd->output);
	vcd->output = NULL;
}


