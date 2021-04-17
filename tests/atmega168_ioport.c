#ifndef F_CPU
#define F_CPU 8000000
#endif
#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

/*
 * This demonstrate how to use the avr_mcu_section.h file
 * The macro adds a section to the ELF file with useful
 * information for the simulator
 */
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega168");

static int uart_putchar(char c, FILE *stream) {
	if (c == '\n')
		uart_putchar('\r', stream);
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);

ISR(INT0_vect)
{
    printf("I<%02X ", PIND);
}

ISR(PCINT0_vect)
{
    printf("K ");
}

ISR(PCINT2_vect)
{
    printf("J<%02X ", PORTD);
    PORTD = 0;
}

int main()
{
	stdout = &mystdout;

        /* Enable output on Port D pins 0-3 and write to them. */

        DDRD = 0xf;
        PORTD = 0xa;

        printf("P<%02X ", PIND); // Should say P<2A as caller sets bit 5.

        /* Toggle some outputs. */

        PIND = 3;

        /* Change directions. */

        DDRD = 0x3c;

        /* Change output. */

        PORTD = 0xf0;

        /* This should say P<70 - pullups and direct output give 0xF0
         * but the caller sees that and turns off bit 7 input,
         * overriding that pullup.
         */

        printf("P<%02X ", PIND);

        /* Set-up rising edge interrupt on pin 2 (INT 0). */

        EICRA = 3;
        EIMSK = 1;

        /* Turn off pin 4, signal the caller to raise pin 2. */

        PORTD = 0xe0;

        /* Verify the interrupt flag is set. */

        printf("F<%02X ", EIFR);

        sei();

        /* This duplicates the value in the INT0 handler, but it
         * takes sufficient time to be sure that there is only one
         * interrupt.  There was a bug that caused continuous interrupts
         * when this was first tried.
         */

        printf("P<%02X ", PIND);

        /* TODO: Test the level-triggered interupt.  It can be started
         * by a pin-value change or by writing to either of EICRA and EIMSK.
         */

        /* Try pin change interrupt. */

        PCICR = (1 << PCIE2); /* Interrupt enable. */
        PCMSK2 = 0x0a;        /* Pins 1 and 3. */
        DDRD = 3;
        PORTD = 1;            /* No interrupt. */
        PORTD = 3;            /* Interrupt. */

        /* Allow time for second interrupt. */

        printf("P<%02X ", PIND);

        // Test "write 1 to clear" on PORT B.

        DDRB = 0xff;
        PCICR = (1 << PCIE0); /* Interrupt enable. */
        PCMSK0 = 3;           /* Pins 0 and 1. */
        cli();
        PORTB = 1;
        PCIFR = 1;            /* Clear interrupt. */
        sei();
        printf("| ");
        cli();
        PORTB = 3;
        PCIFR = 6;
        sei();                /* Interrupt. */
        printf("| ");

	// this quits the simulator, since interupts are off
	// this is a "feature" that allows running tests cases and exit
        cli();
	sleep_cpu();
}
