#include "tests.h"

int main(int argc, char **argv) {
	static const char *expected =
		"Read 8 ADC channels to test interrupts\r\n"
		"All done. Now reading the 1.1V value in polling mode\r\n"
		"Read ADC value 0155 = 1098 mvolts -- ought to be 1098\r\n"
		"Read ADC value 0x1ff -- ought to be 0x1ff\r\n";
	avr_t             *avr;

	tests_init(argc, argv);
	avr = tests_init_avr("atmega644_adc_test.axf");

	// Set AREF to 2200mV, overriding ELF pre-set.

	avr_raise_irq(avr_get_core_irq(avr, AVR_CORE_IRQ_AREF), 2200);

	tests_assert_uart_receive_avr(avr, 10000000, expected, '0');
	tests_success();
	return 0;
}
