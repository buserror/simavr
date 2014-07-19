#include <avr/io.h>
#include <avr/sleep.h>

#include "avr_mcu_section.h"

enum {
	OUTER_CYCLE_COUNTER,
	INNER_CYCLE_COUNTER,
};

AVR_MCU(F_CPU, "atmega88");
AVR_MCU_SIMAVR_COMMAND(&GPIOR0);

const struct avr_mmcu_cycle_counter_t _cycle_counters[] _MMCU_ = {
	{ AVR_MCU_CYCLE_COUNTER(OUTER_CYCLE_COUNTER, "Outer") },
	{ AVR_MCU_CYCLE_COUNTER(INNER_CYCLE_COUNTER, "Inner") },
};

int main(void) {
	asm volatile(
		// PORTB output
		"ldi	r16, 0xff\n\t"
		"out	%[_DDRB], r16\n\t"

		// Load commands & IDs into registers
		"ldi	r16, %[_SIMAVR_CMD_START_CYCLE_COUNTER]\n\t"
		"ldi	r17, %[_OUTER_CYCLE_COUNTER]\n\t"
		"ldi	r18, %[_INNER_CYCLE_COUNTER]\n\t"
		"ldi	r19, %[_SIMAVR_CMD_STOP_CYCLE_COUNTER]\n\t"

		// Start outer cycle counter
		"out	%[_GPIOR0], r16\n\t"
		"out	%[_GPIOR0], r17\n\t"

		// Setup loop counter for 10 iterations
		// (1 cycle)
		"ldi	r20, 10\n\t"

		// Start inner cycle counter
	"0: "	"out	%[_GPIOR0], r16\n\t"
		"out	%[_GPIOR0], r18\n\t"

		// Toggle PORTB
		// (3 cycles per iteration)
		"in	r21, %[_PORTB]\n\t"
		"com	r21\n\t"
		"out	%[_PORTB], r21\n\t"

		// Stop inner cycle counter
		"out	%[_GPIOR0], r19\n\t"

		// Decrement loop counter and branch
		// (9x 3 cycles, 1x 2 cycles)
		"subi	r20, 1\n\t"
		"brne	0b\n\t"

		// Stop outer cycle counter
		"out	%[_GPIOR0], r19\n\t"
		:
		: [_DDRB] "I" (_SFR_IO_ADDR(DDRB)),
		  [_PORTB] "I" (_SFR_IO_ADDR(PORTB)),
		  [_GPIOR0] "I" (_SFR_IO_ADDR(GPIOR0)),
		  [_SIMAVR_CMD_START_CYCLE_COUNTER] "M" (SIMAVR_CMD_START_CYCLE_COUNTER),
		  [_SIMAVR_CMD_STOP_CYCLE_COUNTER] "M" (SIMAVR_CMD_STOP_CYCLE_COUNTER),
		  [_OUTER_CYCLE_COUNTER] "M" (OUTER_CYCLE_COUNTER),
		  [_INNER_CYCLE_COUNTER] "M" (INNER_CYCLE_COUNTER)
		: "r16", "r17", "r18", "r19", "r20", "r21");

	sleep_cpu();
}
