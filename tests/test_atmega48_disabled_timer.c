#include "tests.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);
	switch(tests_init_and_run_test("atmega48_disabled_timer.axf", 100000000)) {
	case LJR_CYCLE_TIMER:
		// the cycle timer fired
		break;
	case LJR_SPECIAL_DEINIT:
		// sleep with interrupts off or some other such reason
		fail("AVR woke up from sleep while it shouldn't have (after %"
		     PRI_avr_cycle_count " cycles)", tests_cycle_count);
	default:
		fail("Error in test case: Should never reach this.");
	}
	tests_success();
	return 0;
}
