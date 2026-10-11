#include "tests.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);
	tests_assert_register_receive("attiny85_watchdog_wdif.axf", 100000,
				      "set, cleared, set again later", (avr_io_addr_t)0x2f);
	tests_success();
	return 0;
}
