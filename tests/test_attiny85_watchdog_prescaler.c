#include "tests.h"

/* Within 100 ms: the 15 ms timeout, not the 8 s one set before it. */
int main(int argc, char **argv) {
	tests_init(argc, argv);
	tests_assert_register_receive("attiny85_watchdog_prescaler.axf", 100000,
				      "8 s, then 15 ms: reset", (avr_io_addr_t)0x2f);
	tests_success();
	return 0;
}
