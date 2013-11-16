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

#include "sc18is600.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sim_avr.h"
#include "avr_spi.h"
#include "avr_twi.h"
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
	SC18_NOP = 0xff,    // fake command meaning there no command
} sc18_cmd_t;

typedef enum {
    SC18_FROM_CS_HOOK,
    SC18_FROM_I2C_HOOK,
} sc18_hook_t;

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
	SC18_SPI_CS_IRQ,
   	SC18_SPI_IRQ_COUNT,
} _sc18_spi_irq_t;


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
	avr_irq_t *	spi_irq;	// irq list for SPI AVR connection
	avr_irq_t *	i2c_irq;	// irq list for I2C bus

    struct sc18_regs_t regs;

	int cs;					// Chip Select

	uint8_t tx_buf[SC18_RX_BUF_SIZE];
	uint8_t rx_buf[SC18_RX_BUF_SIZE];

	sc18_cmd_t cmd;         // current performed command
    int fini;               // flag to signal command performing is ended
    union {
        int step;           // overlapping current step
        struct {
            int step;           // current step
            int index;          // current data index
            int len;            // length of the data to send
            uint8_t i2c_addr;   // target I2C address
        } wr_n;             // write n context
        struct {
            int step;           // current step
            int index;          // current data index
            int len;            // length of the data to send
            uint8_t i2c_addr;   // target I2C address
        } rd_n;             // read n context
    };

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
static void sc18_wr_n(sc18is600_t * sc18, sc18_hook_t hook)
{
    uint32_t msg;

    //printf("\nsc18_wr_n: step %02d ", sc18->step);
    if ( hook == SC18_FROM_CS_HOOK ) {
        //printf("from cs ");
    }
    else {
        //printf("from i2c ");
        return;
    }

	// step #0 is the command
	if ( sc18->step == 0 ) {
        sc18->wr_n.index = 3;
        sc18->wr_n.len = sc18->tx_buf[1];
        sc18->wr_n.i2c_addr = sc18->tx_buf[2];

        msg = avr_twi_irq_msg(TWI_COND_START, sc18->wr_n.i2c_addr, 0);
        avr_raise_irq(sc18->i2c_irq + SC18_I2C_IRQ_OUT, msg);
        sc18->step++;
        return;
    }

    // next steps up to limit are for writing the buffer
    if ( sc18->wr_n.len )  {
        msg = avr_twi_irq_msg(TWI_COND_WRITE, sc18->wr_n.i2c_addr, sc18->tx_buf[sc18->wr_n.index]);
        avr_raise_irq(sc18->i2c_irq + SC18_I2C_IRQ_OUT, msg);
        sc18->step++;
        sc18->wr_n.index++;
        sc18->wr_n.len--;
        return;
    }

    if ( sc18->wr_n.len == 0 )  {
        msg = avr_twi_irq_msg(TWI_COND_STOP, sc18->wr_n.i2c_addr, 0);
        avr_raise_irq(sc18->i2c_irq + SC18_I2C_IRQ_OUT, msg);
        sc18->fini = 1;
        return;
    }

    if ( sc18->wr_n.index > SC18_TX_BUF_SIZE ) {
        printf("buffer overflow %02d\n", sc18->wr_n.index);
        msg = avr_twi_irq_msg(TWI_COND_STOP, sc18->wr_n.i2c_addr, 0);
        avr_raise_irq(sc18->i2c_irq + SC18_I2C_IRQ_OUT, msg);
        sc18->fini = 1;
        return;
    }
}


// read n bytes to I2C-bus slave device
static void sc18_rd_n(sc18is600_t * sc18, sc18_hook_t hook, uint32_t value)
{
    uint32_t msg;

	//printf("sc18_rd_n: step %02d ", sc18->step);

    if ( hook == SC18_FROM_CS_HOOK ) {
        //printf("from cs ");
    }
    else {
        //printf("from i2c ");
        avr_twi_msg_irq_t v = { .u.v = value };

        sc18->rx_buf[sc18->rd_n.index] = v.u.twi.data;
        return;
    }

	// step #0 is the command
	if ( sc18->step == 0 ) {
        sc18->rd_n.index = 0;
        sc18->rd_n.len = sc18->tx_buf[1];
        sc18->rd_n.i2c_addr = sc18->tx_buf[2];

        msg = avr_twi_irq_msg(TWI_COND_START, sc18->rd_n.i2c_addr, 0);
        avr_raise_irq(sc18->i2c_irq + SC18_I2C_IRQ_OUT, msg);
        sc18->step++;
        return;
    }

    // next steps up to limit are for reading the buffer
    if ( sc18->rd_n.len )  {
        msg = avr_twi_irq_msg(TWI_COND_READ | TWI_COND_ACK, sc18->rd_n.i2c_addr, 0);
        avr_raise_irq(sc18->i2c_irq + SC18_I2C_IRQ_OUT, msg);
        sc18->step++;
        sc18->rd_n.index++;
        sc18->rd_n.len--;
        return;
    }

    if ( sc18->wr_n.len == 0 )  {
        msg = avr_twi_irq_msg(TWI_COND_STOP, sc18->wr_n.i2c_addr, 0);
        avr_raise_irq(sc18->i2c_irq + SC18_I2C_IRQ_OUT, msg);
        sc18->fini = 1;
        return;
    }

    if ( sc18->wr_n.index > SC18_TX_BUF_SIZE ) {
        printf("buffer overflow %02d\n", sc18->wr_n.index);
        msg = avr_twi_irq_msg(TWI_COND_STOP, sc18->wr_n.i2c_addr, 0);
        avr_raise_irq(sc18->i2c_irq + SC18_I2C_IRQ_OUT, msg);
        sc18->fini = 1;
        return;
    }
}


// I2C-bus write then read (read after write)
static void sc18_wr_rd(sc18is600_t * sc18, sc18_hook_t hook)
{
	printf("sc18_wr_rd: step %02d ", sc18->step);

    printf("not implemented yet! ");
    sc18->fini = 1;

	// step #0 is the command
	if ( sc18->step == 0 ) {
        return;
    }

    // next steps up to limit are for writing the buffer
    if ( sc18->step > SC18_TX_BUF_SIZE ) {
        printf("too many data %d ", sc18->step);
        return;
    }
}


// read buffer
static uint8_t sc18_rd_buf(uint8_t value, sc18is600_t * sc18)
{
	printf("step %02d ", sc18->step);

	// step #0 is the command
	if ( sc18->step == 0 ) {
        return 0xff;
    }

    // next steps up to limit are for reading the buffer
    if ( sc18->step > SC18_RX_BUF_SIZE ) {
        printf("offset out of bound %d ", sc18->step);
        return 0xff;
    }

    // send the content of the response buffer
	return sc18->rx_buf[sc18->step - 1];
}


// I2C-bus write after write
static void sc18_wr_wr(sc18is600_t * sc18, sc18_hook_t hook)
{
	printf("sc18_wr_wr: step %02d ", sc18->step);

    printf("not implemented yet! ");
    sc18->fini = 1;

	// step #0 is the command
	if ( sc18->step == 0 ) {
        return;
    }

    // next steps up to limit are for writing the buffer
    if ( sc18->step > SC18_TX_BUF_SIZE ) {
        printf("sc18is600: write after write: too many data %d\n", sc18->step);
    }
}


// SPI configuration
static uint8_t sc18_conf(uint8_t value, sc18is600_t * sc18)
{
	printf("step %02d ", sc18->step);

	switch (sc18->step) {
	case 0:
		// command is received
		break;

	case 1:
		// which configuration?
		switch (value) {
		case SC18_SPI_CONF_LSB:
			printf("conf LSB first ");
			break;

		case SC18_SPI_CONF_MSB:
			printf("conf MSB first ");
			break;

		default:
			printf("unknown conf 0x%02x ", value);
			break;
		}

		break;

	default:
		printf("conf invalid step %02d ", sc18->step);
		break;
	}

	return 0xff;
}


// write internal registers
static uint8_t sc18_wr_reg(uint8_t value, sc18is600_t * sc18)
{
	printf("step %02d ", sc18->step);

	switch (sc18->step) {
	case 0:
		printf("           ");
		// command is received
		break;

	case 1:
		// register offset is received
		if ( value < SC18_REGS_OFFSET_MIN && value > SC18_REGS_OFFSET_MAX ) {
			printf("write register invalid offset %d ", value);
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
		printf("wr_reg invalid step %02d ", sc18->step);
		break;
	}

	return 0xff;
}


// power-down mode
static uint8_t sc18_pwr(uint8_t value, sc18is600_t * sc18)
{
	printf("step %02d ", sc18->step);

	// sequence 0x30 0x5a 0xa5
	switch (sc18->step) {
	case 0:
		// command
		break;

	case 1:
		if ( value != SC18_PWR_STEP_1 ) {
			printf("invalid power-down step #1 0x%02x ", value);
		}
		break;

	case 2:
		if ( value != SC18_PWR_STEP_2 ) {
			printf("sc18is600: invalid power-down step #2 0x%02x ", value);
		}
		break;

	default:
		printf("sc18is600: power-down invalid sequence %d ", sc18->step);
		break;
	}

	return 0xff;
}


// read internal registers
static uint8_t sc18_rd_reg(uint8_t value, sc18is600_t * sc18)
{
	printf("step %02d ", sc18->step);

	switch (sc18->step) {
	case 0:
		// command is received
		printf("           ");
		break;

	case 1:
		// register offset is received
		if ( value < SC18_REGS_OFFSET_MIN && value > SC18_REGS_OFFSET_MAX ) {
			printf("invalid offset %d ", value);
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
		printf("invalid step %02d ", sc18->step);
		break;
	}

	return 0xff;
}


// called on every SPI transaction
static void sc18_spi_hook(struct avr_irq_t * irq, uint32_t value, void * param)
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
        printf("sc18_wr_n ");
        sc18->tx_buf[sc18->step] = value & 0xff;
		resp = 0xff;
		break;

	case SC18_RD_N:
        printf("sc18_rd_n ");
        sc18->tx_buf[sc18->step] = value & 0xff;
		resp = 0xff;
		break;

	case SC18_WR_RD:
        printf("sc18_wr_rd ");
        sc18->tx_buf[sc18->step] = value & 0xff;
		resp = 0xff;
		break;

	case SC18_WR_WR:
        printf("sc18_wr_wr ");
        sc18->tx_buf[sc18->step] = value & 0xff;
		resp = 0xff;
		break;

	case SC18_RD_BUF:
        printf("sc18_rd_buf ");
		resp = sc18_rd_buf(value, sc18);
		break;

	case SC18_CONF:
        printf("sc18_conf ");
		resp = sc18_conf(value, sc18);
		break;

	case SC18_WR_REG:
        printf("sc18_wr_reg ");
		resp = sc18_wr_reg(value, sc18);
		break;

	case SC18_RD_REG:
        printf("sc18_rd_reg ");
		resp = sc18_rd_reg(value, sc18);
		break;

	case SC18_PWR:
        printf("sc18_pwr ");
		resp = sc18_pwr(value, sc18);
		break;

	default:
		printf("invalid command [0x%02x], ignoring it ", sc18->cmd);
		resp = 0xff;
		break;
	}

	sc18->step++;

	// send response
    avr_raise_irq(sc18->spi_irq + SC18_SPI_IRQ_OUT, resp);
    printf("|tx --> 0x%02x\n", resp);
}

// called on every change on CS pin
static void sc18_spi_cs_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	sc18is600_t * sc18 = (sc18is600_t*)param;

	sc18->cs = value & SC18_CS_PB0;
	printf("sc18is600: cs = %d ", sc18->cs);

	// reset step as component selection changes
    sc18->step = 0;

    // component gets selected
    if ( sc18->cs == 0 ) {
        // reset command as component gets selected
        sc18->cmd = SC18_NOP;
        printf("\n");
        return;
    }

    // perform the requested I2C transaction
    sc18->fini = 0;
    while ( ! sc18->fini ) {
        switch (sc18->cmd) {
        case SC18_WR_N:
            sc18_wr_n(sc18, SC18_FROM_CS_HOOK);
            break;

        case SC18_RD_N:
            sc18_rd_n(sc18, SC18_FROM_CS_HOOK, value);
            break;

        case SC18_WR_RD:
            sc18_wr_rd(sc18, SC18_FROM_CS_HOOK);
            break;

        case SC18_WR_WR:
            sc18_wr_wr(sc18, SC18_FROM_CS_HOOK);
            break;

        case SC18_RD_BUF:
        case SC18_CONF:
        case SC18_WR_REG:
        case SC18_RD_REG:
        case SC18_PWR:
        case SC18_NOP:
            sc18->fini = 1;
            printf("\n");
            break;

        default:
            sc18->fini = 1;
            printf("unknown command 0x%02x ", sc18->cmd);
            printf("\n");
            break;
        }

    }
}


// called on every transaction on I2C bus
static void sc18_i2c_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	sc18is600_t * sc18 = (sc18is600_t*)param;

#if 0
    avr_twi_msg_irq_t v = { .u.v = value };

    printf("sc18_i2c_hook: i2c addr = 0x%02x data = 0x%02x msg = ", v.u.twi.addr, v.u.twi.data);
    if ( v.u.twi.msg & TWI_COND_START ) {
        printf("START ");
    }
    if ( v.u.twi.msg & TWI_COND_STOP ) {
        printf("STOP ");
    }
    if ( v.u.twi.msg & TWI_COND_READ ) {
        printf("READ ");
    }
    if ( v.u.twi.msg & TWI_COND_WRITE ) {
        printf("WRITE ");
    }
    if ( v.u.twi.msg & TWI_COND_ACK ) {
        printf("ACK ");
    }
#endif

    // continue the requested I2C transaction
    switch (sc18->cmd) {
    case SC18_WR_N:
        sc18_wr_n(sc18, SC18_FROM_I2C_HOOK);
        break;

    case SC18_RD_N:
        sc18_rd_n(sc18, SC18_FROM_I2C_HOOK, value);
        break;

    case SC18_WR_RD:
        sc18_wr_rd(sc18, SC18_FROM_I2C_HOOK);
        break;

    case SC18_WR_WR:
        sc18_wr_wr(sc18, SC18_FROM_I2C_HOOK);
        break;

    case SC18_RD_BUF:
    case SC18_CONF:
    case SC18_WR_REG:
    case SC18_RD_REG:
    case SC18_PWR:
    case SC18_NOP:
        break;

    default:
        printf("unknown command 0x%02x ", sc18->cmd);
        break;
    }
}


static const char * spi_irq_names[SC18_SPI_IRQ_COUNT] = {
		[SC18_SPI_IRQ_OUT] = "8<sc18is600.spi.out",
		[SC18_SPI_IRQ_IN] = "8>sc18is600.spi.in",
		[SC18_SPI_CS_IRQ] = "8>sc18is600.spi.cs",
};

static const char * i2c_irq_names[SC18_I2C_IRQ_COUNT] = {
		[SC18_I2C_IRQ_OUT] = "8<sc18is600.i2c.out",
		[SC18_I2C_IRQ_IN] = "8>sc18is600.i2c.in",
};


void sc18is600_init(struct avr_t * avr)
{
    printf("sc18is600 registered\n");

    sc18 = malloc(sizeof(sc18is600_t));

	memset(sc18, 0, sizeof(sc18is600_t));
	sc18->avr = avr;

    // init of the SPI side
    sc18->spi_irq = avr_alloc_irq(&avr->irq_pool, SC18_SPI_IRQ_IN, SC18_SPI_IRQ_COUNT, spi_irq_names);

    // bus
    avr_connect_irq(sc18->spi_irq + SC18_SPI_IRQ_OUT, avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_INPUT));
    avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT), sc18->spi_irq + SC18_SPI_IRQ_IN);
	avr_irq_register_notify(sc18->spi_irq + SC18_SPI_IRQ_IN, sc18_spi_hook, sc18);

    // CS
    avr_connect_irq(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(SC18_CS_PORT), SC18_CS_PIN), sc18->spi_irq + SC18_SPI_CS_IRQ);
	avr_irq_register_notify(sc18->spi_irq + SC18_SPI_CS_IRQ, sc18_spi_cs_hook, sc18);

    // init of the I2C side
    sc18->i2c_irq = avr_alloc_irq(&avr->irq_pool, SC18_I2C_IRQ_IN, SC18_I2C_IRQ_COUNT, i2c_irq_names);

    // only register the ISR
    // it is the responsability of the I2C component to connect to this I2C bus
	avr_irq_register_notify(sc18->i2c_irq + SC18_I2C_IRQ_IN, sc18_i2c_hook, sc18);
}


void simu_component_init(struct avr_t * avr)
{
	sc18is600_init(avr);
}


void simu_component_fini(struct avr_t * avr)
{
	free(sc18);
}


// to allow other components to connect to I2C bus
avr_irq_t * sc18is600_i2c_irq_get(void)
{
    return sc18->i2c_irq;
}
