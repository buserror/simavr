/*
 * attiny85_spi.c
 *
 * Test USI as SPI slave.
 */

#include <avr/sleep.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny84");

static __flash const char msg[] = "Hello to u 2313";

int main()
{
	const char *cp = msg;

	USICR = _BV(USIWM0) | _BV(USICS1);  // Counts both edges.
	DDRA |= _BV(PA5);				   	// Enable DO
	for (;;) {
		uint8_t c;

		USIDR = c = pgm_read_byte(cp++);
		USISR = _BV(USIOIF);
		while ((USISR & _BV(USIOIF)) == 0)
			;
		TCNT0 = USIBR;	// Copy received data to control program. */
		if (!c) {
			/* Sleeping with interrupts off stops simulation. */

			sleep_cpu();
			break;
		}
	}
}
