/*
	sim_interrupts.c

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
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "sim_interrupts.h"
#include "sim_avr.h"
#include "sim_core.h"

DEFINE_FIFO(avr_int_vector_p, avr_int_pending);

void
avr_interrupt_init(
		avr_t * avr )
{
	avr_int_table_p table = &avr->interrupts;
	memset(table, 0, sizeof(*table));

	static const char *names[] = { ">avr.int.pending", ">avr.int.running" };
	avr_init_irq(&avr->irq_pool, table->irq,
			0, // base number
			AVR_INT_IRQ_COUNT, names);
}

void
avr_interrupt_reset(
		avr_t * avr )
{
	avr_int_table_p table = &avr->interrupts;

	table->running_ptr = 0;
	avr_int_pending_reset(&table->pending);
	avr->interrupt_state = 0;
	for (int i = 0; i < table->vector_count; i++)
		table->vector[i]->pending = 0;
}

void
avr_register_vector(
		avr_t *avr,
		avr_int_vector_t * vector)
{
	if (!vector->vector)
		return;

	avr_int_table_p table = &avr->interrupts;

	char name0[48], name1[48];
	sprintf(name0, ">avr.int.%02x.pending", vector->vector);
	sprintf(name1, ">avr.int.%02x.running", vector->vector);
	const char *names[2] = { name0, name1 };
	avr_init_irq(&avr->irq_pool, vector->irq,
			vector->vector * 256, // base number
			AVR_INT_IRQ_COUNT, names);
	table->vector[table->vector_count++] = vector;
	if (vector->vector > table->max_vector)
		table->max_vector = vector->vector;
	if (vector->trace)
		printf("IRQ%d registered (enabled %04x:%d)\n",
			vector->vector, vector->enable.reg, vector->enable.bit);

	if (!vector->enable.reg)
		AVR_LOG(avr, LOG_WARNING, "IRQ%d No 'enable' bit !\n",
			vector->vector);
}

void
avr_cpuint_set_lvl1vec(avr_t *avr, uint8_t vector)
{
	avr->interrupts.cpuint_lvl1vec = vector;
}

void
avr_cpuint_set_lvl0pri(avr_t *avr, uint8_t pri)
{
	avr->interrupts.cpuint_lvl0pri = pri;
}

void
avr_cpuint_set_lvl0rr(avr_t *avr, uint8_t enabled)
{
	avr->interrupts.cpuint_lvl0rr = !!enabled;
}

uint8_t
avr_cpuint_get_status(avr_t *avr)
{
	return avr->interrupts.cpuint_status;
}

int
avr_has_pending_interrupts(
		avr_t * avr)
{
	avr_int_table_p table = &avr->interrupts;
	return !avr_int_pending_isempty(&table->pending);
}

int
avr_is_interrupt_pending(
		avr_t * avr,
		avr_int_vector_t * vector)
{
	return vector->pending;
}

int
avr_is_interrupt_enabled(
		avr_t * avr,
		avr_int_vector_t * vector)
{
	return avr_regbit_get(avr, vector->enable);
}

int
avr_raise_interrupt(
		avr_t * avr,
		avr_int_vector_t * vector)
{
	if (!vector || !vector->vector)
		return 0;

	if (vector->trace)
		printf("IRQ%d raising (enabled %d)\n",
			vector->vector, avr_regbit_get(avr, vector->enable));

	// always mark the 'raised' flag to one, even if the interrupt is disabled
	// this allow "polling" for the "raised" flag, like for non-interrupt
	// driven UART and so so. These flags are often "write one to clear"
	if (vector->raised.reg)
		avr_regbit_set(avr, vector->raised);

	if (vector->pending) {
		if (vector->trace)
			printf("IRQ%d:I=%d already raised (enabled %d) (cycle %lld pc 0x%x)\n",
				vector->vector, !!avr->sreg[S_I], avr_regbit_get(avr, vector->enable),
				(long long int)avr->cycle, avr->pc);

        return 0;
	}

	avr_raise_irq(vector->irq + AVR_INT_IRQ_PENDING, 1);
	avr_raise_irq(avr->interrupts.irq + AVR_INT_IRQ_PENDING, vector->vector);

	// If the interrupt is enabled, attempt to wake the core
	if (avr_regbit_get(avr, vector->enable)) {
		// Mark the interrupt as pending
		vector->pending = 1;

		avr_int_table_p table = &avr->interrupts;

		avr_int_pending_write(&table->pending, vector);

		// Modern CPUINT: NMI must wake the core even with I=0, and a level-1
		// request may need to preempt a running level-0 handler, so arm the
		// service check regardless of the I bit (the modern service routine
		// does the real gating). Classic cores keep the I-gated behaviour.
		if (avr->arch.flags & AVR_ARCH_F_CPUINT) {
			if (avr->interrupt_state == 0)
				avr->interrupt_state = 1;
		} else if (avr->sreg[S_I] && avr->interrupt_state == 0)
			avr->interrupt_state = 1;
		if (avr->state == cpu_Sleeping) {
			if (vector->trace)
				printf("IRQ%d Waking CPU due to interrupt\n",
					vector->vector);
			avr->state = cpu_Running;	// in case we were sleeping
		}
	}
	// return 'raised' even if it was already pending
	return 1;
}

void
avr_clear_interrupt(
		avr_t * avr,
		avr_int_vector_t * vector)
{
	if (!vector)
		return;
	if (vector->trace)
		printf("IRQ%d cleared\n", vector->vector);
	vector->pending = 0;

	avr_raise_irq(vector->irq + AVR_INT_IRQ_PENDING, 0);
	avr_raise_irq_float(avr->interrupts.irq + AVR_INT_IRQ_PENDING,
			avr_has_pending_interrupts(avr) ?
					avr_int_pending_read_at(
							&avr->interrupts.pending, 0)->vector : 0,
							avr_has_pending_interrupts(avr));

	if (vector->raised.reg && !vector->raise_sticky)
		avr_regbit_clear(avr, vector->raised);
}

int
avr_clear_interrupt_if(
		avr_t * avr,
		avr_int_vector_t * vector,
		uint8_t old)
{
	avr_raise_irq(avr->interrupts.irq + AVR_INT_IRQ_PENDING,
			avr_has_pending_interrupts(avr));
	if (avr_regbit_get(avr, vector->raised)) {
		avr_clear_interrupt(avr, vector);
		return 1;
	}
	avr_regbit_setto(avr, vector->raised, old);
	return 0;
}

avr_irq_t *
avr_get_interrupt_irq(
		avr_t * avr,
		uint8_t v)
{
	avr_int_table_p table = &avr->interrupts;
	if (v == AVR_INT_ANY)
		return table->irq;
	for (int i = 0; i < table->vector_count; i++)
		if (table->vector[i]->vector == v)
			return table->vector[i]->irq;
	return NULL;
}

/* this is called upon RETI. */
void
avr_interrupt_reti(
		struct avr_t * avr)
{
	avr_int_table_p table = &avr->interrupts;
	if (table->running_ptr) {
		avr_int_vector_t * vector = table->running[--table->running_ptr];
		avr_raise_irq(vector->irq + AVR_INT_IRQ_RUNNING, 0);
	}
	avr_raise_irq(table->irq + AVR_INT_IRQ_RUNNING,
			table->running_ptr > 0 ?
					table->running[table->running_ptr-1]->vector : 0);

	// Modern CPUINT: RETI does not touch the I bit (unlike classic AVR, where
	// the RETI opcode handler sets it). Instead it clears the highest-priority
	// execution-level flag in CPUINT.STATUS. Re-arm the service check so any
	// interrupt that was blocked by this level can now be considered.
	if (avr->arch.flags & AVR_ARCH_F_CPUINT) {
		if (table->cpuint_status & AVR_CPUINT_NMIEX)
			table->cpuint_status &= ~AVR_CPUINT_NMIEX;
		else if (table->cpuint_status & AVR_CPUINT_LVL1EX)
			table->cpuint_status &= ~AVR_CPUINT_LVL1EX;
		else if (table->cpuint_status & AVR_CPUINT_LVL0EX)
			table->cpuint_status &= ~AVR_CPUINT_LVL0EX;
		if (avr->interrupt_state == 0 && avr_has_pending_interrupts(avr))
			avr->interrupt_state = 1;
	}
}

/*
 * Level of a vector under the modern CPUINT priority scheme:
 *   2 = non-maskable, 1 = the user-selected level-1 vector, 0 = normal.
 */
static int
_avr_cpuint_level(avr_int_table_p table, avr_int_vector_t * v)
{
	if (v->nmi)
		return 2;
	if (table->cpuint_lvl1vec && v->vector == table->cpuint_lvl1vec)
		return 1;
	return 0;
}

/*
 * Modern CPUINT interrupt dispatch (AVR_ARCH_F_CPUINT). Differs from classic:
 *   - The I bit is NOT cleared on entry and RETI does NOT set it; nesting is
 *     governed by the execution-level flags in CPUINT.STATUS.
 *   - NMI ignores the I bit and outranks everything; a level-1 vector can
 *     preempt a running level-0 handler.
 *   - Level-0 scheduling honours LVL0PRI (modified static) and LVL0RR
 *     (round robin); the default (LVL0PRI=0) is lowest-vector-first.
 *   - Interrupt flags are sticky (cleared by software), handled per-vector.
 */
static void
avr_service_interrupts_modern(avr_t * avr)
{
	// Interrupts are ignored for the duration of the CCP protected window.
	if (unlikely(avr->arch.ccp_window))
		return;

	// Honour the post-SEI one-instruction latency, like the classic path.
	if (avr->interrupt_state < 0) {
		avr->interrupt_state++;
		if (avr->interrupt_state == 0)
			avr->interrupt_state = avr_has_pending_interrupts(avr);
		return;
	}
	if (!avr->interrupt_state)
		return;

	avr_int_table_p table = &avr->interrupts;
	uint8_t status = table->cpuint_status;
	int M = table->max_vector ? table->max_vector + 1 : 1;

	int cnt = avr_int_pending_get_read_size(&table->pending);
	avr_int_vector_t * best = NULL;
	int best_level = -1;
	int best_rank = 0;
	int best_idx = 0;

	for (int ii = 0; ii < cnt; ii++) {
		avr_int_vector_t * v = avr_int_pending_read_at(&table->pending, ii);
		if (!v->pending || !avr_regbit_get(avr, v->enable))
			continue;
		int level = _avr_cpuint_level(table, v);

		// Can this level preempt what is currently executing?
		if (level == 2) {
			if (status & AVR_CPUINT_NMIEX)
				continue;
		} else if (level == 1) {
			if (!avr->sreg[S_I])
				continue;
			if (status & (AVR_CPUINT_NMIEX | AVR_CPUINT_LVL1EX))
				continue;
		} else {
			if (!avr->sreg[S_I])
				continue;
			if (status & (AVR_CPUINT_NMIEX | AVR_CPUINT_LVL1EX | AVR_CPUINT_LVL0EX))
				continue;
		}

		// Rank within a level: smaller = higher priority. For level 0 this
		// implements LVL0PRI wrapping (vector LVL0PRI+1 is highest); for NMI
		// and level 1 it is simply the lowest vector address.
		int rank;
		if (level == 0)
			rank = (v->vector - 1 - table->cpuint_lvl0pri + 2 * M) % M;
		else
			rank = v->vector;

		if (level > best_level ||
				(level == best_level && rank < best_rank)) {
			best = v;
			best_level = level;
			best_rank = rank;
			best_idx = ii;
		}
	}

	if (!best) {
		// Nothing serviceable right now (blocked by an executing level, or I=0
		// for maskable vectors). RETI / a new raise will re-arm us.
		avr->interrupt_state = 0;
		return;
	}

	// Remove the selected vector from the pending fifo. Since we pick by
	// priority rather than fifo order, it may not be at the front; dequeue the
	// front and, if different, drop it back into the selected vector's slot
	// (same scheme as the classic path).
	avr_int_vector_p fifo_front = avr_int_pending_read(&table->pending);
	if (fifo_front != best)
		table->pending.buffer[(table->pending.read + best_idx - 1) %
				avr_int_pending_fifo_size] = fifo_front;

	if (best->trace)
		printf("IRQ%d calling (modern, level %d)\n", best->vector, best_level);

	_avr_push_addr(avr, avr->pc);
	// Mark the execution level. The I bit is intentionally left untouched.
	if (best_level == 2)
		table->cpuint_status |= AVR_CPUINT_NMIEX;
	else if (best_level == 1)
		table->cpuint_status |= AVR_CPUINT_LVL1EX;
	else {
		table->cpuint_status |= AVR_CPUINT_LVL0EX;
		// Round robin: the acknowledged vector becomes lowest priority.
		if (table->cpuint_lvl0rr)
			table->cpuint_lvl0pri = best->vector;
	}
	avr->pc = best->vector * avr->vector_size;

	avr_raise_irq(best->irq + AVR_INT_IRQ_RUNNING, 1);
	avr_raise_irq(table->irq + AVR_INT_IRQ_RUNNING, best->vector);
	if (table->running_ptr == ARRAY_SIZE(table->running)) {
		AVR_LOG(avr, LOG_ERROR, "%s ran out of nested stack! vector=%d\n",
				__func__, best->vector);
	} else {
		table->running[table->running_ptr++] = best;
	}
	avr_clear_interrupt(avr, best);	// clears 'pending'; sticky flag stays set

	// Keep evaluating while anything is still pending (it may be serviceable
	// now, or blocked until a RETI).
	avr->interrupt_state = avr_has_pending_interrupts(avr) ? 1 : 0;
}

/*
 * check whether interrupts are pending. If so, check if the interrupt "latency" is reached,
 * and if so triggers the handlers and jump to the vector.
 */
void
avr_service_interrupts(
		avr_t * avr)
{
	if (avr->arch.flags & AVR_ARCH_F_CPUINT) {
		avr_service_interrupts_modern(avr);
		return;
	}

	if (!avr->sreg[S_I] || !avr->interrupt_state)
		return;

	if (avr->interrupt_state < 0) {
		avr->interrupt_state++;
		if (avr->interrupt_state == 0)
			avr->interrupt_state = avr_has_pending_interrupts(avr);
		return;
	}

	avr_int_table_p table = &avr->interrupts;

	// how many are pending...
	int cnt = avr_int_pending_get_read_size(&table->pending);
	// locate the highest priority one
	int min = 0xff;
	int mini = 0;
	for (int ii = 0; ii < cnt; ii++) {
		avr_int_vector_t * v = avr_int_pending_read_at(&table->pending, ii);
		if (v->vector < min) {
			min = v->vector;
			mini = ii;
		}
	}
	avr_int_vector_t * vector = avr_int_pending_read_at(&table->pending, mini);

	// it's possible that the vector being serviced is not at the front of the fifo, because we process interrupts based
	// on vector priority rather than position in the fifo. if this is the case, we need to manually swap the vector
	// being serviced with the vector at the front of the fifo so that the vector at the front of the fifo can be
	// serviced in a following iteration.
	avr_int_vector_p fifo_front = avr_int_pending_read(&table->pending);
	if (fifo_front->vector != vector->vector) {
		// the read into fifo_front above has incremented pending.read, so now mini points 1 beyond the desired
		// destination for the swap.
		table->pending.buffer[(table->pending.read + mini - 1) % avr_int_pending_fifo_size] = fifo_front;
	}

	// if that single interrupt is masked, ignore it and continue
	// could also have been disabled, or cleared
	if (!avr_regbit_get(avr, vector->enable) || !vector->pending) {
		vector->pending = 0;
		avr->interrupt_state = avr_has_pending_interrupts(avr);
	} else {
		if (vector->trace)
			printf("IRQ%d calling\n", vector->vector);
		_avr_push_addr(avr, avr->pc);
		avr_sreg_set(avr, S_I, 0);
		avr->pc = vector->vector * avr->vector_size;

		avr_raise_irq(vector->irq + AVR_INT_IRQ_RUNNING, 1);
		avr_raise_irq(table->irq + AVR_INT_IRQ_RUNNING, vector->vector);
		if (table->running_ptr == ARRAY_SIZE(table->running)) {
			AVR_LOG(avr, LOG_ERROR, "%s run out of nested stack! vector=%d", __func__, vector->vector);
		} else {
			table->running[table->running_ptr++] = vector;
		}
		avr_clear_interrupt(avr, vector);
	}
}

