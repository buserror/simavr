#ifndef _TWI_MASTER_H_
#define _TWI_MASTER_H_
#include <stdint.h>
#include <stdlib.h>

#include "avr_twi.h"
#include "sim_avr.h"

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
    long unsigned int index;
    int verbose;
} i2c_master;

extern void i2c_master_init( avr_t *, i2c_master * );

extern void i2c_master_attach( avr_t *, i2c_master *, uint32_t );

extern void twi_startTransmission( avr_t *, i2c_master *, struct nmsg * );

#endif
