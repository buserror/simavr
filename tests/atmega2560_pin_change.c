/*
	atmega2560_pin_change.c

	Test for pin_change interrupt simulation.
 */

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "avr_mcu_section.h"

/*
 * This demonstrate how to use the avr_mcu_section.h file
 * The macro adds a section to the ELF file with useful
 * information for the simulator
 */
AVR_MCU(F_CPU, "atmega2560");

static int uart_putchar(char c, FILE *stream)
{
	if (c == '\n')
		uart_putchar('\r', stream);
	loop_until_bit_is_set(UCSR3A, UDRE3);
	UDR3 = c;
	return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);

volatile uint8_t interrupts;
volatile char    buffer[32];

ISR(PCINT0_vect)
{
    buffer[interrupts++] = '0';
}

ISR(PCINT1_vect)
{
    buffer[interrupts++] = '1';
}

ISR(PCINT2_vect)
{
    buffer[interrupts++] = '2';
}

int main()
{
    stdout = &mystdout;
    PCICR = (1 <<PCIE0) + (1 << PCIE1) + (1 <<PCIE2); // Enable interrupts.
    PCMSK0 = 0xf0;

    sei();

    /* Test for interrupt caused external (timer) peripheral: no interrupt. */

    PORTA = 0xff;                  // Triggers output to PORTB/4, no output.
    PORTA = 0x0f;                  // Toogle output bit back to zero.
    DDRB = 0xff;                   // Set ports for output.
    buffer[interrupts++] = ' ';

    /* Test for interrupt caused external (timer) peripheral: interrupts. */

    PORTA = 0;                     // Triggers output to PORTB/4
    buffer[interrupts++] = ' ';

    DDRE = 0xff;
    DDRJ = 0xff;
    DDRK = 0xff;
    PORTB = 0x20;                  // Interrupt
    PORTE = 0xff;                  // No interrupt
    PORTJ = 0x20;                  // No interrupt
    PORTK = 0x20;                  // No interrupt
    buffer[interrupts++] = ' ';
    PORTB = 0x22;                  // No interrupt
    PCMSK1 = 0x7f;
    PCMSK2 = 0x7f;
    PORTE = 0xfe;                  // Interrupt
    PORTB = 0x32;                  // Interrupt
    PORTK = 0x00;                  // Interrupt
    PORTJ = 0x01;                  // Interrupt
    cli();
    buffer[interrupts++] = ' ';
//    PORTE = 0xff; // Fails! Double interrupt after sei().
    PORTJ = 0xff;
    sei();                         // Only one interrupt
    PORTK = 0x80;                  // No interrupt;
    PORTB = 0x00;                  // Interrupt
    PORTK = 0xC0;                  // Interrupt;

    PCMSK1 = 0x7e;
    PORTJ = 0x3F;                  // No interrupt;
    PORTK = 0;                     // Interrupt;
    PORTE = 0;                     // No interrupt;
    PORTJ = 0xe0;                  // Interrupt;

    /* Show that normal GPIO input works. */

    DDRB = 0xff;                   // Set ports for output.
    PORTA = 1;                     // Triggers input to PORTB/4

    cli();
    buffer[interrupts] = '\0';
    fputs((const char *)buffer, stdout);
    putchar('\n');

    // this quits the simulator, since interupts are off
    // this is a "feature" that allows running tests cases and exit

    sleep_cpu();
}
