/*
	sc18is600.c

	Copyright Yann GOUY <yann_gouy@yahoo.fr>

	partially taken by examples/board_i2c_eeprom
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


//
// quite basic simulation of the SC18IS600 SPI component
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sim_avr.h"
#include "avr_spi.h"


//--------------------------------------------------------------------
// private defines
//


//--------------------------------------------------------------------
// private structure definitions
//

typedef struct {
	struct avr_t * avr;		// AVR access for time
	avr_irq_t *	irq;		// irq list

    struct {
        uint8_t IOconfig;   	// internal register map
        uint8_t IOstate;
        uint8_t I2CClock;
        uint8_t I2CTO;
        uint8_t I2CStat;
        uint8_t I2CAddr;
    } regs;
} sc18is600_t;


//--------------------------------------------------------------------
// private constants
//


//--------------------------------------------------------------------
// private variables
//

static sc18is600_t * sc18;


//--------------------------------------------------------------------
// private functions
//



// called on every I2C transaction
static void sc18_in_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
    static uint8_t ret_value = 0;
	sc18is600_t * sc18 = (sc18is600_t*)param;

    // ackwonledge it
    printf("sc18is600[%3d]: rx 0x%02x", ret_value, value);
    ret_value++;
    avr_raise_irq(sc18->irq + SPI_IRQ_INPUT, ret_value);
    printf(" tx 0x%02x\n", ret_value);

    if ( ret_value == 0xff ) {
        exit(1);
    }
#if 0
	avr_twi_msg_irq_t v;

	v.u.v = value;

	// if the slave address is not self address
	if ( ((mpu->self_addr << 1) && 0xfe) != (v.u.twi.addr && 0xfe) ) {
		printf("MPU 6050 @ 0x%02x: bad address 0x%02x\n",  mpu->self_addr, v.u.twi.addr);
		// ignore it
		return;
	}

	// STOP received
	if ( v.u.twi.msg & TWI_COND_STOP ) {
		// it was us !
		printf("]\n");
		return;
	}

	// START received
	if ( v.u.twi.msg & TWI_COND_START ) {
		// ackwonledge it
		avr_raise_irq(mpu->irq + TWI_IRQ_INPUT, avr_twi_irq_msg(TWI_COND_ACK, mpu->self_addr, 1));
		printf("MPU 6050 (t=%5.3fs) @%02x [",  1. * mpu->avr->cycle / mpu->avr->frequency, mpu->self_addr);

		// reset write sequence
		mpu->write_step = 0;

		// update simulation
		simu_update(mpu->avr->cycle, mpu->avr->frequency, mpu->regs + 0x3b);

		return;
	}

	// WRITE
	if (v.u.twi.msg & TWI_COND_WRITE) {
		// ackwonledge it
		avr_raise_irq(mpu->irq + TWI_IRQ_INPUT, avr_twi_irq_msg(TWI_COND_ACK, mpu->self_addr, 1));

		// if first write access since START
		if ( mpu->write_step == 0 ) {
			printf("%02x+", v.u.twi.data);

			// set the register index
			mpu->current_reg = v.u.twi.data;
			mpu->write_step = 1;
		}
		// else
		else {
			printf("%02x+", v.u.twi.data);

			// write data at index
			mpu->regs[mpu->current_reg] = v.u.twi.data;

			// and points to next register
			mpu->current_reg++;
		}
	}

	// READ
	if (v.u.twi.msg & TWI_COND_READ) {
		uint8_t data;

		switch (mpu->current_reg) {
		case 0x75:	// MPU6050_WHO_AM_I
			data = 0x68;
			break;

		default:
			data = mpu->regs[mpu->current_reg];
			break;
		}

		printf("%02x+", data);
		avr_raise_irq(mpu->irq + TWI_IRQ_INPUT, avr_twi_irq_msg(TWI_COND_READ, mpu->self_addr, data));

		mpu->current_reg++;
	}
#endif
}

static const char * spi_irq_names[SPI_IRQ_COUNT] = {
		[SPI_IRQ_INPUT] = "8<sc18is600.in",
		[SPI_IRQ_OUTPUT] = "8>sc18is600.out",
};


void sc18is600_init(struct avr_t * avr)
{
    sc18 = malloc(sizeof(sc18is600_t));

	memset(sc18, 0, sizeof(sc18is600_t));
	sc18->avr = avr;
#if 0
    sc18->irq = avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT);
	avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT), sc18_in_hook, sc18);
#else
    sc18->irq = avr_alloc_irq(&avr->irq_pool, SPI_IRQ_INPUT, SPI_IRQ_COUNT, spi_irq_names);
    avr_connect_irq(sc18->irq + SPI_IRQ_INPUT, avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT));
    avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT), sc18->irq + SPI_IRQ_OUTPUT);
	avr_irq_register_notify(sc18->irq + SPI_IRQ_OUTPUT, sc18_in_hook, sc18);
#endif
}


void simu_component_init(struct avr_t * avr)
{
	sc18is600_init(avr);
}


void simu_component_fini(struct avr_t * avr)
{
	free(sc18);
}


