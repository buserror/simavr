#include <stdio.h>
#include <string.h>
#include "tests.h"
#include "avr_ioport.h"

/* Start of the IOPORT's IRQ list. */

static avr_irq_t *base_irq;

/* Accumulate log of events for comparison at the end. */

static char  log[256];
static char *fill = log;

#define LOG(...) \
    (fill += snprintf(fill, (log + sizeof log) - fill, __VA_ARGS__))

/* IRQ call-back function for changes in pin levels. */

static void monitor_5(struct avr_irq_t *irq, uint32_t value, void *param)
{
    LOG("5-%02X ", value);
}

/* This monitors the simulator's idea of the I/O pin states.
 * Changes this program makes to inputs are not reported,
 * presumably because the simulator "knows" we made them.
 */

static void monitor(struct avr_irq_t *irq, uint32_t value, void *param)
{
    LOG("P-%02X ", value);

    if (value == 9) {
        /* Assume this is because bit 0 was left high when its
         * direction switched to input.  Make it low.
         */

        avr_raise_irq(base_irq + IOPORT_IRQ_PIN0, 0);
    } if (value == 0xf0) {
        /* Assume this is a combination of 0x30 (direct) and 0xc0 (pullups).
         * So change inputs.
         */

        avr_raise_irq(base_irq + IOPORT_IRQ_PIN4, 0); // Ignored.
        avr_raise_irq(base_irq + IOPORT_IRQ_PIN7, 0);
    }

}

/* Writes to output ports and DDR are reported here. */

static void reg_write(struct avr_irq_t *irq, uint32_t value, void *param)
{
    static int zero_count;
    char       c;

    if (irq->irq == IOPORT_IRQ_REG_PORT)
        c = 'o';
    else if (irq->irq == IOPORT_IRQ_DIRECTION_ALL)
        c = 'd';
    else
        c = '?';
    LOG("%c-%02X ", c, value);

    if (irq->irq == IOPORT_IRQ_REG_PORT) {
        if (value == 0xe0) {
            /* Program request to raise bit 2: external interrupt. */

            avr_raise_irq(base_irq + IOPORT_IRQ_PIN2, 1);
        } else if (value == 0) {
            if (zero_count++ == 0) {
                /* Raise bit 3: pin change interrupt. */
            
                avr_raise_irq(base_irq + IOPORT_IRQ_PIN3, 1);
            }
        }
    }
}

/* Called when the AVR reads the input port. */

static void reg_read(struct avr_irq_t *irq, uint32_t value, void *param)
{
    LOG("I-%02X ", value);

    /* Change the value read. */

    avr_raise_irq(base_irq + IOPORT_IRQ_PIN5, 1);
}

/* This string should be sent by the firmware. */

static const char *expected = "P<2A P<70 F<01 I<E0 P<E0 J<03 J<00 P<E8 ";

/* This string is expected in variable log. */

static const char *log_expected =
    "d-0F P-00 o-0A P-0A I-0A 5-01 o-09 P-29 d-3C 5-00 P-09 o-F0 5-01 P-F0 "
    "I-70 "                                     // Interrupts off testing.
    "o-E0 P-E0 I-E0 "                           // External interrupt test.
    "d-03 o-01 P-E1 o-03 P-E3 o-00 P-E8 I-E8 "; // Pin change interrupt test.


int main(int argc, char **argv) {
	avr_t             *avr;

	tests_init(argc, argv);
        avr = tests_init_avr("atmega168_ioport.axf");
        base_irq = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 0);

        avr_irq_register_notify(base_irq + IOPORT_IRQ_PIN5,
                                monitor_5, NULL);
        avr_irq_register_notify(base_irq + IOPORT_IRQ_PIN_ALL,
                                monitor, NULL);
        avr_irq_register_notify(base_irq + IOPORT_IRQ_DIRECTION_ALL,
                                reg_write, NULL);
        avr_irq_register_notify(base_irq + IOPORT_IRQ_REG_PORT,
                                reg_write, NULL);
        avr_irq_register_notify(base_irq + IOPORT_IRQ_REG_PIN,
                                reg_read, NULL);

        tests_assert_uart_receive_avr(avr, 100000, expected, '0');

        if (strcmp(log, log_expected))
            fail("Internal log: %s.\nExpected: %s.\n", log, log_expected);
	tests_success();
	return 0;
}
