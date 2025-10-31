#include <stdio.h>
#include <string.h>

#include "tests.h"
#include "sim_avr.h"

#define TIFR0 0x35 // _SFR_IO8(0x15), from iom328pb.h.

/* JSYS means "Jump to System", the way user programs called the Monitor on
 * the TOPS-20 OS.  Here firmware has executed an illegal instruction
 * to request a service from the containing application.
 * The request parameters follow the instruction as a byte sequence:
 *  0: end of request list.
 *  1: sleep until an application-specific condition occurs.
 *  2: output a string that follows immediately.
 * This list might be extended to output variables (byte or word) in
 * various formats, yielding something like printf(), but taking far less
 * firmware space and simulated execution time.
 *
 * On return, a new value has been set for avr->pc, skipping the parameters.
 */

static void jsys_handler(struct avr_irq_t *irq, uint32_t value, void *param)
{
	avr_t    *avr = (avr_t *)param;
	uint8_t  *pc;
	uint16_t  instruction;

	pc = avr->flash + avr->pc;
	instruction = *(uint16_t *)pc;
	if (instruction != 0xff) {
		fprintf(stderr, "Instruction %02x is not JSYS at PC %02x\n",
				instruction, avr->pc);
		return;
	}
	pc += 2;	// Skip over JSYS
	for (;;) {
		switch (*pc) {
		default:
			printf("Ignoring bad command byte %u\n", *pc);
			/* Fall through ... */
		case 0:
			/* End of command list. */

			if (!((uintptr_t)pc & 1))			// Advance to odd byte.
				++pc;
			avr->pc = (uint16_t)(pc - avr->flash) + 1;
			if (avr->state == cpu_Running)		// Signal success.
				avr->state = cpu_StepDone;
			return;
			break;
		case 1:
			/* Sleep, monitor_tifr0() will wake. */

			if (avr_has_pending_interrupts(avr)) {
				/* There are no interrupts enabled, but in general,
				 * sleep with one pending would cause exit from usual
				 * run-loops. */

				avr->state = cpu_StepDone; // Signal to silently continue.
			} else {
				avr->state = cpu_Sleeping;
			}
			break;
		case 2:
			/* Print a string embedded in the flash. */

			++pc;
			printf("%s", (const char *)pc);	/* Can not use return value!. */
			pc += strlen((const char *)pc);
			break;
		}
		++pc;
	}
}

/* The IRQ handler above, may put the AVR to sleep.
 * Wake it, without an interrupt, when OCR0A compare match flag is set.
 */

static void monitor_tifr0(struct avr_irq_t *irq, uint32_t value, void *param)
{
	avr_t    *avr = (avr_t *)param;

	if ((value & 0xff) && avr->state == cpu_Sleeping)
		avr->state = cpu_Running;
}

/* Watch the value of two single-byte counters at the start of RAM. */

static uint16_t counters;

static void monitor_counters(struct avr_irq_t *irq,
							 uint32_t value, void *param)
{
	counters = value;
}

int main(int argc, char **argv) {
	avr_t   *avr;
	uint8_t  inner, outer;

	tests_init(argc, argv);
	avr = tests_init_avr("atmega328_jsys.axf");

	avr_irq_register_notify(avr_get_core_irq(avr, AVR_CORE_BAD_OPCODE),
							jsys_handler, avr);

	avr_irq_register_notify(avr_iomem_getirq(avr, TIFR0, NULL, 1),
							monitor_tifr0, avr);

	/* Use an SEAM IRQ to watch two variables in the AVR. */

	avr_irq_register_notify(avr_get_memory_irq(avr, 0x100, 1),
							monitor_counters, avr);

	/* Run ... */

	tests_assert_uart_receive_avr(avr, 300000, "", '0');

	inner = counters & 0xff; // Copy of first byte of RAM.
	outer = counters >> 8;

	printf("Executed %lu cycles, inner %u, outer %u.\n",
		   avr->cycle, inner, outer);
	if (inner == 20 && outer == 20)
		tests_success();
	return 0;
}
