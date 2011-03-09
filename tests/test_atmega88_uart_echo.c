#include "tests.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);

	static const char *expected =
		"Hey there, this should be received back\n"
		"Received: Hey there, this should be received back\n";
	tests_assert_uart_receive("atmega88_uart_echo.axf", 100000,
				  expected, '0');

	tests_success();
	return 0;
}
