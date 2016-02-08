/*
 ds1338.h

 DS1338 / DS1307 I2C clock module driver

 Copyright 2012, 2014 Doug Szumski <d.s.szumski@gmail.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

/*
 * A simple driver to interface with the DS1338. Should work with
 * the pin compatible DS1307 too.
 *
 * TODO:
 *
 * > Read/write registers sequentially to/from an array.
 * > Add a function to check the 'oscillator had a problem flag'
 * > Add a function to configure the square wave output
 * > Add a function to allow writing to additional NVRAM
 * > Use bit fields to save RAM when storing time
 */

#ifndef DS1338_H
#define DS1338_H

/*
 * Internal registers. Time is in BCD.
 * See p10 of the DS1388 datasheet.
 */
#define DS1338_TWI_ADR		0xD0
#define DS1338_SECONDS		0x00
#define DS1338_MINUTES		0x01
#define DS1338_HOURS		0x02
#define DS1338_DAY		0x03
#define DS1338_DATE		0x04
#define DS1338_MONTH		0x05
#define DS1338_YEAR		0x06
#define DS1338_CONTROL		0x07

/*
 * Seconds register flag - oscillator is enabled when
 * this is set to zero. Undefined on startup.
 */
#define DS1338_CH		7

/*
 * 12/24 hour select bit. When high clock is in 12 hour
 * mode and the AM/PM bit is operational. When low the
 * AM/PM bit becomes part of the tens counter for the
 * 24 hour clock.
 */
#define DS1338_12_24_HR		6

/*
 * AM/PM flag for 12 hour mode. PM is high.
 */
#define DS1338_AM_PM		5

// Control register settings: 1Hz square wave out
#define DS1338_CONTROL_SETTING 0b10010000
// 4kHz
//#define DS1338_CONTROL_SETTING 0b10010001
// 8 kHz
//#define DS1338_CONTROL_SETTING 0b10010010
// 32kHz
//#define DS1338_CONTROL_SETTING 0b10010011

// Generic BCD conversion. Don't use on seconds or hours.
#define UNPACK_BCD(x) (((x) & 0x0F) + ((x) >> 4) * 10)
#define TO_BCD(x) ((((x) / 10) << 4) + (x) % 10)

typedef struct ds1338_time_t
{
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t is_pm;
	uint8_t day;		// Runs from 0-6
	uint8_t date;
	uint8_t month;
	uint8_t year;
} ds1338_time_t;

void
ds1338_init (void);

void
ds1338_get_time (ds1338_time_t * const time);

void
ds1338_set_time (const ds1338_time_t * const time);

#endif //DS1338_H
