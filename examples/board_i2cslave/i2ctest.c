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
struct msg {
  enum { TWI_WRITE, TWI_READ } mode;
  uint8_t address;
  char *buffer;
  size_t size;
};
struct nmsg {
  struct msg *msg;
  int nmsg;
};

typedef struct {
  avr_t *avr;
  avr_irq_t *irq;
  uint8_t selected;
  struct nmsg *msgs;
  int current_msg;
  int index;
} i2c_master;

static inline struct msg *msg_current( i2c_master *m ) {
  return &m->msgs->msg[ m->current_msg ];
}

static inline struct msg *msg_preview( i2c_master *m ) {
  return &m->msgs->msg[ m->current_msg + 1 ];
}

static inline int msg_hasNext( i2c_master *m ) {
  return ( m->current_msg + 1 ) < m->msgs->nmsg;
}

static inline void msg_moveNext( i2c_master *m ) {
  m->current_msg++;
}

static inline void msg_free( i2c_master *m ) {
      free( m->msgs->msg );
      m->msgs->msg = NULL;
      free( m->msgs );
      m->msgs = NULL;
}

static void i2c_master_send_hook(struct avr_irq_t *irq, uint32_t value,
                               void *param) {
  ((void)irq);

  i2c_master *p = (i2c_master *)param;
  avr_twi_msg_irq_t v;
  v.u.v = value;

  switch (v.u.twi.msg) {
  case (TWI_COND_ACK | TWI_COND_ADDR):

    if( p->index >= msg_current( p )->size ) {
      // if there is a next message
      // and the address and mode is the same
      // goto the next and send further
      // if( msg_hasNext( p ) && msg_preview(p)->mode == TWI_WRITE && msg_preview(p)->address == p->selected ) {
      //   p->index = 0;
      //   msg_moveNext(p);
      // }
      if( msg_hasNext(p) && msg_current(p)->mode == msg_preview(p)->mode ) {
        p->index = 0;
        msg_moveNext(p);
      }
      else if( msg_hasNext(p) && msg_current(p)->mode != msg_preview(p)->mode ) {
        p->index = 0;
        msg_moveNext(p);
        printf( "Send start to 0x%02X\n", p->selected );
        avr_raise_irq(
            p->irq + TWI_IRQ_INPUT,
            avr_twi_irq_msg(TWI_COND_START | TWI_COND_ADDR | ( msg_current(p)->mode == TWI_WRITE ? TWI_COND_WRITE : TWI_COND_READ ),
                            p->selected, 1));
        break;
      }
    }

    if( p->index < msg_current( p )->size ) {
      if( msg_current(p)->mode == TWI_WRITE ) {
        char c = msg_current(p)->buffer[ p->index++ ];
        printf( "Send Databyte: '%c' (0x%02X)\n", c, c );
        avr_raise_irq(p->irq + TWI_IRQ_INPUT,
          avr_twi_irq_msg(msg_current(p)->mode == TWI_WRITE ? TWI_COND_WRITE : TWI_COND_READ,
          p->selected, c ));
      }
    }
    else {
      if( msg_current(p)->mode == TWI_WRITE ) {
        printf( "Send stop!\n" );
        avr_raise_irq(p->irq + TWI_IRQ_INPUT, avr_twi_irq_msg( TWI_COND_STOP, p->selected, 0));
      }
      else {
        printf( "Send NACK!\n" );
        avr_raise_irq(p->irq + TWI_IRQ_INPUT,
          avr_twi_irq_msg( TWI_COND_READ, p->selected, 0 ));
      }

      p->selected = 0;
      msg_free(p);
    }
    break;
  case ( TWI_COND_ACK | TWI_COND_READ ):
  case ( TWI_COND_ACK | TWI_COND_ADDR | TWI_COND_READ ):
  //----------------
    msg_current(p)->buffer[ p->index++ ] = v.u.twi.data;
    printf( "Receive Databyte: '%c' (0x%02X)\n", v.u.twi.data, v.u.twi.data );
    avr_raise_irq(p->irq + TWI_IRQ_INPUT,
      avr_twi_irq_msg( TWI_COND_READ | ( p->index < msg_current(p)->size ? TWI_COND_ACK : 0 ),
        p->selected, 1 ));
  //----------------
    break;
  case TWI_COND_ACK:
    break;
  default:
    printf( "Undefined state: 0x%02X (0x%08X)\n", v.u.twi.msg, value );
    avr_raise_irq(p->irq + TWI_IRQ_INPUT,avr_twi_irq_msg(TWI_COND_STOP, p->selected, 0));
    // avr_irq_unregister_notify(p->irq + TWI_IRQ_OUTPUT, i2c_master_send_hook, p);

    p->selected = 0;
    msg_free(p);
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
}

void i2c_master_attach(avr_t *avr, i2c_master *p, uint32_t i2c_irq_base) {
  avr_connect_irq(p->irq + TWI_IRQ_INPUT,
                  avr_io_getirq(avr, i2c_irq_base, TWI_IRQ_INPUT));
  avr_connect_irq(avr_io_getirq(avr, i2c_irq_base, TWI_IRQ_OUTPUT),
                  p->irq + TWI_IRQ_OUTPUT);
}
//---------------------------------------

avr_t *avr = NULL;

static void twi_startTransmission( avr_t *avr, i2c_master *m, struct nmsg *msgs ) {
  m->current_msg = 0;
  m->index = 0;
  m->msgs = msgs;

  if( !msg_hasNext( m ) ) {
    return;
  }

  m->selected = msg_current(m)->address;
  if( msg_current(m)->mode == TWI_WRITE ) {
    // avr_irq_register_notify( m->irq + TWI_IRQ_OUTPUT, i2c_master_send_hook, m );
  }

  printf( "Send start to 0x%02X\n", m->selected );
  avr_raise_irq(
      m->irq + TWI_IRQ_INPUT,
      avr_twi_irq_msg(TWI_COND_START | TWI_COND_ADDR | TWI_COND_WRITE,
                      m->selected, 1));
}

avr_cycle_count_t twi_sendSomething(struct avr_t * avr,	avr_cycle_count_t when,	void * param) {
  static uint8_t adr_buf = 0x10;
  static char str_buf[] = "Hello AVR!";
  struct msg *msg = malloc( sizeof( struct msg ) * 2 );
  struct nmsg *nmsg = malloc( sizeof( struct nmsg ) );
  if(!msg || !nmsg)
    exit(1);

  msg[0].address = 0x42;
  msg[0].mode = TWI_WRITE;
  msg[0].buffer = ( char * ) &adr_buf;
  msg[0].size = sizeof( uint8_t );

  msg[1].address = 0x42;
  msg[1].mode = TWI_WRITE;
  msg[1].buffer = &str_buf[0];
  msg[1].size = sizeof( str_buf );

  nmsg->msg = msg;
  nmsg->nmsg = 2;

  twi_startTransmission( avr, param, nmsg );

  return 0;
}

avr_cycle_count_t twi_receiveSomething(struct avr_t * avr,	avr_cycle_count_t when,	void * param) {
  static uint8_t adr_buf = 0x16;
  static char str_buf[3];
  struct msg *msg = malloc( sizeof( struct msg ) * 2 );
  struct nmsg *nmsg = malloc( sizeof( struct nmsg ) );
  if(!msg || !nmsg)
    exit(1);

  msg[0].address = 0x42;
  msg[0].mode = TWI_WRITE;
  msg[0].buffer = ( char * ) &adr_buf;
  msg[0].size = sizeof( uint8_t );

  msg[1].address = 0x42;
  msg[1].mode = TWI_READ;
  msg[1].buffer = &str_buf[0];
  msg[1].size = sizeof( str_buf );

  nmsg->msg = msg;
  nmsg->nmsg = 2;

  twi_startTransmission( avr, param, nmsg );

  return 0;
}

int main(int argc, char *argv[]) {
  elf_firmware_t f = {};
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
  // avr->log = 4;
  if (0) {
    avr->state = cpu_Stopped;
    avr_gdb_init(avr);
  }

  i2c_master ma;
  i2c_master_init(avr, &ma);
  i2c_master_attach(avr, &ma, AVR_IOCTL_TWI_GETIRQ(0));

  avr_irq_register_notify( ma.irq + TWI_IRQ_OUTPUT, i2c_master_send_hook, &ma );

  avr_cycle_timer_register_usec( avr, 500, twi_sendSomething, &ma );
  avr_cycle_timer_register_usec( avr, 2500, twi_receiveSomething, &ma );

  printf("\nDemo launching:\n");

  int state = cpu_Running;
  while ((state != cpu_Done) && (state != cpu_Crashed)) {
    state = avr_run(avr);
  }
}
