/*
	attiny85_adc_test.c

	Copyright 2021 Giles Atkinson

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <avr/cpufunc.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");
AVR_MCU_VOLTAGES(5000, 5000, 3000) // VCC, AVCC, VREF - millivolts.

/* No UART in tiny85, so simply write to unimplemented register ISIDR. */

static int uart_putchar(char c, FILE *stream) {
  if (c == '\n')
    uart_putchar('\r', stream);
  USIDR = c;
  return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);

/* Table of values for ADMUX and ADSRB. */

static struct params {
    uint8_t mux, srb;
} params[] = {
    {0x90, 0 },    // 2.56V ref, input ADC0
    {0x81, 0 },    // 1.10V ref, input ADC1
    {0x82, 0 },    // 1.10V ref, input ADC2, overflow
    {0x83, 0 },    // 1.10V ref, input ADC3, zero

    {0x88, 0},     // 1.10V ref, ADC0/ADC0 differential
    {0xa1, 0},     // 1.10V ref, ADC1, left adjusted
    {0x96, 0},     // 2.56V ref, ADC2/ADC3 differential. signed
    {0x97, 0x80},  // 2.56V ref, ADC2/ADC3 differential, signed, X20

    {0x8B, 0},     // 1.10V ref, ADC0/ADC1 differential X20
    {0x81, 0xa0},  // 1.10V ref, input ADC1, BIN and IPR on.
    {0x96, 0x80},  // 2.56V ref, ADC2/ADC3 differential, signed
    {0x96, 0x20},  // 2.56V ref, ADC2/ADC3 differential, IPR

    {0x9a, 0x80},  // 2.56V ref, ADC0/ADC1 differential, signed +ve overflow
    {0x9a, 0x80},  // 2.56V ref, ADC0/ADC1 differential, signed, positive
    {0x86, 0x80},  // 1.10V ref, ADC2/ADC3 differential, signed, -ve overflow
    {0x43, 0 },    // 3.00 V external ref, input ADC3
};

#define NUM_SUBTESTS (sizeof params / sizeof params[0])

static int index_i, index_o;
static uint16_t int_results[NUM_SUBTESTS + 2];

ISR(ADC_vect)
{
    if (index_i >= NUM_SUBTESTS) {
        /* Done: disable auto-trigger. */

        ADCSRA &= ~(1 << ADATE);
        return;
    }

    /* Write the next ADCMUX/ADSRB settings. */

    ADMUX = params[index_i].mux;
    ADCSRB = params[index_i].srb;
    index_i++;
}

int main(void)
{
    int i;

    stdout = &mystdout;
    printf("ADC");

    /* Turn on the ADC. */

    ADCSRA = (1 << ADEN) + 5;      // Enable, clock scale = 32

    /* Do conversions. */

    for (i = 0; i < NUM_SUBTESTS; ++i) {
        ADMUX = params[i].mux;
        ADCSRB = params[i].srb;
        ADCSRA = (1 << ADEN) + (1 << ADSC) + (1 << ADIF) + 5;
        while ((ADCSRA & (1 << ADIF)) == 0)
            ;
        printf(" %d", ADC);
    }
    uart_putchar('\n', stdout);
    ADCSRA = (1 << ADEN) + (1 << ADIF); // Clear interrupt flag.

    /* Do it again with interrupts. printf() is too slow to send the
     * results in real time, even with maximum pre-scaler ratio.
     */

    sei();
    ADCSRA = (1 << ADEN) + (1 << ADSC) + (1 << ADATE) + (1 << ADIE) + 4;

    while (index_o < NUM_SUBTESTS + 2) {
        sleep_cpu();
        int_results[index_o++] = ADC;
    }
    for (i = 0; i < NUM_SUBTESTS + 2; ++i)
        printf(" %d", int_results[i]);
    cli();
    sleep_cpu();
}
