#include "tests.h"
#include "avr_acomp.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);

	static const char *expected =
		"Check analog comparator with polling values\r\n"
		"110110101010000100\r\n"
		"Check analog comparator interrupts\r\n"
		"YYYYYY\r\n"
		"Check analog comparator triggering timer capture\r\n"
		"YY";

	avr_t *avr = tests_init_avr("atmega88_ac_test.axf");

	// set voltages
	avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ACOMP_GETIRQ, ACOMP_IRQ_AIN0), 2000);
	avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ACOMP_GETIRQ, ACOMP_IRQ_AIN1), 1800);
	avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ACOMP_GETIRQ, ACOMP_IRQ_ADC0), 200);
	avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ACOMP_GETIRQ, ACOMP_IRQ_ADC1), 3000);
	avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ACOMP_GETIRQ, ACOMP_IRQ_ADC2), 1500);
	avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ACOMP_GETIRQ, ACOMP_IRQ_ADC3), 1500);
	avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ACOMP_GETIRQ, ACOMP_IRQ_ADC4), 3000);
	avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ACOMP_GETIRQ, ACOMP_IRQ_ADC5), 200);
	avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ACOMP_GETIRQ, ACOMP_IRQ_ADC6), 3000);
	avr_raise_irq(avr_io_getirq(avr, AVR_IOCTL_ACOMP_GETIRQ, ACOMP_IRQ_ADC7), 1500);

	tests_assert_uart_receive_avr(avr, 100000,
				   expected, '0');

	tests_success();
	return 0;
}
