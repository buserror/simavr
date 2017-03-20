/*
	ds1338_virt.h

	Copyright 2014 Doug Szumski <d.s.szumski@gmail.com>

	Based on i2c_eeprom example by:

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

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

/*
 *  A virtual DS1338 real time clock which runs on the TWI bus.
 *
 *  Features:
 *
 *  > External oscillator is synced to the AVR core
 *  > Square wave output with scalable frequency
 *  > Leap year correction until 2100
 *
 *  Should also work for the pin compatible DS1307 device.
 */

#ifndef DS1338_VIRT_H_
#define DS1338_VIRT_H_

#include "sim_irq.h"
#include "sim_avr.h"
#include "avr_ioport.h"

// TWI address is fixed
#define DS1338_VIRT_TWI_ADDR		0xD0

/*
 * Internal registers. Time is in BCD.
 * See p10 of the DS1388 datasheet.
 */
#define DS1338_VIRT_SECONDS		0x00
#define DS1338_VIRT_MINUTES		0x01
#define DS1338_VIRT_HOURS		0x02
#define DS1338_VIRT_DAY			0x03
#define DS1338_VIRT_DATE		0x04
#define DS1338_VIRT_MONTH		0x05
#define DS1338_VIRT_YEAR		0x06
#define DS1338_VIRT_CONTROL		0x07

/*
 * Seconds register flag - oscillator is enabled when
 * this is set to zero. Undefined on startup.
 */
#define DS1338_VIRT_CH			7

/*
 * 12/24 hour select bit. When high clock is in 12 hour
 * mode and the AM/PM bit is operational. When low the
 * AM/PM bit becomes part of the tens counter for the
 * 24 hour clock.
 */
#define DS1338_VIRT_12_24_HR		6

/*
 * AM/PM flag for 12 hour mode. PM is high.
 */
#define DS1338_VIRT_AM_PM		5

/*
 * Control register flags. See p11 of the DS1388 datasheet.
 *
 *  +-----+-----+-----+------------+------+
 *  | OUT | RS1 | RS0 | SQW_OUTPUT | SQWE |
 *  +-----+-----+-----+------------+------+
 *  | X   | 0   | 0   | 1Hz        |    1 |
 *  | X   | 0   | 1   | 4.096kHz   |    1 |
 *  | X   | 1   | 0   | 8.192kHz   |    1 |
 *  | X   | 1   | 1   | 32.768kHz  |    1 |
 *  | 0   | X   | X   | 0          |    0 |
 *  | 1   | X   | X   | 1          |    0 |
 *  +-----+-----+-----+------------+------+
 *
 *  OSF : Oscillator stop flag. Set to 1 when oscillator
 *        is interrupted.
 *
 *  SQWE : Square wave out, set to 1 to enable.
 */
#define DS1338_VIRT_RS0			0
#define DS1338_VIRT_RS1			1
#define DS1338_VIRT_SQWE		4
#define DS1338_VIRT_OSF			5
#define DS1338_VIRT_OUT			7

#define DS1338_CLK_FREQ 32768
#define DS1338_CLK_PERIOD_US (1000000 / DS1338_CLK_FREQ)

// Generic unpack of 8bit BCD register. Don't use on seconds or hours.
#define UNPACK_BCD(x) (((x) & 0x0F) + ((x) >> 4) * 10)

enum {
	DS1338_TWI_IRQ_OUTPUT = 0,
	DS1338_TWI_IRQ_INPUT,
	DS1338_SQW_IRQ_OUT,
	DS1338_IRQ_COUNT
};

/*
 * Square wave out prescaler modes; see p11 of DS1338 datasheet.
 */
enum {
	DS1338_VIRT_PRESCALER_DIV_32768 = 0,
	DS1338_VIRT_PRESCALER_DIV_8,
	DS1338_VIRT_PRESCALER_DIV_4,
	DS1338_VIRT_PRESCALER_OFF,
};

/*
 * Describes the behaviour of the specified BCD register.
 *
 * The tens mask is used to avoid config bits present in
 * the same register.
 */
typedef struct bcd_reg_t {
	uint8_t * reg;
	uint8_t min_val;
	uint8_t max_val;
	uint8_t tens_mask;
} bcd_reg_t;

// TODO: This should be generic, is also used in ssd1306, and maybe elsewhere..
typedef struct ds1338_pin_t
{
	char port;
	uint8_t pin;
} ds1338_pin_t;

/*
 * DS1338 I2C clock
 */
typedef struct ds1338_virt_t {
	struct avr_t * avr;
	avr_irq_t * irq;		// irq list
	uint8_t verbose;
	uint8_t selected;		// selected address
	uint8_t reg_selected;		// register selected for write
	uint8_t reg_addr;		// register pointer
	uint8_t nvram[64];		// battery backed up NVRAM
	uint16_t rtc;			// RTC counter
	uint8_t square_wave;
} ds1338_virt_t;

void
ds1338_virt_init(struct avr_t * avr,
                 ds1338_virt_t * p);

/*
 * Attach the ds1307 to the AVR's TWI master code,
 * pass AVR_IOCTL_TWI_GETIRQ(0) for example as i2c_irq_base
 */
void
ds1338_virt_attach_twi(ds1338_virt_t * p,
                       uint32_t i2c_irq_base);

void
ds1338_virt_attach_square_wave_output(ds1338_virt_t * p,
                                      ds1338_pin_t * wiring);

static inline int
ds1338_get_flag(uint8_t reg, uint8_t bit)
{
	return (reg & (1 << bit)) != 0;
}

#endif /* DS1338_VIRT_H_ */
