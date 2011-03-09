#include "tests.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);

	static const char *expected =
		"Read 8 ADC channels to test interrupts\r\n"
		"All done. Now reading the 1.1V value in pooling mode\r\n"
		"Read ADC value 0155 = 1098 mvolts -- ought to be 1098\r\n";
	tests_assert_uart_receive("atmega644_adc_test.axf", 100000,
				  expected, '0');

	tests_success();
	return 0;
}
