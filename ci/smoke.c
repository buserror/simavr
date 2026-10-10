/*
 * The firmware the release workflow runs simavr on, as a smoke test: it proves
 * that the binary loads an ELF file (which is what libelf is for) and that the
 * core executes instructions. --list-cores touches neither.
 *
 * It ends by itself. Returning from main leaves avr-libc's exit handler, which
 * is `cli` followed by `rjmp .-2`, and simavr recognises that as "rjmp .-2 with
 * interrupts off" and quits gracefully with status 0 (see gracefully_quit() in
 * simavr/sim/sim_avr.c). So the test needs no timeout to come to an end, and the
 * exit status is the whole assertion.
 *
 * Compiled with -g: a gdb session against simavr should be able to show a
 * source line and read a variable, and without debug info it cannot.
 *
 * Deliberately without simavr's own headers: the firmware should not have to be
 * rebuilt when those change.
 *
 *   avr-gcc -mmcu=atmega328p -Og -g -o smoke-atmega328p.elf smoke.c
 */
#include <avr/io.h>
#include <stdint.h>

int main(void)
{
	volatile uint16_t sum = 0;

	/* a little arithmetic, a little SRAM, a little I/O -- enough that the
	 * core has to do something other than jump */
	for (uint8_t i = 1; i <= 10; i++)
		sum += i;

	DDRB |= _BV(PB5);
	if (sum == 55)
		PORTB |= _BV(PB5);
	else
		PORTB &= (uint8_t)~_BV(PB5);

	return 0;
}
