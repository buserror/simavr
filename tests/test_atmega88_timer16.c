#include "tests.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);
	enum tests_finish_reason reason =
		tests_init_and_run_test("atmega88_timer16.axf", 10000000);
	switch(reason) {
	case LJR_CYCLE_TIMER:
		fail("Test failed to finish properly; reason=%d, cycles=%"
		     PRI_avr_cycle_count, reason, tests_cycle_count);
		break;
	case LJR_SPECIAL_DEINIT:
		break;
	default:
		fail("This should not be reached; reason=%d", reason);
	}
	tests_assert_cycles_between(12500000, 12500300);
	tests_success();
	return 0;
}
