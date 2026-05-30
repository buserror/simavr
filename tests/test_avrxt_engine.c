/*
	test_avrxt_engine.c

	Standalone unit test for the Phase 2 engine additions:
	  - AVRxt instruction timing (cycle counts)
	  - CCP (Configuration Change Protection) unlock window
	  - flash-mapped-into-data-space reads

	These are exercised by allocating a real core and toggling the modern
	architecture flags directly, then hand-assembling opcodes into flash and
	single-stepping with avr_run_one(). This avoids needing a full modern core
	(that lands in Phase 5).

	Build/run from the tests directory:
	  cc -I../simavr/sim -o test_avrxt_engine test_avrxt_engine.c \
	     -L../simavr/obj-* -lsimavr -lm && ./test_avrxt_engine
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_avr.h"
#include "sim_core.h"
#include "sim_interrupts.h"
#include "sim_regbit.h"

static int failures;

static void check(const char *what, long got, long want)
{
	if (got != want) {
		printf("  FAIL: %-40s got %ld, want %ld\n", what, got, want);
		failures++;
	} else {
		printf("  ok:   %-40s = %ld\n", what, got);
	}
}

// Put a 16-bit opcode at word address pc/2.
static void put16(avr_t *avr, uint32_t byteaddr, uint16_t op)
{
	avr->flash[byteaddr] = op & 0xff;
	avr->flash[byteaddr + 1] = op >> 8;
}

// Step one instruction; return cycles consumed.
static avr_cycle_count_t step(avr_t *avr)
{
	avr_cycle_count_t before = avr->cycle;
	avr->pc = avr_run_one(avr);
	return avr->cycle - before;
}

// Run one instruction at a fixed pc=0 and report its cycle count, for both
// classic and AVRxt timing, given a 16-bit opcode (and optional 2nd word).
static avr_cycle_count_t cycles_for(avr_t *avr, uint16_t op, int has_word2,
									uint16_t word2, int xt)
{
	if (xt)
		avr->arch.flags |= AVR_ARCH_F_XT_TIMING;
	else
		avr->arch.flags &= ~AVR_ARCH_F_XT_TIMING;
	avr->pc = 0;
	avr->cycle = 0;
	put16(avr, 0, op);
	if (has_word2)
		put16(avr, 2, word2);
	avr->pc = 0;
	avr_cycle_count_t before = avr->cycle;
	avr->pc = avr_run_one(avr);
	return avr->cycle - before;
}

int main(void)
{
	avr_t *avr = avr_make_mcu_by_name("attiny85");
	if (!avr) { printf("cannot make core\n"); return 2; }
	avr->log = LOG_ERROR;
	avr_init(avr);
	// Give ourselves a sane stack and known register state.
	avr->data[avr->arch.sp_addr] = 0xff;
	avr->data[avr->arch.sp_addr + 1] = 0x02;

	printf("== AVRxt instruction timing ==\n");
	// PUSH r0 (0x920f): AVRe 2, AVRxt 1.
	check("PUSH classic", cycles_for(avr, 0x920f, 0, 0, 0), 2);
	check("PUSH avrxt",   cycles_for(avr, 0x920f, 0, 0, 1), 1);
	// ST Z, r0 (0x8200): AVRe 2, AVRxt 1.
	check("ST Z classic", cycles_for(avr, 0x8200, 0, 0, 0), 2);
	check("ST Z avrxt",   cycles_for(avr, 0x8200, 0, 0, 1), 1);
	// STD Z+1, r0 (0x8201): AVRe 2, AVRxt 1.
	check("STD Z+1 classic", cycles_for(avr, 0x8201, 0, 0, 0), 2);
	check("STD Z+1 avrxt",   cycles_for(avr, 0x8201, 0, 0, 1), 1);
	// LDD r0, Z+1 (0x8001): load stays 2 on both.
	check("LDD Z+1 classic", cycles_for(avr, 0x8001, 0, 0, 0), 2);
	check("LDD Z+1 avrxt",   cycles_for(avr, 0x8001, 0, 0, 1), 2);
	// LDS r0, 0x0100 (0x9000 + word): AVRe 2, AVRxt 3.
	check("LDS classic", cycles_for(avr, 0x9000, 1, 0x0100, 0), 2);
	check("LDS avrxt",   cycles_for(avr, 0x9000, 1, 0x0100, 1), 3);
	// SBI 0x05,0 (0x9a28): AVRe 2, AVRxt 1.
	check("SBI classic", cycles_for(avr, 0x9a28, 0, 0, 0), 2);
	check("SBI avrxt",   cycles_for(avr, 0x9a28, 0, 0, 1), 1);
	// CBI 0x05,0 (0x9828): AVRe 2, AVRxt 1.
	check("CBI classic", cycles_for(avr, 0x9828, 0, 0, 0), 2);
	check("CBI avrxt",   cycles_for(avr, 0x9828, 0, 0, 1), 1);
	// RCALL .0 (0xd000): AVRe 3, AVRxt 2.
	check("RCALL classic", cycles_for(avr, 0xd000, 0, 0, 0), 3);
	check("RCALL avrxt",   cycles_for(avr, 0xd000, 0, 0, 1), 2);
	// CALL 0x000100 (0x940e + word): AVRe 4, AVRxt 3.
	check("CALL classic", cycles_for(avr, 0x940e, 1, 0x0080, 0), 4);
	check("CALL avrxt",   cycles_for(avr, 0x940e, 1, 0x0080, 1), 3);
	// ICALL (0x9509): AVRe 3, AVRxt 2.
	check("ICALL classic", cycles_for(avr, 0x9509, 0, 0, 0), 3);
	check("ICALL avrxt",   cycles_for(avr, 0x9509, 0, 0, 1), 2);
	// Unaffected control case: POP (0x900f) stays 2 on both.
	check("POP classic", cycles_for(avr, 0x900f, 0, 0, 0), 2);
	check("POP avrxt",   cycles_for(avr, 0x900f, 0, 0, 1), 2);

	printf("== CCP window ==\n");
	avr->arch.ccp_window = 0;
	check("ccp initially closed", avr_ccp_io_write_enabled(avr), 0);
	avr_ccp_write(avr, AVR_CCP_IOREG);
	check("ccp open after IOREG sig", avr_ccp_io_write_enabled(avr) > 0, 1);
	avr_ccp_write(avr, 0x12 /* wrong */);
	// wrong signature is a no-op; the previously-armed window is unaffected
	check("ccp still open (window>0)", avr_ccp_io_write_enabled(avr) > 0, 1);

	/*
	 * Datasheet (DS40002205A 8.5.7.1): the protected write must occur "within
	 * four instructions" *following* the CCP write. Model the exact decrement
	 * stream as avr_run_one() applies it (decrement at end of each instruction,
	 * the CCP-writing instruction being k=0), and count how many subsequent
	 * instructions see the window open. Must be exactly AVR_CCP_WINDOW.
	 */
	avr->arch.ccp_window = 0;
	avr_ccp_write(avr, AVR_CCP_IOREG);                 // k=0 body: arm
	if (avr->arch.ccp_window) avr->arch.ccp_window--;  // k=0 end-of-instr decrement
	avr->pc = 0; put16(avr, 0, 0x0000);                // a NOP to step on
	int open = 0;
	for (int k = 1; k <= AVR_CCP_WINDOW + 2; k++) {
		int en = avr_ccp_io_write_enabled(avr);        // checked during instr body
		avr->pc = 0; step(avr);                        // engine decrements at end
		if (en) open++; else break;
	}
	check("window open for exactly AVR_CCP_WINDOW instrs after write",
			open, AVR_CCP_WINDOW);

	printf("== flash mapped into data space ==\n");
	avr->arch.flashmap_start = 0x8000;
	avr->flash[0x0010] = 0xA5;
	avr->flash[0x0011] = 0x5A;
	// LDS r0, 0x8010  -> should read flash[0x10] = 0xA5
	avr->pc = 0; avr->cycle = 0;
	put16(avr, 0, 0x9000); put16(avr, 2, 0x8010);
	avr->data[0] = 0;
	avr->pc = 0;
	avr->pc = avr_run_one(avr);
	check("LDS from mapped flash", avr->data[0], 0xA5);

	printf("== CPUINT interrupt controller ==\n");
	{
		const uint16_t REN = 0x40, RRAISE = 0x41;   // fake enable/flag registers
		// vector 1 = NMI, vectors 5/10/20 = normal (5 used as level-1 candidate)
		static avr_int_vector_t vnmi, v5, v10, v20;
		vnmi.vector = 1; vnmi.nmi = 1; vnmi.raise_sticky = 1;
		vnmi.enable = (avr_regbit_t)AVR_IO_REGBIT(REN, 3);
		vnmi.raised = (avr_regbit_t)AVR_IO_REGBIT(RRAISE, 3);
		v5.vector = 5; v5.raise_sticky = 1;
		v5.enable = (avr_regbit_t)AVR_IO_REGBIT(REN, 0);
		v5.raised = (avr_regbit_t)AVR_IO_REGBIT(RRAISE, 0);
		v10.vector = 10; v10.raise_sticky = 1;
		v10.enable = (avr_regbit_t)AVR_IO_REGBIT(REN, 1);
		v10.raised = (avr_regbit_t)AVR_IO_REGBIT(RRAISE, 1);
		v20.vector = 20; v20.raise_sticky = 1;
		v20.enable = (avr_regbit_t)AVR_IO_REGBIT(REN, 2);
		v20.raised = (avr_regbit_t)AVR_IO_REGBIT(RRAISE, 2);
		avr_register_vector(avr, &vnmi);
		avr_register_vector(avr, &v5);
		avr_register_vector(avr, &v10);
		avr_register_vector(avr, &v20);
		avr->arch.flags |= AVR_ARCH_F_CPUINT | AVR_ARCH_F_MODERN;
		int vs = avr->vector_size;

		#define INT_RESET() do { \
			avr_interrupt_reset(avr); \
			avr->interrupts.cpuint_status = 0; \
			avr->interrupts.cpuint_lvl1vec = 0; \
			avr->interrupts.cpuint_lvl0pri = 0; \
			avr->interrupts.cpuint_lvl0rr = 0; \
			avr->interrupts.running_ptr = 0; \
			avr->arch.ccp_window = 0; \
			avr->data[REN] = 0xff; avr->data[RRAISE] = 0; \
		} while (0)

		// A. Level-0 dispatch: jumps to vector, sets LVL0EX, leaves I set.
		INT_RESET();
		avr->sreg[S_I] = 1;
		avr->pc = 0x100;
		avr_raise_interrupt(avr, &v10);
		avr_service_interrupts(avr);
		check("LVL0 dispatch -> vector*vsize", avr->pc, 10 * vs);
		check("LVL0 sets LVL0EX", avr_cpuint_get_status(avr) & AVR_CPUINT_LVL0EX ? 1 : 0, 1);
		check("entry does NOT clear I", avr->sreg[S_I], 1);
		check("flag stays raised (sticky)", avr_regbit_get(avr, v10.raised), 1);
		// RETI clears the level flag, leaves I alone.
		avr_interrupt_reti(avr);
		check("RETI clears LVL0EX", avr_cpuint_get_status(avr) & AVR_CPUINT_LVL0EX ? 1 : 0, 0);
		check("RETI does NOT touch I", avr->sreg[S_I], 1);

		// B. NMI is serviced even with global interrupts disabled (I=0).
		INT_RESET();
		avr->sreg[S_I] = 0;
		avr->pc = 0x100;
		avr_raise_interrupt(avr, &vnmi);
		avr_service_interrupts(avr);
		check("NMI dispatched with I=0", avr->pc, 1 * vs);
		check("NMI sets NMIEX", avr_cpuint_get_status(avr) & AVR_CPUINT_NMIEX ? 1 : 0, 1);

		// C. Level-1 preempts a running level-0 handler.
		INT_RESET();
		avr->sreg[S_I] = 1;
		avr->interrupts.cpuint_lvl1vec = 5;             // v5 is now level 1
		avr->interrupts.cpuint_status = AVR_CPUINT_LVL0EX;  // pretend in a LVL0 ISR
		avr->pc = 0x100;
		avr_raise_interrupt(avr, &v5);
		avr_service_interrupts(avr);
		check("LVL1 preempts LVL0", avr->pc, 5 * vs);
		check("LVL1 sets LVL1EX", avr_cpuint_get_status(avr) & AVR_CPUINT_LVL1EX ? 1 : 0, 1);

		// D. Level-0 does NOT preempt a running level-0 handler.
		INT_RESET();
		avr->sreg[S_I] = 1;
		avr->interrupts.cpuint_status = AVR_CPUINT_LVL0EX;  // in a LVL0 ISR
		avr->pc = 0x100;
		avr_raise_interrupt(avr, &v10);
		avr_service_interrupts(avr);
		check("LVL0 does not preempt LVL0 (pc unchanged)", avr->pc, 0x100);

		// E. Static priority: lowest vector number wins by default.
		INT_RESET();
		avr->sreg[S_I] = 1;
		avr->pc = 0x100;
		avr_raise_interrupt(avr, &v20);
		avr_raise_interrupt(avr, &v10);
		avr_service_interrupts(avr);
		check("static priority: lower vector first", avr->pc, 10 * vs);

		// F. Modified static via LVL0PRI: vector LVL0PRI+1 gets highest priority.
		INT_RESET();
		avr->sreg[S_I] = 1;
		avr->interrupts.cpuint_lvl0pri = 19;   // -> vector 20 highest priority
		avr->pc = 0x100;
		avr_raise_interrupt(avr, &v20);
		avr_raise_interrupt(avr, &v10);
		avr_service_interrupts(avr);
		check("LVL0PRI=19 makes vector 20 win", avr->pc, 20 * vs);

		// G. CCP window suppresses interrupt servicing.
		INT_RESET();
		avr->sreg[S_I] = 1;
		avr->arch.ccp_window = 2;
		avr->pc = 0x100;
		avr_raise_interrupt(avr, &v10);
		avr_service_interrupts(avr);
		check("CCP window suppresses interrupts", avr->pc, 0x100);
		avr->arch.ccp_window = 0;
		avr_service_interrupts(avr);
		check("serviced once CCP window closes", avr->pc, 10 * vs);

		#undef INT_RESET
	}

	printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
			failures, failures == 1 ? "" : "s");
	return failures ? 1 : 0;
}
