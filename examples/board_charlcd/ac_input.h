#ifndef __AC_INPUT_H__
#define __AC_INPUT_H__

#include "sim_irq.h"

/*
 * Simulates a 50hz changing signal
 */

enum {
    IRQ_AC_OUT = 0,
    IRQ_AC_COUNT
};

typedef struct ac_input_t {
    avr_irq_t * irq;
    struct avr_t * avr;
    uint8_t value;
} ac_input_t;

void ac_input_init(struct avr_t * avr, ac_input_t * b);

#endif
