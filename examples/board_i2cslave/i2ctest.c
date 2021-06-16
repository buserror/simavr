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

#include "avr_twi.h"
#include "i2c_eeprom.h"
#include "sim_avr.h"
#include "sim_elf.h"
#include "sim_gdb.h"

//---------------------------------------
typedef struct {
  avr_t *avr;
  avr_irq_t *irq;
  uint8_t selected;
} i2c_master;

static const char msg[] = { 0x10, 'H', 'e', 'l', 'l', 'o', ' ', 'A', 'V', 'R', '!' };
static int msg_index = 0;

static void i2c_master_in_hook(struct avr_irq_t *irq, uint32_t value,
                               void *param) {
  ((void)irq);

  i2c_master *p = (i2c_master *)param;
  avr_twi_msg_irq_t v;
  v.u.v = value;

  switch (v.u.twi.msg) {
  // case TWI_COND_ACK:
  //   avr_raise_irq(avr_io_getirq(p->avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT),
  //                 avr_twi_irq_msg(TWI_COND_WRITE, p->selected, 'B'));
  //   break;
  case (TWI_COND_ACK | TWI_COND_ADDR):
    if( msg_index < sizeof( msg ) ) {
      char c = msg[msg_index++];
      printf( "Send Databyte: '%c' (0x%02X)\n", c, c );
      avr_raise_irq(avr_io_getirq(p->avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT),
                    avr_twi_irq_msg(TWI_COND_WRITE, p->selected, c ));
    }
    else {
      printf( "Send stop!\n" );
      avr_raise_irq(avr_io_getirq(p->avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT),
                    avr_twi_irq_msg(TWI_COND_STOP, p->selected, 0));
    }
    break;
  default:
    avr_raise_irq(avr_io_getirq(p->avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT),
                  avr_twi_irq_msg(TWI_COND_STOP, p->selected, 0));
    p->selected = 0;
    break;
  }
}

static const char *_master_irq_names[2] = {
    [TWI_IRQ_INPUT] = "8>master.out",
    [TWI_IRQ_OUTPUT] = "32<master.in",
};

void i2c_master_init(avr_t *avr, i2c_master *p) {
  p->avr = avr;
  p->irq = avr_alloc_irq(&avr->irq_pool, 0, 2, _master_irq_names);
  p->selected = 0;
  avr_irq_register_notify(p->irq + TWI_IRQ_OUTPUT, i2c_master_in_hook, p);
}

void i2c_master_attach(avr_t *avr, i2c_master *p, uint32_t i2c_irq_base) {
  avr_connect_irq(p->irq + TWI_IRQ_INPUT,
                  avr_io_getirq(avr, i2c_irq_base, TWI_IRQ_INPUT));
  avr_connect_irq(avr_io_getirq(avr, i2c_irq_base, TWI_IRQ_OUTPUT),
                  p->irq + TWI_IRQ_OUTPUT);
}
//---------------------------------------

avr_t *avr = NULL;

int main(int argc, char *argv[]) {
  elf_firmware_t f;
  const char *fname = "atmega328p_i2cslave.axf";

  printf("Firmware pathname is %s\n", fname);
  elf_read_firmware(fname, &f);

  printf("firmware %s f=%d mmcu=%s\n", fname, (int)f.frequency, f.mmcu);

  avr = avr_make_mcu_by_name(f.mmcu);
  if (!avr) {
    fprintf(stderr, "%s: AVR '%s' not known\n", argv[0], f.mmcu);
    exit(1);
  }
  avr_init(avr);
  avr_load_firmware(avr, &f);

  // even if not setup at startup, activate gdb if crashing
  avr->gdb_port = 1234;
  if (0) {
    avr->state = cpu_Stopped;
    avr_gdb_init(avr);
  }

  avr_flashaddr_t main_addr = 0;
  for (int i = 0; i < f.symbolcount; i++) {
    // if (strncmp(f.symbol[i]->symbol, "_avr_twi_simple_slave", 128) == 0) {
    if (strncmp(f.symbol[i]->symbol, "main", 128) == 0) {
      main_addr = f.symbol[i]->addr;
      break;
    }
  }

  i2c_master ma;
  i2c_master_init(avr, &ma);
  i2c_master_attach(avr, &ma, AVR_IOCTL_TWI_GETIRQ(0));

  avr->log = 4;

  int send = 0;

  printf("\nDemo launching:\n");

  int state = cpu_Running;
  while ((state != cpu_Done) && (state != cpu_Crashed)) {
    state = avr_run(avr);
    if (!send && avr->pc == main_addr) {
      ma.selected = 0x42;
      msg_index = 0;
      printf( "Send start to 0x%02X\n", ma.selected );
      avr_raise_irq(
          avr_io_getirq(avr, AVR_IOCTL_TWI_GETIRQ(0), TWI_IRQ_INPUT),
          avr_twi_irq_msg(TWI_COND_START | TWI_COND_ADDR | TWI_COND_WRITE,
                          ma.selected, 1));
      send = 1;
    }
  }
}
