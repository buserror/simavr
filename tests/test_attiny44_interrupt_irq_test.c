/*
	test_attiny44_interrupt_irq_test.c

	Copyright 2022 Giles Atkinson

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
#include <stdlib.h>
#include <string.h>
#include "sim_irq.h"
#include "sim_interrupts.h"
#include "tests.h"

/* Accumulate log of events for comparison at the end. */

static char  log[512];
static char *fill = log;

#define LOG(...) \
    (fill += snprintf(fill, (log + sizeof log) - fill, __VA_ARGS__))

#define GPIOR1 ((avr_io_addr_t)0x34) // Unused I/O register.

/* Callbacks for interrupt events. */

static void any_pending(struct avr_irq_t *irq, uint32_t value, void *param)
{
	LOG("P-%d%s ", value, irq->flags & IRQ_FLAG_FLOATING ? "f": "");
}

static void any_running(struct avr_irq_t *irq, uint32_t value, void *param)
{
	LOG("R-%d ", value);
}

static void ext_pending(struct avr_irq_t *irq, uint32_t value, void *param)
{
	LOG("PX-%d ", value);
}

static void ext_running(struct avr_irq_t *irq, uint32_t value, void *param)
{
	LOG("RX-%d ", value);
}

static void pc1_pending(struct avr_irq_t *irq, uint32_t value, void *param)
{
	LOG("PC-%d ", value);
}

static void pc1_running(struct avr_irq_t *irq, uint32_t value, void *param)
{
	LOG("RC-%d ", value);
}

static void adc_pending(struct avr_irq_t *irq, uint32_t value, void *param)
{
	LOG("PA-%d ", value);
}

static void adc_running(struct avr_irq_t *irq, uint32_t value, void *param)
{
	LOG("RA-%d ", value);
}

static void gpior1_change(struct avr_t *avr, avr_io_addr_t addr,
                          uint8_t v, void *param)
{
	LOG("*-%d ", v);
}

static const char *expected =
    "PX-1 P-1 PC-1 P-3 PA-1 P-13 "		// Raise three interrupts.
    "RX-1 R-1 PX-0 P-3f *-1 RX-0 R-0 "  // External (INT0) interrupt runs.
    "RC-1 R-3 PC-0 P-13f *-2 RC-0 R-0 " // Pin change interrupt runs.
    "RA-1 R-13 PA-0 P-0 *-3 RA-0 R-0 "  // ADC interrupt runs.
    "PX-1 P-1 PC-1 P-3 PA-1 P-13 "		// Second round: raise three.
    "RX-1 R-1 PX-0 P-3f *-1 "			// External (INT0) interrupt runs.
    "RC-1 R-3 PC-0 P-13f *-2 "			// Pin change interrupt runs.
    "RA-1 R-13 PA-0 P-0 *-3 RA-0 "		// ADC interrupt runs.
	"R-3 RC-0 R-1 RX-0 R-0 ";			// Unwind stack.

int main(int argc, char **argv) {
	avr_t                *avr;
	avr_irq_t            *irq;

	tests_init(argc, argv);
	avr = tests_init_avr("attiny44_interrupt_irq_test.axf");

	/* Request callbacks on events in the interrupt code.. */

	irq = avr_get_interrupt_irq(avr, AVR_INT_ANY);
	avr_irq_register_notify(irq + AVR_INT_IRQ_PENDING, any_pending, NULL);
	avr_irq_register_notify(irq + AVR_INT_IRQ_RUNNING, any_running, NULL);
	irq = avr_get_interrupt_irq(avr, 1);  // INT0 - pin PB2
	avr_irq_register_notify(irq + AVR_INT_IRQ_PENDING, ext_pending, NULL);
	avr_irq_register_notify(irq + AVR_INT_IRQ_RUNNING, ext_running, NULL);
	irq = avr_get_interrupt_irq(avr, 3);  // PCINT1 - Port B
	avr_irq_register_notify(irq + AVR_INT_IRQ_PENDING, pc1_pending, NULL);
	avr_irq_register_notify(irq + AVR_INT_IRQ_RUNNING, pc1_running, NULL);
	irq = avr_get_interrupt_irq(avr, 13); // ADC
	avr_irq_register_notify(irq + AVR_INT_IRQ_PENDING, adc_pending, NULL);
	avr_irq_register_notify(irq + AVR_INT_IRQ_RUNNING, adc_running, NULL);

	/* Watch GPIOR1 for interrupt handler activity. */

	avr_register_io_write(avr, GPIOR1, gpior1_change, NULL);

	/* Run program and check results. */

	if (tests_run_test(avr, 100000) == LJR_CYCLE_TIMER)
		fail("Timed out\n");
	if (strcmp(expected, log))
		fail("\nInternal log: %s.\n    Expected: %s.\n", log, expected);
	tests_success();
	return 0;
}
