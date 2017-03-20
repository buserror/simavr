/*
 ds1338.c

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

#include <stdint.h>

#include "ds1338.h"
#include "i2cmaster.h"

static void
ds1338_write(const uint8_t reg, const uint8_t data)
{
	i2c_start(DS1338_TWI_ADR + I2C_WRITE);
	i2c_write(reg);
	i2c_write(data);
	i2c_stop();
}

static uint8_t
ds1338_read(const uint8_t reg)
{
	i2c_start(DS1338_TWI_ADR + I2C_WRITE);
	i2c_write(reg);
	i2c_rep_start(DS1338_TWI_ADR + I2C_READ);
	uint8_t data = i2c_readNak();
	i2c_stop();

	return data;
}

void
ds1338_init(void)
{
	ds1338_write(DS1338_CONTROL, DS1338_CONTROL_SETTING);
	// Set the CS bit to zero to enable clock
	uint8_t data = ds1338_read(DS1338_SECONDS);
	data &= ~(1 << DS1338_CH);
	ds1338_write(DS1338_SECONDS, data);
}

void
ds1338_get_time(ds1338_time_t * const time)
{
	uint8_t data = ds1338_read(DS1338_SECONDS);
	time->seconds = 10 * ((data & 0b01110000) >> 4) + (data & 0x0F);

	data = ds1338_read(DS1338_MINUTES);
	time->minutes = UNPACK_BCD(data);

	data = ds1338_read(DS1338_HOURS);
	if (data & (1 << DS1338_12_24_HR))
	{
		// 12 hour mode
		time->hours = 10 * ((data & 0b00010000) >> 4) + (data & 0x0F);
		time->is_pm = data & (1 << DS1338_AM_PM);
	} else {
		// 24 hour mode
		time->hours = 10 * ((data & 0b00110000) >> 4) + (data & 0x0F);
	}

	data = ds1338_read(DS1338_DAY);
	// Shift day so that it's 0-6, not 1-7
	time->day = data - 1;

	data = ds1338_read(DS1338_DATE);
	time->date = UNPACK_BCD(data);

	data = ds1338_read(DS1338_MONTH);
	time->month = UNPACK_BCD(data);

	data = ds1338_read(DS1338_YEAR);
	time->year = UNPACK_BCD(data);
}

void
ds1338_set_time(const ds1338_time_t * const time)
{
	// Should always write zero to the CS bit to enable the clock
	ds1338_write(DS1338_SECONDS, TO_BCD(time->seconds));
	ds1338_write(DS1338_MINUTES, TO_BCD(time->minutes));

	uint8_t hour_reg = ds1338_read(DS1338_HOURS);
	// Wipe everything apart from the 12/24 hour bit
	hour_reg &= ~(0b00111111);
	if ((hour_reg & (1 << DS1338_12_24_HR)) && (time->is_pm)) {
		// 12 hour mode and it's PM
		hour_reg |= (1 << DS1338_AM_PM);
	}
	hour_reg |= TO_BCD(time->hours);
	ds1338_write(DS1338_HOURS, hour_reg);

	// Shift back day from 0-6 to 1-7.
	ds1338_write(DS1338_DAY, time->day + 1);
	ds1338_write(DS1338_DATE, TO_BCD(time->date));
	ds1338_write(DS1338_MONTH, TO_BCD(time->month));
	ds1338_write(DS1338_YEAR, TO_BCD(time->year));
}
