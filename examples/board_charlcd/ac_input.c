
#include "sim_avr.h"
#include "ac_input.h"
#include "stdio.h"

static avr_cycle_count_t switch_auto( struct avr_t * avr, avr_cycle_count_t when, void * param){
    ac_input_t * b = (ac_input_t *)param;
    b->value = b->value == 0 ? 1 : 0;
    avr_raise_irq( b->irq + IRQ_AC_OUT, b->value );
    return when + avr_usec_to_cycles(avr,10000);
}

void ac_input_init(struct avr_t *avr, ac_input_t *b){
    b->irq = avr_alloc_irq(0,IRQ_AC_COUNT);
    b->avr = avr;
    b->value = 0;
    avr_cycle_timer_register_usec(avr, 10000, switch_auto, b);
}
