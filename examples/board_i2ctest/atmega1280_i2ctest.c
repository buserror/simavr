/*
	atmega48_i2ctest.c

	Copyright 2008-2011 Michel Pollet <buserror@gmail.com>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

// for linker, emulator, and programmer's sake
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega1280");

#include "../shared/avr_twi_master.h"
#include "../shared/twimaster.h"

#include <stdio.h>

#define EEPROM_ADDR	0xA0

static int uart_putchar(char c, FILE *stream) {
  if (c == '\n')
    uart_putchar('\r', stream);
  loop_until_bit_is_set(UCSR0A, UDRE0);
  UDR0 = c;
  return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);

static void
test_twi_with_atmel_driver(void)
{
	TWI_Master_Initialise();

	{	// write 2 bytes at some random address
		uint8_t msg[8] = {
				EEPROM_ADDR, // TWI address,
				0xaa, 0x01, // eeprom address, in little endian
				0xde, 0xad,	// data bytes
		};
		TWI_Start_Transceiver_With_Data(msg, 5, 1);

		while (TWI_Transceiver_Busy())
			sleep_mode();
	}
	{
		uint8_t msg[8] = {
				EEPROM_ADDR, // TWI address,
				0xa8, 0x01, // eeprom address, in little endian
		};
		TWI_Start_Transceiver_With_Data(msg, 3, 0); // dont send stop!

		while (TWI_Transceiver_Busy())
			sleep_mode();
	}
	{
		uint8_t msg[9] = {
				EEPROM_ADDR + 1, // TWI address,
		};
		TWI_Start_Transceiver_With_Data(msg, 9, 1); // write 1 byte, read 8, send stop

		while (TWI_Transceiver_Busy())
			sleep_mode();
	}
}

/*
 * Tests the TWI using a commonly used and non-interrupt driven
 * alternative to the Atmel driver:
 *
 * "I2C master library using hardware TWI interface"
 * Author:   Peter Fleury <pfleury@gmx.ch>  http://jump.to/fleury
 */
static void
test_twi_with_pf_driver(void)
{
	/*
	 * This init followed by a start condition is enough to overwrite all TWI
	 * related bits set by TWI_Master_Initialise () in the Atmel driver.
	 */
	i2c_init();

	i2c_start(EEPROM_ADDR + I2C_WRITE);
	// eeprom address, in little endian
	i2c_write(0xaa);
	i2c_write(0x01);
	// data bytes
	i2c_write(0xd0);
	i2c_write(0x0d);
	i2c_stop();

	i2c_start(EEPROM_ADDR + I2C_WRITE);
	// set address
	i2c_write(0xa8);
	i2c_write(0x01);
	// Don't stop

	// Read back data
	i2c_start (EEPROM_ADDR + I2C_READ);
	for (uint8_t i = 0; i < 8; ++i) {
		i2c_readNak();
	};
	i2c_stop();
}

int main()
{
	stdout = &mystdout;

	sei();

	test_twi_with_atmel_driver();

	/*
	 * This should produce *exactly* the same output as above, except
	 * the data written should be D00D as opposed to DEAD.
	 */
	test_twi_with_pf_driver ();

	cli();
	sleep_mode();
}
