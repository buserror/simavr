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
#include "avr_ioport.h"		// AVR_IOCTL_IOPORT_GETSTATE()
#include "sim_io.h"			// avr_ioctl()


//--------------------------------------------------------------------
// private defines
//

#define SC18_TX_BUF_SIZE	96
#define SC18_RX_BUF_SIZE	96

#define SC18_SPI_CONF_LSB	0x81
#define SC18_SPI_CONF_MSB	0x42

#define SC18_PWR_STEP_1		0x5a
#define SC18_PWR_STEP_2		0xa5

typedef enum {
	SC18_WR_N = 0x00,
	SC18_RD_N = 0x01,
	SC18_WR_RD = 0x02,
	SC18_RD_BUF = 0x06,
	SC18_WR_WR = 0x03,
	SC18_CONF = 0x18,
	SC18_WR_REG = 0x20,
	SC18_RD_REG = 0x21,
	SC18_PWR = 0x30,
} sc18_cmd_t;

// internal register map
typedef struct sc18_regs_t {
        uint8_t IOconfig;
        uint8_t IOstate;
        uint8_t I2CClock;
        uint8_t I2CTO;
        uint8_t I2CStat;
        uint8_t I2CAddr;
} _sc18_regs_t;

#define SC18_REGS_OFFSET_MIN	0
#define SC18_REGS_OFFSET_MAX	(sizeof(struct sc18_regs_t) / sizeof(uint8_t))

#define SC18_CS_PB0				0x01
#define SC18_CS_PORT			'B'
#define SC18_CS_PIN				0

typedef enum {
	SC18_SPI_IRQ_IN,
	SC18_SPI_IRQ_OUT,
	SC18_CS_IRQ,
   	SC18_IRQ_COUNT,
} _sc18_irq_t;

//--------------------------------------------------------------------
// private constants
//

static const char* const reg_name[] = {
	"IOconfig",
	"IOstate ",
	"I2CClock",
	"I2CTO   ",
	"I2CStat ",
	"I2CAddr ",
};


//--------------------------------------------------------------------
// private structure definitions
//

typedef struct {
	struct avr_t * avr;		// AVR access for time
	avr_irq_t *	irq;		// irq list

    struct sc18_regs_t regs;

	int cs;					// Chip Select

	int step;				// internal command step
	sc18_cmd_t cmd;

	uint8_t tx_buf[SC18_TX_BUF_SIZE];
	uint8_t rx_buf[SC18_RX_BUF_SIZE];

	uint8_t reg_offset;
} sc18is600_t;


//--------------------------------------------------------------------
// private variables
//

static sc18is600_t * sc18;


//--------------------------------------------------------------------
// private functions
//

// write n bytes to I2C-bus slave device
static uint8_t sc18_wr_n(uint8_t value, sc18is600_t * sc18)
{
	printf("sc18_wr_n: step %02d ", sc18->step);

	// step #0 is the command
	if ( sc18->step != 0 ) {
		// next steps up to limit are for writing the buffer
		if ( sc18->step > SC18_TX_BUF_SIZE ) {
			printf("sc18is600: write n bytes: too many data %d\n", sc18->step);
		}
		else {
			// store the frame
			sc18->tx_buf[sc18->step - 1] = value;
		}
	}

	return 0xff;
}


// read n bytes to I2C-bus slave device
static uint8_t sc18_rd_n(uint8_t value, sc18is600_t * sc18)
{
	printf("sc18_rd_n: step %02d ", sc18->step);

	// step #0 is the command
	if ( sc18->step != 0 ) {
		// next steps up to limit are for writing the buffer
		if ( sc18->step > SC18_TX_BUF_SIZE ) {
			printf("sc18is600: read n bytes: too many data %d\n", sc18->step);
		}
		else {
			// store the frame
			sc18->tx_buf[sc18->step - 1] = value;
		}
	}

	return 0xff;
}


// I2C-bus write then read (read after write)
static uint8_t sc18_wr_rd(uint8_t value, sc18is600_t * sc18)
{
	printf("sc18_wr_rd: step %02d ", sc18->step);

	// step #0 is the command
	if ( sc18->step != 0 ) {
		// next steps up to limit are for writing the buffer
		if ( sc18->step > SC18_TX_BUF_SIZE ) {
			printf("sc18is600: read after write: too many data %d\n", sc18->step);
		}
		else {
			// store the frame
			sc18->tx_buf[sc18->step - 1] = value;
		}
	}

	return 0xff;
}


// read buffer
static uint8_t sc18_rd_buf(uint8_t value, sc18is600_t * sc18)
{
	printf("sc18_rd_buf: step %02d ", sc18->step);

	// step #0 is the command
	if ( sc18->step != 0 ) {
		// next steps up to limit are for reading the buffer
		if ( sc18->step > SC18_RX_BUF_SIZE ) {
			printf("sc18is600: read buffer: offset out of bound %d\n", sc18->step);
		}
		else {
			// send the I2C received byte
			return sc18->rx_buf[sc18->step - 1];
		}
	}

	return 0xff;
}


// I2C-bus write after write
static uint8_t sc18_wr_wr(uint8_t value, sc18is600_t * sc18)
{
	printf("sc18_wr_wr: step %02d ", sc18->step);

	// step #0 is the command
	if ( sc18->step != 0 ) {
		// next steps up to limit are for writing the buffer
		if ( sc18->step > SC18_TX_BUF_SIZE ) {
			printf("sc18is600: write after write: too many data %d\n", sc18->step);
		}
		else {
			// store the frame
			sc18->tx_buf[sc18->step - 1] = value;
		}
	}

	return 0xff;
}


// SPI configuration
static uint8_t sc18_conf(uint8_t value, sc18is600_t * sc18)
{
	printf("sc18_conf: step %02d ", sc18->step);

	switch (sc18->step) {
	case 0:
		// command is received
		break;

	case 1:
		// which configuration?
		switch (value) {
		case SC18_SPI_CONF_LSB:
			printf("sc18is600: conf LSB first\n");
			break;

		case SC18_SPI_CONF_MSB:
			printf("sc18is600: conf MSB first\n");
			break;

		default:
			printf("sc18is600: unknown conf 0x%02x\n", value);
			break;
		}

		break;

	default:
		printf("sc18is600: conf invalid step %02d\n", sc18->step);
		break;
	}

	return 0xff;
}


// write internal registers
static uint8_t sc18_wr_reg(uint8_t value, sc18is600_t * sc18)
{
	printf("sc18_wr_reg: step %02d ", sc18->step);

	switch (sc18->step) {
	case 0:
		printf("           ");
		// command is received
		break;

	case 1:
		// register offset is received
		if ( value < SC18_REGS_OFFSET_MIN && value > SC18_REGS_OFFSET_MAX ) {
			printf("sc18is600: write register invalid offset %d\n", value);
		}
		else {
			sc18->reg_offset = value;
			printf("[%s] ", reg_name[value]);
		}
		break;

	case 2:
		printf("           ");
		// register value is received
		*((uint8_t*)&sc18->regs + sc18->reg_offset) = value;
		break;

	default:
		printf("sc18is600: wr_reg invalid step %02d\n", sc18->step);
		break;
	}

	return 0xff;
}


// power-down mode
static uint8_t sc18_pwr(uint8_t value, sc18is600_t * sc18)
{
	printf("sc18_pwr: step %02d ", sc18->step);

	// sequence 0x30 0x5a 0xa5
	switch (sc18->step) {
	case 0:
		// command
		break;

	case 1:
		if ( value != SC18_PWR_STEP_1 ) {
			printf("sc18is600: invalid power-down step #1 0x%02x\n", value);
		}
		break;

	case 2:
		if ( value != SC18_PWR_STEP_2 ) {
			printf("sc18is600: invalid power-down step #2 0x%02x\n", value);
		}
		break;

	default:
		printf("sc18is600: power-down invalid sequence %d\n", sc18->step);
		break;
	}

	return 0xff;
}


// read internal registers
static uint8_t sc18_rd_reg(uint8_t value, sc18is600_t * sc18)
{
	printf("sc18_rd_reg: step %02d ", sc18->step);

	switch (sc18->step) {
	case 0:
		// command is received
		printf("           ");
		break;

	case 1:
		// register offset is received
		if ( value < SC18_REGS_OFFSET_MIN && value > SC18_REGS_OFFSET_MAX ) {
			printf("sc18is600: read register invalid offset %d\n", value);
		}
		else {
			sc18->reg_offset = value;
			printf("[%s] ", reg_name[value]);
		}
		break;

	case 2:
		printf("           ");
		// register value is received
		return *((uint8_t*)&sc18->regs + sc18->reg_offset);
		break;

	default:
		printf("sc18is600: wr_reg invalid step %02d\n", sc18->step);
		break;
	}

	return 0xff;
}


// called on every SPI transaction
static void sc18_in_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	uint8_t resp;
	sc18is600_t * sc18 = (sc18is600_t*)param;

	// check if chip is selected
	if ( sc18->cs == 1 ) {
		sc18->step = 0;
		printf("sc18is600 CS is 1\n");
		return;
	}

    // new char received
    printf("sc18is600: 0x%02x --> rx| ", value);

	if ( sc18->step == 0 ) {
		sc18->cmd = value;
	}

	// which command?
	switch (sc18->cmd) {
	case SC18_WR_N:
		resp = sc18_wr_n(value, sc18);
		break;

	case SC18_RD_N:
		resp = sc18_rd_n(value, sc18);
		break;

	case SC18_WR_RD:
		resp = sc18_wr_rd(value, sc18);
		break;

	case SC18_RD_BUF:
		resp = sc18_rd_buf(value, sc18);
		break;

	case SC18_WR_WR:
		resp = sc18_wr_wr(value, sc18);
		break;

	case SC18_CONF:
		resp = sc18_conf(value, sc18);
		break;

	case SC18_WR_REG:
		resp = sc18_wr_reg(value, sc18);
		break;

	case SC18_RD_REG:
		resp = sc18_rd_reg(value, sc18);
		break;

	case SC18_PWR:
		resp = sc18_pwr(value, sc18);
		break;

	default:
		printf("sc18is600: invalid command [0x%02x], ignoring it ", sc18->cmd);
		resp = 0xff;
		break;
	}

	sc18->step++;

	// send response
    avr_raise_irq(sc18->irq + SC18_SPI_IRQ_OUT, resp);
    printf("|tx --> 0x%02x\n", resp);
}

// called on every change on CS pin
static void sc18_cs_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	sc18is600_t * sc18 = (sc18is600_t*)param;

	sc18->cs = value & SC18_CS_PB0;
	printf("sc18is600: cs = %d\n", sc18->cs);

	// reset step when component is no longer selected
	if ( sc18->cs == 1 ) {
		sc18->step = 0;
	}
}


static const char * spi_irq_names[SC18_IRQ_COUNT] = {
		[SC18_SPI_IRQ_OUT] = "8<sc18is600.out",
		[SC18_SPI_IRQ_IN] = "8>sc18is600.in",
		[SC18_CS_IRQ] = "8>sc18is600.cs",
};


void sc18is600_init(struct avr_t * avr)
{
    sc18 = malloc(sizeof(sc18is600_t));

	memset(sc18, 0, sizeof(sc18is600_t));
	sc18->avr = avr;
    sc18->irq = avr_alloc_irq(&avr->irq_pool, SC18_SPI_IRQ_IN, SC18_IRQ_COUNT, spi_irq_names);

    avr_connect_irq(sc18->irq + SC18_SPI_IRQ_OUT, avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT));
    avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT), sc18->irq + SC18_SPI_IRQ_IN);
	avr_irq_register_notify(sc18->irq + SC18_SPI_IRQ_IN, sc18_in_hook, sc18);

    avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(SC18_CS_PORT), SC18_CS_PIN), sc18->irq + SC18_CS_IRQ);
	avr_irq_register_notify(sc18->irq + SC18_CS_IRQ, sc18_cs_hook, sc18);
}


void simu_component_init(struct avr_t * avr)
{
	sc18is600_init(avr);
}


void simu_component_fini(struct avr_t * avr)
{
	free(sc18);
}


