/*
		i2ctest.c

		Copyright 2021 Sebastian Koschmieder <sep@seplog.org>

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avr_ioport.h"
#include "avr_usi.h"
#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_gdb.h"
#include "sim_vcd_file.h"

static avr_cycle_count_t awaitCallback(struct avr_t * avr,	avr_cycle_count_t when,	void * param) {
	(void)avr; (void)when;
	*((int*)param) = 0;
	return 0;
}
static void await_avr_usec(avr_t *avr, uint32_t usec) {
	int looping = 1;
	if (usec) {
		avr_cycle_timer_register_usec(avr, usec, awaitCallback, &looping);
	}
	else {//Single cycle
		avr_cycle_timer_register(avr, 1UL, awaitCallback, &looping);
	}
	while (looping) {
		avr_run(avr);
	}
}

//---------------------------------------
const char avr_i2c_port = 'B';
const uint8_t avr_sda_bit = 0;
const uint8_t avr_scl_bit = 2;

typedef enum {
	SDA_SEND_LOW = 0,
	SDA_SEND_HIGH = 1,
	SDA_LISTENING = 2
} sda_mode_t;

typedef struct {
	avr_irq_t *slv_sda;
	avr_irq_t *slv_scl;
} i2c_bus_t;

struct msg {
	enum { TWI_WRITE, TWI_READ } mode;
	uint8_t address;
	char *buffer;
	size_t size;
};
struct chain_msgs {
	struct msg *msg;
	int msg_cnt;
};

typedef struct {
	avr_t *avr;
	i2c_bus_t bus;
	avr_irq_t *irq;
	uint8_t selected;
	struct chain_msgs *msg_chain;
	int current_msg;
	int cur_msg_byte;
} i2c_master;

static inline struct msg *msg_current( i2c_master *ma ) {
	return &ma->msg_chain->msg[ ma->current_msg ];
}

static inline struct msg *msg_preview( i2c_master *ma ) {
	return &ma->msg_chain->msg[ ma->current_msg + 1 ];
}

static inline int msg_hasNext( i2c_master *ma ) {
	return ( ma->current_msg + 1 ) < ma->msg_chain->msg_cnt;
}

static inline void msg_moveNext( i2c_master *ma ) {
	ma->current_msg++;
}

static inline void msg_free( i2c_master *ma ) {
	free( ma->msg_chain->msg );
	ma->msg_chain->msg = NULL;
	free( ma->msg_chain );
	ma->msg_chain = NULL;
}

//------------------------------------------------------------------------------
static inline uint8_t twi_get_bus_state(i2c_master *ma) {
	avr_ioport_state_t iostate;
	avr_ioctl(ma->avr, AVR_IOCTL_IOPORT_GETSTATE(avr_i2c_port), &iostate);
	uint8_t ret = (iostate.pin & (0x01U << avr_sda_bit)) ? 0b01U : 0;
	ret |= (iostate.pin & (0x01U << avr_scl_bit)) ? 0b10U : 0;
	return ret;
}

static void twi_set_sda(i2c_master *ma, sda_mode_t mode) {
	avr_ioport_state_t iostate;

	switch (mode) {
	case SDA_SEND_LOW:
		avr_raise_irq(ma->irq + USI_IRQ_DI, 0);
		ma->bus.slv_sda->flags |= IRQ_FLAG_STRONG;
		break;
	case SDA_SEND_HIGH:
		ma->bus.slv_sda->flags &= ~IRQ_FLAG_STRONG;
		avr_raise_irq(ma->irq + USI_IRQ_DI, 1);
		break;
	case SDA_LISTENING:
		ma->bus.slv_sda->flags &= ~IRQ_FLAG_STRONG;
		avr_ioctl(ma->avr, AVR_IOCTL_IOPORT_GETSTATE(avr_i2c_port), &iostate);
		if (!(iostate.ddr & (0x01U << avr_sda_bit)))
			avr_raise_irq(ma->irq + USI_IRQ_DI, 1);//pullup only if MCU not outputing already
		break;
	}
}
static void twi_set_scl(i2c_master *ma, uint8_t high) {
	avr_irq_t *usi_scl = avr_io_getirq(ma->avr, AVR_IOCTL_USI_GETIRQ(), USI_IRQ_USCK);
	if (high) {
		do { //Wait till the User Flag used for stretching is cleared
			avr_raise_irq(ma->irq + USI_IRQ_USCK, 1);
			await_avr_usec(ma->avr, 0);
		} while(usi_scl->value == 0);
	} else {
		avr_raise_irq(ma->irq + USI_IRQ_USCK, 0);
	}
}

static void twi_send_start(i2c_master *ma) {
	//Note if no stop was sent, the start will be a repeated start
	switch (twi_get_bus_state(ma)) {
	case 0b10: //SCL BUS_LINE_HIGH, SDA BUS_LINE_LOW
		await_avr_usec(ma->avr, 1);
		twi_set_scl(ma, 0);
		FALLTHROUGH
	case 0b00: //SCL BUS_LINE_LOW, SDA BUS_LINE_LOW
		await_avr_usec(ma->avr, 1);
		twi_set_sda(ma, SDA_SEND_HIGH);
		FALLTHROUGH
	case 0b01: //SCL BUS_LINE_LOW, SDA BUS_LINE_HIGH
		await_avr_usec(ma->avr, 1);
		twi_set_scl(ma, 1);
		FALLTHROUGH
	default: //SCL BUS_LINE_HIGH, SDA BUS_LINE_HIGH
		await_avr_usec(ma->avr, 1);
		break;
	}
	twi_set_sda(ma, SDA_SEND_LOW);	//SDA going low while SCL is high
	await_avr_usec(ma->avr, 0);
	twi_set_scl(ma, 0);
}

static void twi_send_stop(i2c_master *ma) {
	printf("Sending Stop\n");
	switch (twi_get_bus_state(ma)) {
	case 0b11: //SCL BUS_LINE_HIGH, SDA BUS_LINE_HIGH
		await_avr_usec(ma->avr, 1);
		twi_set_scl(ma, 0);
		FALLTHROUGH
	case 0b01: //SCL BUS_LINE_LOW, SDA BUS_LINE_HIGH
		await_avr_usec(ma->avr, 1);
		twi_set_sda(ma, SDA_SEND_LOW);
		FALLTHROUGH
	case 0b00: //SCL BUS_LINE_LOW, SDA BUS_LINE_LOW
		await_avr_usec(ma->avr, 1);
		twi_set_scl(ma, 1);
		FALLTHROUGH
	default: //SCL BUS_LINE_HIGH, SDA BUS_LINE_LOW
		await_avr_usec(ma->avr, 1);
		break;
	}
	twi_set_sda(ma, SDA_SEND_HIGH);
	await_avr_usec(ma->avr, 0);
}

static uint8_t twi_send_byte(i2c_master *ma, uint8_t byte) {
	if (ma->bus.slv_scl->value) {
		await_avr_usec(ma->avr, 0);
		twi_set_scl(ma, 0);
	}
	for (int i = 8; i != 0; --i) {
		await_avr_usec(ma->avr, 3); //Changing in the peak of the SCL low period
		twi_set_sda(ma, (byte & 0x80U) ? SDA_SEND_HIGH : SDA_SEND_LOW);
		await_avr_usec(ma->avr, 3);
		twi_set_scl(ma, 1);
		await_avr_usec(ma->avr, 6); //Time for slave to read
		twi_set_scl(ma, 0);
		byte <<= 1;
	}
	twi_set_sda(ma, SDA_LISTENING);
	await_avr_usec(ma->avr, 6); //Time for slave to change pin
	twi_set_scl(ma, 1);
	await_avr_usec(ma->avr, 3);//Reading in the peak of the SCL high period
	register uint8_t ret = !(ma->bus.slv_sda->value & 0xff);
	await_avr_usec(ma->avr, 3);
	twi_set_scl(ma, 0);
	return ret; //1 for Acked, 0 for Nacked
}

static uint8_t twi_read_byte(i2c_master *ma, uint8_t ack) {
	register uint8_t shift_reg = 0;
	if (ma->bus.slv_scl->value) {
		await_avr_usec(ma->avr, 0);
		twi_set_scl(ma, 0);
	}
	await_avr_usec(ma->avr, 0);
	twi_set_sda(ma, SDA_LISTENING);
	for (int i = 8; i != 0; --i) {
		shift_reg <<= 1;
		await_avr_usec(ma->avr, 6); //Time for slave to change the line
		twi_set_scl(ma, 1);
		await_avr_usec(ma->avr, 3); //Reading in the peak of the SCL high period
		shift_reg += !!(ma->bus.slv_sda->value & 0xff);
		await_avr_usec(ma->avr, 3);
		twi_set_scl(ma, 0);
	}
	await_avr_usec(ma->avr, 3); //Changing in the peak of the SCL low period
	twi_set_sda(ma, (ack) ? SDA_SEND_LOW : SDA_SEND_HIGH);
	await_avr_usec(ma->avr, 3);
	twi_set_scl(ma, 1);
	await_avr_usec(ma->avr, 6); //Time for slave to read
	twi_set_scl(ma, 0);
	return shift_reg;
}

void i2c_master_init(avr_t *avr, i2c_master *ma) {
	static const char *_master_irq_names[] = {
		[USI_IRQ_DI] = "master.sda",
		[USI_IRQ_DO] = "",
		[USI_IRQ_USCK] = "master.scl",
		[USI_IRQ_TIM0_COMP] = "",
	};
	ma->avr = avr;
	ma->bus.slv_sda = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(avr_i2c_port), avr_sda_bit);
	ma->bus.slv_scl = avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ(avr_i2c_port), avr_scl_bit);
	ma->irq = avr_alloc_irq(&avr->irq_pool, 0, USI_IRQ_COUNT, _master_irq_names);
	ma->selected = 0;
}

void i2c_master_attach(avr_t *avr, i2c_master *ma) {
	avr_connect_irq(ma->irq + USI_IRQ_DI, ma->bus.slv_sda);
	avr_connect_irq(ma->irq + USI_IRQ_USCK, ma->bus.slv_scl);
	avr_raise_irq(ma->irq + USI_IRQ_DI, 1UL);
	avr_raise_irq(ma->irq + USI_IRQ_USCK, 1UL);
}
//---------------------------------------

static void twi_startTransmission(i2c_master *ma, struct chain_msgs *msg_chain) {
	ma->current_msg = 0;
	ma->cur_msg_byte = 0;
	ma->msg_chain = msg_chain;

	if( !msg_chain || !msg_chain->msg || !msg_chain->msg_cnt) {
		return;
	}
	int addr_byte, ack = 0;

	do {
		ma->selected = msg_current(ma)->address;
		addr_byte = ma->selected << 1;
		if( msg_current(ma)->mode == TWI_READ ) {
			addr_byte |= 0x01U;
		}

		printf("Sending start to 0x%02X %c\n", ma->selected, msg_current(ma)->mode?'r':'w');
		twi_send_start(ma);
		ack = twi_send_byte(ma, addr_byte);
		if (!ack) {
			printf("NACKED\n");
			msg_current(ma)->size = 0;	//Meaning error in transmission
		} else {
			printf("ACKED\n");
			char *buf = msg_current(ma)->buffer;
			if( msg_current(ma)->mode == TWI_READ ) {
				for (int i = msg_current(ma)->size; i != 0; --i) {
					*buf = twi_read_byte(ma, 1);
					buf++;
				}
				twi_read_byte(ma, 0);
			} else {
				for (int i = 0; i < msg_current(ma)->size; ++i) {
					ack = twi_send_byte(ma, *buf);
					if (!ack) {
						printf("NACKED @ byte %d\n", i);
						break;
					}
					buf++;
				}
			}
		}
		//Stop here if there is no other message to send
		if (!msg_hasNext(ma)) {
			twi_send_stop(ma);
			break;
		}
		msg_moveNext(ma);
		//Checking if address has changed so STOP is sent
		if (ma->selected != msg_current(ma)->address) {
			twi_send_stop(ma);
		} //Otherwise repeated start will be used
	} while(1);
}

avr_cycle_count_t twi_sendSomething(i2c_master *ma, uint8_t addr) {
	static uint8_t adr_buf = '2';
	static char str_buf[] = "Hello AVR!";
	struct msg *msg = malloc( sizeof( struct msg ) * 2 );
	struct chain_msgs *msg_chain = malloc( sizeof( struct chain_msgs ) );
	if(!msg || !msg_chain)
		exit(1);

	msg[0].address = addr;
	msg[0].mode = TWI_WRITE;
	msg[0].buffer = ( char * ) &adr_buf;
	msg[0].size = sizeof( uint8_t );

	msg[1].address = addr;
	msg[1].mode = TWI_WRITE;
	msg[1].buffer = &str_buf[0];
	msg[1].size = sizeof( str_buf ) - 1;

	msg_chain->msg = msg;
	msg_chain->msg_cnt = 2;

	twi_startTransmission(ma, msg_chain);

	return 0;
}

avr_cycle_count_t twi_receiveSomething(i2c_master *ma, uint8_t addr) {
	static uint8_t adr_buf = '0';
	static char str_buf[7];
	struct msg *msg = malloc( sizeof( struct msg ) * 2 );
	struct chain_msgs *msg_chain = malloc( sizeof( struct chain_msgs ) );
	if(!msg || !msg_chain)
		exit(1);

	msg[0].address = addr;
	msg[0].mode = TWI_WRITE;
	msg[0].buffer = ( char * ) &adr_buf;
	msg[0].size = sizeof( uint8_t );

	msg[1].address = addr;
	msg[1].mode = TWI_READ;
	msg[1].buffer = &str_buf[0];
	msg[1].size = sizeof( str_buf ) - 1;

	msg_chain->msg = msg;
	msg_chain->msg_cnt = 2;

	twi_startTransmission( ma, msg_chain);
	if (msg[1].size)
		printf("Received Data: \e[31m%s\e[0m\n", str_buf);

	return 0;
}

int main(int argc, char *argv[]) {
	elf_firmware_t f = {};
	const char *fname = "attiny85_i2cslave.axf";

	printf("Firmware pathname is %s\n", fname);
	elf_read_firmware(fname, &f);

	printf("firmware %s f=%d mmcu=%s\n", fname, (int)f.frequency, f.mmcu);

	avr_t *avr = NULL;
	avr_vcd_t vcd_file;

	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' not known\n", argv[0], f.mmcu);
		exit(1);
	}
	avr_init(avr);
	avr_load_firmware(avr, &f);

	//Configuring Fuses for 8MHz operation
	avr->fuse[AVR_FUSE_LOW] = 0xE2;
	avr->fuse[AVR_FUSE_HIGH] = 0xDF;
	avr->fuse[AVR_FUSE_EXT] = 0xFF;

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	// avr->log = 4;
	if (0) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}
	avr_set_console_register(avr, 0x31/*GPIOR0*/);

	i2c_master master;
	i2c_master_init(avr, &master);
	i2c_master_attach(avr, &master);

	/*
	 *	VCD file initialization
	 *	This will allow you to create a "wave" file and display it in gtkwave
	 */
	avr_irq_t *ddrb_irq = avr_iomem_getirq(avr, 0x37, "DDRB", AVR_IOMEM_IRQ_ALL);
	avr_irq_t *usicr_irq = avr_iomem_getirq(avr, 0x2D, "USICR", AVR_IOMEM_IRQ_ALL);
	avr_irq_t *usisr_irq = avr_iomem_getirq(avr, 0x2E, "USISR", AVR_IOMEM_IRQ_ALL);
	avr_irq_t *usidr_irq = avr_iomem_getirq(avr, 0x2F, "USIDR", AVR_IOMEM_IRQ_ALL);
	avr_irq_t *usi_st_vect_irq = avr_get_interrupt_irq(avr, 13);
	avr_irq_t *usi_ov_vect_irq = avr_get_interrupt_irq(avr, 14);
	avr_vcd_init(avr, "gtkwave_output.vcd", &vcd_file, 100 /* usec */);
	avr_vcd_add_signal(&vcd_file, master.bus.slv_sda, 1 /* bits */, "SDA" );
	avr_vcd_add_signal(&vcd_file, master.bus.slv_scl, 1 /* bits */, "SCL" );
	avr_vcd_add_signal(&vcd_file, ddrb_irq , 8 /* bits */, "DDRB");
	avr_vcd_add_signal(&vcd_file, usicr_irq, 8 /* bits */, "USICR");
	avr_vcd_add_signal(&vcd_file, usisr_irq, 8 /* bits */, "USISR");
	avr_vcd_add_signal(&vcd_file, usidr_irq, 8 /* bits */, "USIDR");
	avr_vcd_add_signal(&vcd_file, usi_st_vect_irq, 1 /* bits */, "USIST_vect" );
	avr_vcd_add_signal(&vcd_file, usi_ov_vect_irq, 1 /* bits */, "USIOV_vect" );

	printf("\nDemo launching:\n");
	avr_vcd_start(&vcd_file);

	await_avr_usec(avr, 1000);
	twi_sendSomething(&master, 0x42);
	await_avr_usec(avr, 100000);
	twi_receiveSomething(&master, 0x42);
	await_avr_usec(avr, 100000);
	twi_sendSomething(&master, 0x21);
	await_avr_usec(avr, 100000);
	twi_receiveSomething(&master, 0x21);
	await_avr_usec(avr, 200000);

	avr_vcd_close(&vcd_file);
	await_avr_usec(avr, 100000);
	return 0;
}
