#include "tests.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);
	switch(tests_init_and_run_test("atmega48_enabled_timer.axf", 100000000)) {
	case LJR_CYCLE_TIMER:
		fail("AVR did not wake up to the enabled timer.");
	case LJR_SPECIAL_DEINIT:
		break;
	default:
		fail("Error in test case: Should never reach this.");
	}
	tests_success();
	return 0;
}
