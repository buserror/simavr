#include <stdio.h>
#include <stdlib.h>
#include "tests.h"

int main(int argc, char **argv) {
	static const char *expected = "Wrote 64 bytes to address 4096\r\nCheck Pass";
	avr_t             *avr;

	tests_init(argc, argv);
	avr = tests_init_avr("attiny85_spm_test.axf");

	tests_assert_register_receive_avr(avr, 100000, expected, (avr_io_addr_t)0x2f);
	tests_success();
	return 0;
}
