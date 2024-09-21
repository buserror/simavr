#ifndef __AYAB_SHIELD__
#define __AYAB_SHIELD__

#include "i2c_mcp23008_virt.h"

#define PIN_V1 2
#define PIN_V2 3
#define PIN_BP 4
#define PIN_LED_A 5
#define PIN_LED_B 6

#define PIN_HALL_RIGHT ADC_IRQ_ADC0
#define PIN_HALL_LEFT  ADC_IRQ_ADC1

typedef struct {
    int led[2];
    i2c_mcp23008_t mcp23008[2];
} shield_t;

#endif
