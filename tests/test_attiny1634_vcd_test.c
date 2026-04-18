#include "tests.h"
#include "sim_vcd_file.h"

int main(int argc, char **argv) {
	static const char *expected =
		"Read ADC: 657 mvolts\r\n"
		"Read ADC: 2197 mvolts\r\n";
	avr_vcd_t          input;
	avr_t             *avr;

	tests_init(argc, argv);
	avr = tests_init_avr("attiny1634_vcd_test.axf");
	avr_vcd_init_input(avr, "stimulus_attiny1634.in", &input);

	tests_assert_uart_receive_avr(avr, 100000, expected, '0');

	tests_success();
	return 0;
}
