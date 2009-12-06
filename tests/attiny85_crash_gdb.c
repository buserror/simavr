/*
 * attiny85_crash_gdb.c
 *
 *  Created on: 1 Dec 2009
 *      Author: jone
 */

#include <avr/sleep.h>
#include <avr/io.h>
#include "avr_mcu_section.h"

AVR_MCU(F_CPU, "attiny85");


int main()
{

	/*
	 * this is not much, but that crashed the core, and should activate
	 * the gdb server properly, so you can see it stopped, here
	 */

	*((uint8_t*)0xdead) = 0x55;

	// should never reach here !

	sleep_mode();
}
