/*
 * The interrupt response time: from an interrupt being raised to the core
 * being at its vector. The datasheet (ATtiny25/45/85, "Interrupt Response
 * Time"): four cycles, and four more if the interrupt wakes the core.
 */
#include <stdio.h>
#include "tests.h"
#include "sim_interrupts.h"

#define VECTOR	10	/* TIMER0_COMPA */

static avr_cycle_count_t raised;

static void pending(struct avr_irq_t *irq, uint32_t value, void *param)
{
	avr_t *avr = param;
	if (value)
		raised = avr->cycle;
}

int main(int argc, char **argv) {
	tests_init(argc, argv);
	avr_t *avr = tests_init_avr("attiny85_interrupt_response.axf");

	avr_irq_register_notify(avr_get_interrupt_irq(avr, VECTOR) + AVR_INT_IRQ_PENDING,
				pending, avr);

	long response[2];
	int n = 0;
	uint32_t at = VECTOR * avr->vector_size, last = avr->pc;
	while (n < 2 && avr->cycle < 1000000) {
		int state = avr_run(avr);
		if (state == cpu_Done || state == cpu_Crashed)
			break;
		if (avr->pc == at && last != at)
			response[n++] = avr->cycle - raised;
		last = avr->pc;
	}
	if (n < 2)
		fail("the interrupt was taken %d times, not 2", n);
	if (response[0] != 4)
		fail("running: response in %ld cycles, not 4", response[0]);
	if (response[1] != 8)
		fail("asleep: response in %ld cycles, not 8", response[1]);
	tests_success();
	return 0;
}
