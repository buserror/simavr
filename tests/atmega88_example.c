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

/*
 * This demonstrate how to use the avr_mcu_section.h file
 * The macro adds a section to the ELF file with useful
 * information for the simulator
 */
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega88");

/* declare this in a .eeprom ELF section */
uint32_t value EEMEM = 0xdeadbeef;

static int uart_putchar(char c, FILE *stream) {
  if (c == '\n')
    uart_putchar('\r', stream);
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);


int main()
{
	stdout = &mystdout;

	// read the eeprom value
	uint32_t c = eeprom_read_dword((void*)&value);
	printf("Read from eeprom 0x%08lx -- should be 0xdeadbeef\n", c);
	// change the eeprom
	eeprom_write_dword((void*)&value, 0xcafef00d);
	// re-read it
	c = eeprom_read_dword((void*)&value);
	printf("Read from eeprom 0x%08lx -- should be 0xcafef00d\n", c);

	// this quits the simulator, since interupts are off
	// this is a "feature" that allows running tests cases and exit
	sleep_cpu();
}
