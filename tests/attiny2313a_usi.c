/*
 * attiny2313a.c
 *
 * Test USI as SPI master.
 */

#include <util/delay.h>
#include <avr/cpufunc.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>

#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny2313a");

static PROGMEM const char msg[] = "Hello there, 84";

int main()
{
	const char *cp = msg;

	_delay_us(100.0);	// Let the 84 get ready.

	/*  Set up SPI master. */

	USICR = _BV(USIWM0) | _BV(USICS1);
	DDRB = _BV(PB6) | _BV(PB7);		// Enable DO and SCLK output.
	for (;;) {
		uint8_t c;

		USIDR = c = pgm_read_byte(cp++);
		USISR |= _BV(USIOIF);
		while ((USISR & _BV(USIOIF)) == 0)
			USICR |= _BV(USITC);
		TCNT0 = USIDR;	// Copy received data to control program. */
		if (!c) {
			/* Sleeping with interrupts off stops simulation. */

			sleep_cpu();
			break;
		}

		/* Pause while receiver reloads. */

		_NOP();
		_NOP();
		_NOP();
		_NOP();
	}
}
