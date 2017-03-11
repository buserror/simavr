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
#define _GNU_SOURCE /* for strdupa */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <ctype.h>
#include "sim_vcd_file.h"
#include "sim_avr.h"
#include "sim_time.h"
#include "sim_utils.h"

DEFINE_FIFO(avr_vcd_log_t, avr_vcd_fifo);

static void
_avr_vcd_notify(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param);

int
avr_vcd_init(
		struct avr_t * avr,
		const char * filename,
		avr_vcd_t * vcd,
		uint32_t period)
{
	memset(vcd, 0, sizeof(avr_vcd_t));
	vcd->avr = avr;
	vcd->filename = strdup(filename);
	vcd->period = avr_usec_to_cycles(vcd->avr, period);

	return 0;
}

/*
 * Parse a VCD 'timing' line. The lines are assumed to be:
 * #<absolute timestamp>[\n][<value x/0/1><signal alias character>|
 * 		b[x/0/1]?<space><signal alias character]+
 * For example:
 * #1234 1' 0$
 * Or:
 * #1234
 * b1101x1 '
 * 0$
 *
 * This function tries to handle this transparently, and pushes the
 * signal/values into the FIFO for processing by the timer when
 * convenient.
 * NOTE: Add 'floating' support here. Also, FIX THE TIMING.
 */
static avr_cycle_count_t
avr_vcd_input_parse_line(
		avr_vcd_t * vcd,
		argv_p v )
{
	avr_cycle_count_t res = 0;
	int vi = 0;

	if (v->argc == 0)
		return res;

	if (v->argv[0][0] == '#') {
		res = atoll(v->argv[0] + 1) * vcd->vcd_to_us;
		vcd->start = vcd->period;
		vcd->period = res;
		vi++;
	}
	for (int i = vi; i < v->argc; i++) {
		char * a = v->argv[i];
		uint32_t val = 0;
		int floating = 0;
		char name = 0;
		int sigindex = -1;

		if (*a == 'b')
			a++;
		while (*a) {
			if (*a == 'x') {
				val <<= 1;
				floating |= (floating << 1) | 1;
			} else if (*a == '0' || *a == '1') {
				val = (val << 1) | (*a - '0');
				floating <<= 1;
			} else {
				name = *a;
				break;
			}
			a++;
		}
		if (!name && (i < v->argc - 1)) {
			const char *n = v->argv[i+1];
			if (strlen(n) == 1) {
				// we've got a name, it was not attached
				name = *n;
				i++;	// skip that one
			}
		}
		if (name) {
			for (int si = 0;
						si < vcd->signal_count &&
						sigindex == -1; si++) {
				if (vcd->signal[si].alias == name)
					sigindex = si;
			}
		}
		if (sigindex == -1) {
			printf("Signal name '%c' value %x not found\n",
					name? name : '?', val);
			continue;
		}
		avr_vcd_log_t e = {
				.when = vcd->period,
				.sigindex = sigindex,
				.floating = !!floating,
				.value = val,
		};
	//	printf("%10u %d\n", e.when, e.value);
		avr_vcd_fifo_write(&vcd->log, e);
	}
	return res;
}

/*
 * Read some signals from the file and fill the FIFO with it, we read
 * a completely arbitrary amount of stuff to fill the FIFO reasonably well
 */
static int
avr_vcd_input_read(
		avr_vcd_t * vcd )
{
	char line[1024];

	while (fgets(line, sizeof(line), vcd->input)) {
	//	printf("%s", line);
		if (!line[0])	// technically can't happen, but make sure next line works
			continue;
		vcd->input_line = argv_parse(vcd->input_line, line);
		avr_vcd_input_parse_line(vcd, vcd->input_line);
		/* stop once the fifo is full enough */
		if (avr_vcd_fifo_get_read_size(&vcd->log) >= 128)
			break;
	}
	return avr_vcd_fifo_isempty(&vcd->log);
}

/*
 * This is called when we need to change the state of one or more IRQ,
 * so look in the FIFO to know 'our' stamp time, read as much as we can
 * that is still on that same timestamp.
 * When when the FIFO content has too far in the future, re-schedule the
 * timer for that time and shoot of.
 * Also try to top up the FIFO with new read stuff when it's drained
 */
static avr_cycle_count_t
_avr_vcd_input_timer(
		struct avr_t * avr,
		avr_cycle_count_t when,
		void * param)
{
	avr_vcd_t * vcd = param;

	// get some more if needed
	if (avr_vcd_fifo_get_read_size(&vcd->log) < (vcd->signal_count * 16))
		avr_vcd_input_read(vcd);

	if (avr_vcd_fifo_isempty(&vcd->log)) {
		printf("%s DONE but why are we here?\n", __func__);
		return 0;
	}

	avr_vcd_log_t log = avr_vcd_fifo_read_at(&vcd->log, 0);
	uint64_t stamp = log.when;
	while (!avr_vcd_fifo_isempty(&vcd->log)) {
		log = avr_vcd_fifo_read_at(&vcd->log, 0);
		if (log.when != stamp)	// leave those in the FIFO
			break;
		// we already have it
		avr_vcd_fifo_read_offset(&vcd->log, 1);
		avr_vcd_signal_p signal = &vcd->signal[log.sigindex];
		avr_raise_irq_float(&signal->irq, log.value, log.floating);
	}

	if (avr_vcd_fifo_isempty(&vcd->log)) {
		AVR_LOG(vcd->avr, LOG_TRACE,
				"%s Finished reading, ending simavr\n",
				vcd->filename);
		avr->state = cpu_Done;
		return 0;
	}
	log = avr_vcd_fifo_read_at(&vcd->log, 0);

	when += avr_usec_to_cycles(avr, log.when - stamp);

	return when;
}

int
avr_vcd_init_input(
		struct avr_t * avr,
		const char * filename, 	// filename to read
		avr_vcd_t * vcd )		// vcd struct to initialize
{
	memset(vcd, 0, sizeof(avr_vcd_t));
	vcd->avr = avr;
	vcd->filename = strdup(filename);

	vcd->input = fopen(vcd->filename, "r");
	if (!vcd->input) {
		perror(filename);
		return -1;
	}
	char line[1024];
	argv_p v = NULL;

	while (fgets(line, sizeof(line), vcd->input)) {
		if (!line[0])	// technically can't happen, but make sure next line works
			continue;
		v = argv_parse(v, line);

		// we are done reading headers, got our first timestamp
		if (v->line[0] == '#') {
			vcd->start = 0;
			avr_vcd_input_parse_line(vcd, v);
			avr_cycle_timer_register_usec(vcd->avr,
						vcd->period, _avr_vcd_input_timer, vcd);
			break;
		}
		// ignore multiline stuff
		if (v->line[0] != '$')
			continue;

		const char * end = !strcmp(v->argv[v->argc - 1], "$end") ?
								v->argv[v->argc - 1] : NULL;
		const char *keyword = v->argv[0];

		if (keyword == end)
			keyword = NULL;
		if (!keyword)
			continue;

		if (!strcmp(keyword, "$timescale")) {
			double cnt = 0;
			vcd->vcd_to_us = 1;
			char *si = v->argv[1];
			while (si && *si && isdigit(*si))
				cnt = (cnt * 10) + (*si++ - '0');
			while (*si == ' ')
				si++;
			if (!*si)
				si = v->argv[2];
		//	if (!strcmp(si, "ns")) // TODO: Check that,
		//		vcd->vcd_to_us = cnt;
		//	printf("cnt %dus; unit %s\n", (int)cnt, si);
		} else if (!strcmp(keyword, "$var")) {
			const char *name = v->argv[4];

			vcd->signal[vcd->signal_count].alias = v->argv[3][0];
			vcd->signal[vcd->signal_count].size = atoi(v->argv[2]);
			strncpy(vcd->signal[vcd->signal_count].name, name,
						sizeof(vcd->signal[0].name));

			vcd->signal_count++;
		}
	}
	// reuse this one
	vcd->input_line = v;

	for (int i = 0; i < vcd->signal_count; i++) {
		AVR_LOG(vcd->avr, LOG_TRACE, "%s %2d '%c' %s : size %d\n",
				__func__, i,
				vcd->signal[i].alias, vcd->signal[i].name,
				vcd->signal[i].size);
		/* format is <four-character ioctl>[_<IRQ index>] */
		if (strlen(vcd->signal[i].name) >= 4) {
			char *dup = strdupa(vcd->signal[i].name);
			char *ioctl = strsep(&dup, "_");
			int index = 0;
			if (dup)
				index = atoi(dup);
			if (strlen(ioctl) == 4) {
				uint32_t ioc = AVR_IOCTL_DEF(
									ioctl[0], ioctl[1], ioctl[2], ioctl[3]);
				avr_irq_t * irq = avr_io_getirq(vcd->avr, ioc, index);
				if (irq) {
					vcd->signal[i].irq.flags = IRQ_FLAG_INIT;
					avr_connect_irq(&vcd->signal[i].irq, irq);
				} else
					AVR_LOG(vcd->avr, LOG_WARNING,
							"%s IRQ was not found\n",
							vcd->signal[i].name);
				continue;
			}
			AVR_LOG(vcd->avr, LOG_WARNING,
					"%s is an invalid IRQ format\n",
					vcd->signal[i].name);
		}
	}
	return 0;
}

void
avr_vcd_close(
		avr_vcd_t * vcd)
{
	avr_vcd_stop(vcd);

	/* dispose of any link and hooks */
	for (int i = 0; i < vcd->signal_count; i++) {
		avr_vcd_signal_t * s = &vcd->signal[i];

		avr_free_irq(&s->irq, 1);
	}

	if (vcd->filename) {
		free(vcd->filename);
		vcd->filename = NULL;
	}
}

static char *
_avr_vcd_get_float_signal_text(
		avr_vcd_signal_t * s,
		char * out)
{
	char * dst = out;

	if (s->size > 1)
		*dst++ = 'b';

	for (int i = s->size; i > 0; i--)
		*dst++ = 'x';
	if (s->size > 1)
		*dst++ = ' ';
	*dst++ = s->alias;
	*dst = 0;
	return out;
}

static char *
_avr_vcd_get_signal_text(
		avr_vcd_signal_t * s,
		char * out,
		uint32_t value)
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

static void
avr_vcd_flush_log(
		avr_vcd_t * vcd)
{
#if AVR_VCD_MAX_SIGNALS > 32
	uint64_t seen = 0;
#else
	uint32_t seen = 0;
#endif
	uint64_t oldbase = 0;	// make sure it's different
	char out[48];

	if (avr_vcd_fifo_isempty(&vcd->log) || !vcd->output)
		return;

	while (!avr_vcd_fifo_isempty(&vcd->log)) {
		avr_vcd_log_t l = avr_vcd_fifo_read(&vcd->log);
		// 1ns base
		uint64_t base = avr_cycles_to_nsec(vcd->avr, l.when - vcd->start);

		/*
		 * if that trace was seen in this nsec already, we fudge the
		 * base time to make sure the new value is offset by one nsec,
		 * to make sure we get at least a small pulse on the waveform.
		 *
		 * This is a bit of a fudge, but it is the only way to represent
		 * very short "pulses" that are still visible on the waveform.
		 */
		if (base == oldbase &&
				(seen & (1 << l.sigindex)))
			base++;	// this forces a new timestamp

		if (base > oldbase || !seen) {
			seen = 0;
			fprintf(vcd->output, "#%" PRIu64  "\n", base);
			oldbase = base;
		}
		// mark this trace as seen for this timestamp
		seen |= (1 << l.sigindex);
		fprintf(vcd->output, "%s\n",
				l.floating ?
					_avr_vcd_get_float_signal_text(
							&vcd->signal[l.sigindex],
							out) :
					_avr_vcd_get_signal_text(
							&vcd->signal[l.sigindex],
							out, l.value));
	}
}

static avr_cycle_count_t
_avr_vcd_timer(
		struct avr_t * avr,
		avr_cycle_count_t when,
		void * param)
{
	avr_vcd_t * vcd = param;
	avr_vcd_flush_log(vcd);
	return when + vcd->period;
}

static void
_avr_vcd_notify(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	avr_vcd_t * vcd = (avr_vcd_t *)param;

	if (!vcd->output)
		return;

	avr_vcd_signal_t * s = (avr_vcd_signal_t*)irq;
	avr_vcd_log_t l = {
		.sigindex = s->irq.irq,
		.when = vcd->avr->cycle,
		.value = value,
		.floating = !!(avr_irq_get_flags(irq) & IRQ_FLAG_FLOATING),
	};
	if (avr_vcd_fifo_isfull(&vcd->log)) {
		AVR_LOG(vcd->avr, LOG_WARNING,
				"%s FIFO Overload, flushing!\n",
				__func__);
		/* Decrease period by a quarter, for next time */
		vcd->period -= vcd->period >> 2;
		avr_vcd_flush_log(vcd);
	}
	avr_vcd_fifo_write(&vcd->log, l);
}

int
avr_vcd_add_signal(
		avr_vcd_t * vcd,
		avr_irq_t * signal_irq,
		int signal_bit_size,
		const char * name )
{
	if (vcd->signal_count == AVR_VCD_MAX_SIGNALS)
		return -1;
	int index = vcd->signal_count++;
	avr_vcd_signal_t * s = &vcd->signal[index];
	strncpy(s->name, name, sizeof(s->name));
	s->size = signal_bit_size;
	s->alias = ' ' + vcd->signal_count ;

	/* manufacture a nice IRQ name */
	int l = strlen(name);
	char iname[10 + l + 1];
	if (signal_bit_size > 1)
		sprintf(iname, "%d>vcd.%s", signal_bit_size, name);
	else
		sprintf(iname, ">vcd.%s", name);

	const char * names[1] = { iname };
	avr_init_irq(&vcd->avr->irq_pool, &s->irq, index, 1, names);
	avr_irq_register_notify(&s->irq, _avr_vcd_notify, vcd);

	avr_connect_irq(signal_irq, &s->irq);
	return 0;
}


int
avr_vcd_start(
		avr_vcd_t * vcd)
{
	vcd->start = vcd->avr->cycle;
	avr_vcd_fifo_reset(&vcd->log);

	if (vcd->input) {
		/*
		 * nothing to do here, the first cycle timer will take care
		 * if it.
		 */
		return 0;
	}
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
		fprintf(vcd->output, "%s\n",
				_avr_vcd_get_float_signal_text(s, out));
	}
	fprintf(vcd->output, "$end\n");
	avr_cycle_timer_register(vcd->avr, vcd->period, _avr_vcd_timer, vcd);
	return 0;
}

int
avr_vcd_stop(
		avr_vcd_t * vcd)
{
	avr_cycle_timer_cancel(vcd->avr, _avr_vcd_timer, vcd);
	avr_cycle_timer_cancel(vcd->avr, _avr_vcd_input_timer, vcd);

	avr_vcd_flush_log(vcd);

	if (vcd->input_line)
		free(vcd->input_line);
	vcd->input_line = NULL;
	if (vcd->input)
		fclose(vcd->input);
	vcd->input = NULL;
	if (vcd->output)
		fclose(vcd->output);
	vcd->output = NULL;
	return 0;
}


