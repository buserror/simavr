/*
	sim_irq.c

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
#include "sim_irq.h"


void avr_init_irq(avr_t * avr, avr_irq_t * irq, uint32_t base, uint32_t count)
{
	memset(irq, 0, sizeof(avr_irq_t) * count);

	for (int i = 0; i < count; i++)
		irq[i].irq = base + i;
}

avr_irq_t * avr_alloc_irq(avr_t * avr, uint32_t base, uint32_t count)
{
	avr_irq_t * irq = (avr_irq_t*)malloc(sizeof(avr_irq_t) * count);
	avr_init_irq(avr, irq, base, count);
	return irq;
}

void avr_irq_register_notify(avr_t * avr, avr_irq_t * irq, avr_irq_notify_t notify, void * param)
{
	if (!irq || !notify)
		return;
	
	avr_irq_hook_t *hook = irq->hook;
	while (hook) {
		if (hook->notify == notify && hook->param == param)
			return;	// already there
		hook = hook->next;
	}
	hook = malloc(sizeof(avr_irq_hook_t));
	memset(hook, 0, sizeof(avr_irq_hook_t));
	hook->next = irq->hook;
	hook->notify = notify;
	hook->param = param;
	irq->hook = hook;
}

void avr_raise_irq(avr_t * avr, avr_irq_t * irq, uint32_t value)
{
	if (!irq || irq->value == value)
		return ;
	avr_irq_hook_t *hook = irq->hook;
	while (hook) {
		if (hook->notify) {
			if (hook->busy == 0) {
				hook->busy++;
				hook->notify(avr, irq, value, hook->param);
				hook->busy--;
			}
		}
		hook = hook->next;
	}
	irq->value = value;
}

static void _avr_irq_connect(avr_t * avr, avr_irq_t * irq, uint32_t value, void * param)
{
	avr_irq_t * dst = (avr_irq_t*)param;
	avr_raise_irq(avr, dst, value);
}

void avr_connect_irq(avr_t * avr, avr_irq_t * src, avr_irq_t * dst)
{
	avr_irq_register_notify(avr, src, _avr_irq_connect, dst);
}
