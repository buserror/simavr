#undef F_CPU
#define F_CPU 16000000

#include <avr/io.h>

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega48");

#include "avr_hd44780.c"

int main()
{
	hd44780_init();
	/*
	 * Clear the display.
	 */
	hd44780_outcmd(HD44780_CLR);
	hd44780_wait_ready(1); // long wait

	/*
	 * Entry mode: auto-increment address counter, no display shift in
	 * effect.
	 */
	hd44780_outcmd(HD44780_ENTMODE(1, 0));
	hd44780_wait_ready(0);

	/*
	 * Enable display, activate non-blinking cursor.
	 */
	hd44780_outcmd(HD44780_DISPCTL(1, 1, 0));
	hd44780_wait_ready(0);

	uint16_t count = 0;
	while (1)
	{
		uint16_t temp = count;
		for (uint8_t i = 5; i > 0; i--)
		{
			hd44780_outcmd(HD44780_DDADDR(i - 1));
			hd44780_wait_ready(0);
			hd44780_outdata(temp % 10 + 48);
			temp /= 10;
			hd44780_wait_ready(0);
		}
		count++;
	}
}