#include "tests.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);

	static const char *expected =
		"Watchdog is active\r\n"
		"Waiting for Watchdog to kick\r\n"
		"Watchdog kicked us!\r\n";

	tests_assert_uart_receive("atmega48_watchdog_test.axf", 1000000,
				  expected, '0');
	tests_success();
	return 0;
}
