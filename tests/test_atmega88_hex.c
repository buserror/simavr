
#include <string.h>

#include "tests.h"

/* Modified version of test_atmega88_example.c that uses a .hex file. */

int main(int argc, char **argv) {
	static const char *expected =
		"Read from eeprom 0xdeadbeef -- should be 0xdeadbeef\r\n"
		"Read from eeprom 0xcafef00d -- should be 0xcafef00d\r\n";
    avr_t *avr;

	tests_init(argc, argv);
	avr = make_avr_from_file("atmega88_example.hex");
	avr->frequency = 8 * 1000 * 1000;
	tests_assert_uart_receive_avr(avr, 100000, expected, '0');

	tests_success();
	return 0;
}
