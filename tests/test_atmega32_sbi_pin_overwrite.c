#include "tests.h"
#include "avr_ioport.h"
#include <stdio.h>

static void raise_pind6_irq_with_zero(struct avr_irq_t * irq, uint32_t value, void * param)
{
  avr_t *avr = (avr_t *)param;
  avr_irq_t * pind6=  avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 6);
  avr_raise_irq(pind6, 0);
}

static void raise_pind6_irq_with_one(struct avr_irq_t * irq, uint32_t value, void * param)
{
  avr_t *avr = (avr_t *)param;
  avr_irq_t * pind6=  avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 6);
  avr_raise_irq(pind6, 1);
}


int main(int argc, char **argv) {
	tests_init(argc, argv);

	static const char *expected = "OK1, OK2, OK3, OK4, OK5, OK6, OK7, OK8, OK9, OK10, OK11, ";

	avr_t *avr = tests_init_avr("atmega32_sbi_pin_overwrite.axf");

  avr_irq_t * pina0 =  avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('A'), 0);
	avr_irq_register_notify(pina0, raise_pind6_irq_with_zero, (void *)avr);

  avr_irq_t * pinb0 =  avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 0);
	avr_irq_register_notify(pinb0, raise_pind6_irq_with_one, (void *)avr);
  
	tests_assert_uart_receive_avr(avr,
			       100000,
			       expected,
			       '0');

	tests_success();
	return 0;
}
