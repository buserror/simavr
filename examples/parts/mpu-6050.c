/*
	mpu-6050.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
	modified by Yann GOUY

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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sim_avr.h"
#include "avr_twi.h"

#include "mpu-6050.h"


// quite basic simulation of the MPU-6050
//

#define NB_REGS	0x100


typedef struct {
	avr_irq_t *	irq;		// irq list
	uint8_t self_addr;		// I2C salve address

	uint8_t current_reg;	// current accessed register

	int write_step;			// to handle modification of the register index
	uint8_t regs[NB_REGS];	// internal register map
} mpu_6050_t;


// called on every I2C transaction
static void mpu_6050_in_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	mpu_6050_t * mpu = (mpu_6050_t*)param;
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
		printf("STOP\n");
		return;
	}

	// START received
	if ( v.u.twi.msg & TWI_COND_START ) {
		// ackwonledge it
		avr_raise_irq(mpu->irq + TWI_IRQ_MISO, avr_twi_irq_msg(TWI_COND_ACK, mpu->self_addr, 1));
		printf("MPU 6050 @ 0x%02x: START ",  mpu->self_addr);

		// reset write sequence
		mpu->write_step = 0;

		return;
	}

	// WRITE
	if (v.u.twi.msg & TWI_COND_WRITE) {
		// ackwonledge it
		avr_raise_irq(mpu->irq + TWI_IRQ_MISO, avr_twi_irq_msg(TWI_COND_ACK, mpu->self_addr, 1));

		// if first write access since START
		if ( mpu->write_step == 0 ) {
			printf("W index <- 0x%02x ", v.u.twi.data);

			// set the register index
			mpu->current_reg = v.u.twi.data;
			mpu->write_step = 1;
		}
		// else
		else {
			printf("W 0x%02x ", v.u.twi.data);

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

		printf("R %02x ", data);
		avr_raise_irq(mpu->irq + TWI_IRQ_MISO, avr_twi_irq_msg(TWI_COND_READ, mpu->self_addr, data));

		mpu->current_reg++;
	}
}

static const char * _ee_irq_names[2] = {
		[TWI_IRQ_MISO] = "8>mpu_6050.out",
		[TWI_IRQ_MOSI] = "32<mpu_6050.in",
};

void mpu_6050_init(struct avr_t * avr, uint8_t addr)
{
	mpu_6050_t * mpu = malloc(sizeof(mpu_6050_t));

	memset(mpu, 0, sizeof(mpu_6050_t));
	mpu->self_addr = addr;

	mpu->irq = avr_alloc_irq(&avr->irq_pool, 0, 2, _ee_irq_names);
	avr_irq_register_notify(mpu->irq + TWI_IRQ_MOSI, mpu_6050_in_hook, mpu);

	// "connect" the IRQs of the eeprom to the TWI/i2c master of the AVR
	uint32_t i2c_irq_base = AVR_IOCTL_TWI_GETIRQ(0);
	avr_connect_irq(mpu->irq + TWI_IRQ_MISO, avr_io_getirq(avr, i2c_irq_base, TWI_IRQ_MISO));
	avr_connect_irq(avr_io_getirq(avr, i2c_irq_base, TWI_IRQ_MOSI), mpu->irq + TWI_IRQ_MOSI);
}
