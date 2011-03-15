/*
	i2ctest.c

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



#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <pthread.h>

#include "sim_avr.h"
#include "avr_twi.h"
#include "sim_elf.h"
#include "sim_gdb.h"
#include "sim_vcd_file.h"


typedef struct i2c_eeprom_t {
	avr_irq_t *	irq;		// irq list
	uint8_t addr_base;
	uint8_t addr_mask;

	uint8_t selected;		// selected address
	int index;	// byte index

	uint16_t reg_addr;		// read/write address register
	int size;				// also implies the address size, one or two byte
	uint8_t ee[4096];
} i2c_eeprom_t;

#include <string.h>

/*
 * called when a RESET signal is sent
 */
static void
i2c_eeprom_in_hook(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	i2c_eeprom_t * p = (i2c_eeprom_t*)param;
	avr_twi_msg_irq_t v;
	v.u.v = value;

	if (v.u.twi.msg & TWI_COND_STOP) {
		if (p->selected) {
			// it was us !
			printf("eeprom received stop\n");
		}
		p->selected = 0;
		p->index = 0;
		p->reg_addr = 0;
	}
	if (v.u.twi.msg & TWI_COND_START) {
		p->selected = 0;
		p->index = 0;
		if ((p->addr_base & p->addr_mask) == (v.u.twi.addr & p->addr_mask)) {
			// it's us !
			p->selected = v.u.twi.addr;
			avr_raise_irq(p->irq + TWI_IRQ_MISO,
					avr_twi_irq_msg(TWI_COND_ACK, p->selected, 1));
		}
	}
	if (p->selected) {
		if (v.u.twi.msg & TWI_COND_WRITE) {
			printf("eeprom write %02x\n", v.u.twi.data);
			// address size is how many bytes we use for address register
			avr_raise_irq(p->irq + TWI_IRQ_MISO,
					avr_twi_irq_msg(TWI_COND_ACK, p->selected, 1));
			int addr_size = p->size > 256 ? 2 : 1;
			if (p->index < addr_size) {
				p->reg_addr |= (v.u.twi.data << (p->index * 8));
				printf("eeprom set address to %04x\n", p->reg_addr);
			} else {
				printf("eeprom WRITE data %04x: %02x\n", p->reg_addr, v.u.twi.data);
				p->ee[p->reg_addr++] = v.u.twi.data;
			}
			p->reg_addr &= (p->size -1);
			p->index++;
		}
		if (v.u.twi.msg & TWI_COND_READ) {
			printf("eeprom READ data %04x: %02x\n", p->reg_addr, p->ee[p->reg_addr]);
			uint8_t data = p->ee[p->reg_addr++];
			avr_raise_irq(p->irq + TWI_IRQ_MISO,
					avr_twi_irq_msg(TWI_COND_READ, p->selected, data));
			p->reg_addr &= (p->size -1);
			p->index++;
		}
	}
}

static const char * _ee_irq_names[2] = {
		[TWI_IRQ_MISO] = "8>eeprom.out",
		[TWI_IRQ_MOSI] = "32<eeprom.in",
};

void
i2c_eeprom_init(
		struct avr_t * avr,
		i2c_eeprom_t * p,
		uint8_t addr,
		uint8_t mask,
		uint8_t * data,
		size_t size)
{
	memset(p, 0, sizeof(p));
	memset(p->ee, 0xff, sizeof(p->ee));
	p->irq = avr_alloc_irq(&avr->irq_pool, 0, 2, _ee_irq_names);
	avr_irq_register_notify(p->irq + TWI_IRQ_MOSI, i2c_eeprom_in_hook, p);

	p->size = size > sizeof(p->ee) ? sizeof(p->ee) : size;
	if (data)
		memcpy(p->ee, data, p->size);
}

avr_t * avr = NULL;
avr_vcd_t vcd_file;

i2c_eeprom_t ee;

int main(int argc, char *argv[])
{
	elf_firmware_t f;
	const char * fname =  "atmega48_i2ctest.axf";
	char path[256];

	sprintf(path, "%s/%s", dirname(argv[0]), fname);
	printf("Firmware pathname is %s\n", fname);
	elf_read_firmware(fname, &f);

	printf("firmware %s f=%d mmcu=%s\n", fname, (int)f.frequency, f.mmcu);

	avr = avr_make_mcu_by_name(f.mmcu);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' now known\n", argv[0], f.mmcu);
		exit(1);
	}
	avr_init(avr);
	avr_load_firmware(avr, &f);

	// initialize our 'peripheral'
	i2c_eeprom_init(avr, &ee, 0xa0, 0xfe, NULL, 1024);

	// "connect" the IRQs of the button to the port pin of the AVR
	avr_connect_irq(
		ee.irq + TWI_IRQ_MISO,
		avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_MISO));
	avr_connect_irq(
		avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_MOSI),
		ee.irq + TWI_IRQ_MOSI );

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (0) {
		//avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	/*
	 *	VCD file initialization
	 *
	 *	This will allow you to create a "wave" file and display it in gtkwave
	 *	Pressing "r" and "s" during the demo will start and stop recording
	 *	the pin changes
	 */
	avr_vcd_init(avr, "gtkwave_output.vcd", &vcd_file, 100000 /* usec */);
	avr_vcd_add_signal(&vcd_file,
		avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_STATUS), 8 /* bits */ ,
		"TWSR" );

	printf( "Demo launching:\n"
			);

	while (1)
		avr_run(avr);
}
