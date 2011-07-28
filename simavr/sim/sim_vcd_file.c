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
		avr_init_irq(&avr->irq_pool, &vcd->signal[i].irq, i, 1, NULL /* TODO IRQ name */);
		avr_irq_register_notify(&vcd->signal[i].irq, _avr_vcd_notify, vcd);
	}
	
	return 0;
}

void avr_vcd_close(avr_vcd_t * vcd)
{
	avr_vcd_stop(vcd);
}

void _avr_vcd_notify(struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_vcd_t * vcd = (avr_vcd_t *)param;
	if (!vcd->output)
		return;
	avr_vcd_signal_t * s = (avr_vcd_signal_t*)irq;
	if (vcd->logindex == AVR_VCD_LOG_SIZE) {
		printf("_avr_vcd_notify %s overrun value buffer %d\n", s->name, AVR_VCD_LOG_SIZE);
		return;
	}
	avr_vcd_log_t *l = &vcd->log[vcd->logindex++];
	l->signal = s;
	l->when = vcd->avr->cycle;
	l->value = value;
}

static char * _avr_vcd_get_signal_text(avr_vcd_signal_t * s, char * out, uint32_t value)
{
	char * dst = out;
		
	if (s->size > 1)
		*dst++ = 'b';
	
	for (int i = s->size; i > 0; i--)
		*dst++ = value & (1 << (i-1)) ? '1' : '0';
	if (s->size > 1)
		*dst++ = ' ';
	*dst++ = s->alias;
	*dst = 0;
	return out;
}

static void avr_vcd_flush_log(avr_vcd_t * vcd)
{
#if AVR_VCD_MAX_SIGNALS > 32
	uint64_t seen = 0;
#else
	uint32_t seen = 0;
#endif
	uint64_t oldbase = 0;	// make sure it's different
	char out[48];

	if (!vcd->logindex)
		return;
//	printf("avr_vcd_flush_log %d\n", vcd->logindex);


	for (uint32_t li = 0; li < vcd->logindex; li++) {
		avr_vcd_log_t *l = &vcd->log[li];
		uint64_t base = avr_cycles_to_nsec(vcd->avr, l->when - vcd->start);	// 1ns base

		// if that trace was seen in this usec already, we fudge the base time
		// to make sure the new value is offset by one usec, to make sure we get
		// at least a small pulse on the waveform
		// This is a bit of a fudge, but it is the only way to represent very 
		// short"pulses" that are still visible on the waveform.
		if (base == oldbase && seen & (1 << l->signal->irq.irq))
			base++;	// this forces a new timestamp
			
		if (base > oldbase || li == 0) {
			seen = 0;
			fprintf(vcd->output, "#%llu\n", (long long unsigned int)base);
			oldbase = base;
		}
		seen |= (1 << l->signal->irq.irq);	// mark this trace as seen for this timestamp
		fprintf(vcd->output, "%s\n", _avr_vcd_get_signal_text(l->signal, out, l->value));
	}
	vcd->logindex = 0;
}

static avr_cycle_count_t _avr_vcd_timer(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_vcd_t * vcd = param;
	avr_vcd_flush_log(vcd);
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
	avr_connect_irq(signal_irq, &s->irq);
	return 0;
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
		
	fprintf(vcd->output, "$timescale 1ns $end\n");	// 1ns base
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
		char out[48];
		fprintf(vcd->output, "%s\n", _avr_vcd_get_signal_text(s, out, s->irq.value));
	}
	fprintf(vcd->output, "$end\n");
	vcd->start = vcd->avr->cycle;
	avr_cycle_timer_register(vcd->avr, vcd->period, _avr_vcd_timer, vcd);	
	return 0;
}

int avr_vcd_stop(avr_vcd_t * vcd)
{
	avr_cycle_timer_cancel(vcd->avr, _avr_vcd_timer, vcd);

	avr_vcd_flush_log(vcd);
	
	if (vcd->output)
		fclose(vcd->output);
	vcd->output = NULL;
	return 0;
}


