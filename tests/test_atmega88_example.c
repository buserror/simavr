#include "tests.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);

	static const char *expected =
		"Read from eeprom 0xdeadbeef -- should be 0xdeadbeef\r\n"
		"Read from eeprom 0xcafef00d -- should be 0xcafef00d\r\n";
	tests_assert_uart_receive("atmega88_example.axf", 100000,
				  expected, '0');	

	tests_success();
	return 0;
}
