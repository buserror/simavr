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

/*
 * This small section tells simavr to generate a VCD trace dump.
 * Opening it with gtkwave will show you the data being pumped out into
 * the data register UDR0, and the UDRE0 ("empty") bit being set, then cleared
 *
 * The output bytes are logged as the value of an IRQ, because the special
 * nature of UDRE0 (separate read and write buffers) means that logging
 * the register UDRE0 would return any data received by the UART.
 */

AVR_MCU_VCD_REGISTER_BIT(UCSR0A, UDRE0);
AVR_MCU_VCD_IO_IRQ(uar0, 1 /* UART_IRQ_OUTPUT */, "UART_output");

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
