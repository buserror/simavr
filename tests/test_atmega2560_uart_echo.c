#include "tests.h"
#include "avr_uart.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);

	avr_t* avr = tests_init_avr("atmega2560_uart_echo.axf");
	avr->log = LOG_TRACE;

	avr_irq_t * src = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('3'), UART_IRQ_OUTPUT);
	avr_irq_t * dst = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('3'), UART_IRQ_INPUT);
	avr_connect_irq(src, dst);

	static const char *expected =
		"Hey there, this should be received back\r\n"
		"Received: Hey there, this should be received back\r\r\n";

	tests_assert_uart_receive_avr(avr, 10000000, expected, '3');

	tests_success();
	return 0;
}
