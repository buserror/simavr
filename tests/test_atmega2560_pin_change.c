#include "tests.h"
#include "sim_avr.h"
#include "avr_ioport.h"

static avr_irq_t *twiddle_irq;

/* Called on write to port A, twiddle PORTB/4 as peripheral might. */

static void reg_write(struct avr_irq_t *irq, uint32_t value, void *param)
{
    static int count;
    uint32_t   flag;

    /* Set the output flag on the first two calls.
     *
     * BUG: 3 should be 4 here, but it still works.
     */

    flag = (++count < 3) ? AVR_IOPORT_OUTPUT : 0;
    avr_raise_irq(twiddle_irq, (twiddle_irq->value ^ 1) | flag);
}

int main(int argc, char **argv) {
    static const char *expected = " 0 0 1021 102210\r\n";
    avr_t             *avr;

    tests_init(argc, argv);
    avr = tests_init_avr("atmega2560_pin_change.axf");

    twiddle_irq = avr_io_getirq(avr,
                                AVR_IOCTL_IOPORT_GETIRQ('B'),
                                IOPORT_IRQ_PIN4);
    avr_irq_register_notify(avr_io_getirq(avr,
                                          AVR_IOCTL_IOPORT_GETIRQ('A'),
                                          IOPORT_IRQ_REG_PORT),
                            reg_write, avr);

    tests_assert_uart_receive_avr(avr, 10000000, expected, '3');
    tests_success();
    return 0;
}
