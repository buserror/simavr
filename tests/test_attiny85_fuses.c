#include "tests.h"

/* The datasheet's defaults: low 0x62, high 0xDF, extended 0xFF; no locks. */
int main(int argc, char **argv) {
	tests_init(argc, argv);
	tests_assert_register_receive("attiny85_fuses.axf", 100000,
				      "62 DF FF FF ", (avr_io_addr_t)0x2f);
	tests_success();
	return 0;
}
