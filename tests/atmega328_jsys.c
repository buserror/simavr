/*
 * atmega328_jsys.c: a small demonstaration of using an illegal
 * instruction code to raise an IRQ and invoke a service from the
 * surrounding application.  In this case a simple sleep/wake.
 * The technique allows low-overhead simulation of busy-waiting.
 *
 * Derived from atmega48_enabled.c.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega328");

uint8_t count, ext_count;

int main(void)
{
	// Set up timer0 - do not start yet

	TCCR0A |= (1 << WGM01);                   // Configure timer 0 for CTC mode
	OCR0A   = 0xAA;                           // CTC compare value
	
	TCCR0B |= (1 << CS00) | (1 << CS01);      // Start timer: clk/64

	sei();									  // But none are enabled!
	do {
		while ((TIFR0 & (1 << OCF0A)) == 0) {
			++count;
			asm volatile ("\t.word 0xff\n\t"  // JSYS requesting sleep.
						  ".byte 1, 0\n");
		}
		TIFR0 |= (1 << OCF0A);                // Clear it
		++ext_count;
	} while (count < 20);
	cli();

	/* Try an output JSYS.  It will not normally be seen as tests.c suppresses
	 * stdout, but may be enabled there.
	 *
	 * This might be extended to mimic printf(), but with no stdio in the AVR.
	 * The format strings might be placed in the .mmcu section,
	 * further reducing flash use.
	 *
	 * Gcc is extremely sensitive about guessing the size of the ASM output.
	 */

	asm volatile ("\t.word 0xff\n\t"	  // JSYS requesting output.
				  ".byte 2, 'H'\n\t"      // Stupid, but .asciz fails.
				  ".byte 'e', 'l'\n\t"
				  ".byte 'l', 'o'\n\t"
				  ".byte ' ', 'f'\n\t"
				  ".byte 'r', 'o'\n\t"
				  ".byte 'm', ' '\n\t"
				  ".byte 'A', 'V'\n\t"
				  ".byte 'R', '.'\n\t"
				  ".byte '\n', 0\n\t"
				  ".byte 0, 0\n");
	sleep_cpu();
}
