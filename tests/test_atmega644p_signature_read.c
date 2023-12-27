#include "tests.h"

int main(int argc, char **argv) {
    static const char *expected = "Signature is 0x1e 96 09\r\n";
	avr_t             *avr;

	tests_init(argc, argv);
	avr = tests_init_avr("atmega644p_signature_read.axf");
	tests_assert_uart_receive_avr(avr, 10000000, expected, '0');
	tests_success();
	return 0;
}
