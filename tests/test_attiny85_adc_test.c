#include <stdio.h>
#include <stdlib.h>
#include "tests.h"
#include "avr_adc.h"

/* Start of the ADC's IRQ list. */

static avr_irq_t *base_irq;

/* Table of voltages to apply to each input in turn. */

static uint32_t volts[] = {
                           1283, // 2.56V Ref. just over half full scale
                           990,  // 1.1 V ref 0.9 FS
                           1200, //       over-voltage
                           0,    //       zero

                           1040, //       ADC0/ADC0 differential
                           386,  //       ADC1, left adjust
                           100,  // 2.56V ADC2/ADC3 differential
                           210,  // 2.56V ADC2/ADC3 differential, signed X20

                           400,  // 1.10V ref, ADC0/ADC1 differential X20
                           2,    // 1.10V ref, ADC1
                           100,  // 2.56V ref, ADC2/ADC3 differential, signed
                           2000, // 2.56V ref, ADC2/3 diff, signed, IPR

                           3000, // 2.56V ref, ADC0/1 diff, signed, overflow
                           2215, // 2.56V ref, ADC0/1 diff, signed
                           700,  // 1.10V ref, ADC2/3 diff, signed -ve overflow
                           2222, // AREF (3V)  ADC3
};

static unsigned int index;

/* Callback for A-D conversion sampling. */

static void conversion(struct avr_irq_t *irq, uint32_t value, void *param)
{
    int i;
    union {
        avr_adc_mux_t request;
        uint32_t      v;
    }   u = { .v = value };

    if (index >= ARRAY_SIZE(volts)) {
        /* This happens when starting with interrupts.
         * Do the sub-tests again after repeating the last one twice,
         * once for the initial conversion and once again for the conversion
         * that has already started at the time of the first interrupt.
         * That agrees with the data sheet.
         */

        if (index == ARRAY_SIZE(volts))
            index++;
        else
            index = 0;
        return;
    }

    i = index & 3;
    if (i != u.request.src &&
        !(u.request.src + 1 == i && u.request.diff == i)) {
        /* Requested input not expected. */

        fail("Simulator requested input %d, but %d expected, index %d\n",
             u.request.src, i, index);
    }

    avr_raise_irq(base_irq + i, volts[index]);
    index++;
}

int main(int argc, char **argv) {
	static const char *expected = "ADC 512 920 1023 0"
                                           " 0 22912 39 585"
                                           " 260 1 1003 759"
                                           " 511 156 512 757\r\n"
                                           " 757 757"
                                           " 512 920 1023 0"
                                           " 0 22912 39 585"
                                           " 260 1 1003 759"
                                           " 511 156 512 757";
	avr_t             *avr;

	tests_init(argc, argv);
        avr = tests_init_avr("attiny85_adc_test.axf");

        /* Request callback when a value is sampled for conversion. */

        base_irq = avr_io_getirq(avr, AVR_IOCTL_ADC_GETIRQ, 0);
        avr_irq_register_notify(base_irq + ADC_IRQ_OUT_TRIGGER,
                                conversion, NULL);

        /* Run program and check results. */

        tests_assert_register_receive_avr(avr, 100000, expected,
                                          (avr_io_addr_t)0x2f /* &USIDR */);
 	tests_success();
 	return 0;
}
