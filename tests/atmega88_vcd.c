/*
	atmega88_example.c

 */

#ifndef F_CPU
#define F_CPU 8000000
#endif
#include <avr/io.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>
#include <util/delay.h>

/*
 * This demonstrate how to use the avr_mcu_section.h file
 * The macro adds a section to the ELF file with useful
 * information for the simulator
 */
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega88");


int main()
{
	DDRB = 1;
	PORTB = 0;


	// test VCD output for 100us
	for(int i=0; i<10; i++)
	{
		PORTB = 1 ^ PORTB;
		_delay_us(100);
	}


	// this quits the simulator, since interupts are off
	// this is a "feature" that allows running tests cases and exit
	sleep_cpu();
}
