/*
	sim_interrupts.c

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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "sim_interrupts.h"
#include "sim_core.h"

void avr_register_vector(avr_t *avr, avr_int_vector_t * vector)
{
	if (vector->vector) {
		vector->irq.irq = vector->vector;
		avr->vector[vector->vector] = vector;
		if (vector->trace)
			printf("%s register vector %d (enabled %04x:%d)\n", __FUNCTION__, vector->vector, vector->enable.reg, vector->enable.bit);

		if (!vector->enable.reg)
			printf("avr_register_vector: No 'enable' bit on vector %d !\n", vector->vector);
	}
}

int avr_has_pending_interrupts(avr_t * avr)
{
	return avr->pending[0] || avr->pending[1];
}

int avr_is_interrupt_pending(avr_t * avr, avr_int_vector_t * vector)
{
	return avr->pending[vector->vector >> 5] & (1 << (vector->vector & 0x1f));
}

int avr_is_interrupt_enabled(avr_t * avr, avr_int_vector_t * vector)
{
	return avr_regbit_get(avr, vector->enable);
}

int avr_raise_interrupt(avr_t * avr, avr_int_vector_t * vector)
{
	if (!vector || !vector->vector)
		return 0;
	if (vector->trace)
		printf("%s raising %d (enabled %d)\n", __FUNCTION__, vector->vector, avr_regbit_get(avr, vector->enable));
	// always mark the 'raised' flag to one, even if the interrupt is disabled
	// this allow "pooling" for the "raised" flag, like for non-interrupt
	// driven UART and so so. These flags are often "write one to clear"
	if (vector->raised.reg)
		avr_regbit_set(avr, vector->raised);

	// Mark the interrupt as pending
	avr->pending[vector->vector >> 5] |= (1 << (vector->vector & 0x1f));
	avr_raise_irq(&vector->irq, 1);

	// If the interrupt is enabled, attempt to wake the core
	if (avr_regbit_get(avr, vector->enable)) {
		if (!avr->pending_wait)
			avr->pending_wait = 1;		// latency on interrupts ??
		if (avr->state != cpu_Running) {
			if (vector->trace)
				printf("Waking CPU due to interrupt\n");
			avr->state = cpu_Running;	// in case we were sleeping
		}
	}
	// return 'raised' even if it was already pending
	return 1;
}

void avr_clear_interrupt(avr_t * avr, int v)
{
	avr_int_vector_t * vector = avr->vector[v];
	avr->pending[v >> 5] &= ~(1 << (v & 0x1f));
	if (!vector)
		return;
	if (vector->trace)
		printf("%s cleared %d\n", __FUNCTION__, vector->vector);
	avr_raise_irq(&vector->irq, 0);
	if (vector->raised.reg)
		avr_regbit_clear(avr, vector->raised);
}

int avr_clear_interrupt_if(avr_t * avr, avr_int_vector_t * vector, uint8_t old)
{
	if (avr_regbit_get(avr, vector->raised)) {
		avr_clear_interrupt(avr, vector->vector);
		avr_regbit_clear(avr, vector->raised);
		return 1;
	}
	avr_regbit_setto(avr, vector->raised, old);
	return 0;
}

avr_irq_t * avr_get_interrupt_irq(avr_t * avr, uint8_t v)
{
	avr_int_vector_t * vector = avr->vector[v];
	return vector ? &vector->irq : NULL;
}

/*
 * check whether interrupts are pending. If so, check if the interrupt "latency" is reached,
 * and if so triggers the handlers and jump to the vector.
 */
void avr_service_interrupts(avr_t * avr)
{
	if (!avr->sreg[S_I])
		return;

	if (avr_has_pending_interrupts(avr)) {
		if (avr->pending_wait) {
			avr->pending_wait--;
			if (avr->pending_wait == 0) {
				for (int bi = 0; bi < 2; bi++)
					if (avr->pending[bi]) {
						uint32_t map = avr->pending[bi];
						while (map) {
							int bit = ffs(map)-1;
							int v = (bi * 32) + bit;	// vector
							avr_int_vector_t * vector = avr->vector[v];
							// if that single interrupt is masked, ignore it and continue
							if (vector && !avr_regbit_get(avr, vector->enable)) {
								map &= ~(1 << bit);
								continue;
							}
							if (vector && vector->trace)
								printf("%s calling %d\n", __FUNCTION__, v);
							_avr_push16(avr, avr->pc >> 1);
							avr->sreg[S_I] = 0;
							avr->pc = v * avr->vector_size;

							avr_clear_interrupt(avr, v);
							break;
						}
						break;
					}
			}
		} else
			avr->pending_wait = 2;	// for next one...
	}
}

