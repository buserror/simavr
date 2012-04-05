/*
	thermistor.c

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

#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "avr_adc.h"

#include "thermistor.h"

/*
 * called when a byte is send via the uart on the AVR
 */
static void thermistor_in_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	thermistor_p p = (thermistor_p)param;
	avr_adc_mux_t v = *((avr_adc_mux_t*)&value);

//	printf("%s(%2d/%2d)\n", __func__, p->adc_mux_number, v.src);

	if (v.src != p->adc_mux_number)
		return;

	short *t = p->table;
	for (int ei = 0; ei < p->table_entries; ei++, t += 2) {
		if (t[1] < p->current) {
			printf("%s(%2d) %.2f matches %3dC is %d adc\n", __func__, v.src,
					p->current, t[1], t[0] / p->oversampling);
			avr_raise_irq(p->irq + IRQ_TERM_ADC_VALUE_OUT, t[0] / p->oversampling);
			return;
		}
	}
	printf("%s(%d) temperature out of range (%.2f), we're screwed\n",
			__func__, p->adc_mux_number, p->current);
}

static const char * irq_names[IRQ_TERM_COUNT] = {
	[IRQ_TERM_ADC_TRIGGER_IN] = "8<thermistor.trigger",
	[IRQ_TERM_TEMP_VALUE_OUT] = "16>thermistor.out",
};

void
thermistor_init(
		struct avr_t * avr,
		thermistor_p p,
		int adc_mux_number,
		short * table,
		int	table_entries,
		int oversampling,
		float start_temp )
{
	p->avr = avr;
	p->irq = avr_alloc_irq(&avr->irq_pool, 0, IRQ_TERM_COUNT, irq_names);
	avr_irq_register_notify(p->irq + IRQ_TERM_ADC_TRIGGER_IN, thermistor_in_hook, p);

	p->oversampling = oversampling;
	p->table = table;
	p->table_entries = table_entries;
	p->adc_mux_number = adc_mux_number;
	p->current = p->target = start_temp;

	avr_irq_t * src = avr_io_getirq(p->avr, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_OUT_TRIGGER);
	avr_irq_t * dst = avr_io_getirq(p->avr, AVR_IOCTL_ADC_GETIRQ, adc_mux_number);
	if (src && dst) {
		avr_connect_irq(src, p->irq + IRQ_TERM_ADC_TRIGGER_IN);
		avr_connect_irq(p->irq + IRQ_TERM_ADC_VALUE_OUT, dst);
	}
	printf("%s on ADC %d start %.2f\n", __func__, adc_mux_number, p->current);
}

void
thermistor_set_temp(
		thermistor_p t,
		float temp )
{

}
