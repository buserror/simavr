#include "tests.h"
#include "avr_spi.h"
#include "avr_uart.h"
#include "avr_ioport.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);

	avr_t* avr = tests_init_avr("atmega2560_spi_slave_echo.axf");
	avr->log = LOG_DEBUG;

	avr_irq_t * srcUART = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('3'), UART_IRQ_OUTPUT);
	avr_irq_t * dstSPI = avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT);
	avr_connect_irq(srcUART, dstSPI);

    avr_irq_t * srcSPI = avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT);
    avr_irq_t * dstUART = avr_io_getirq(avr, AVR_IOCTL_UART_GETIRQ('3'), UART_IRQ_INPUT);
    avr_connect_irq(srcSPI, dstUART);

    avr_irq_t * srcSS = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 4);
    avr_irq_t * dstSS = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 0);
	avr_connect_irq(srcSS, dstSS);

	static const char *expected = "0Hey there, this should be received back\r\n\r";

	tests_assert_spi_master_receive_avr(avr, 50000000, expected, 0);

	tests_success();
	return 0;
}
