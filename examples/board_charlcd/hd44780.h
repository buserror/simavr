#ifndef __HD44780_H__
#define __HD44780_H__

#include "sim_irq.h"

/******************
 * Simulates a HD44780 controlled character LED.
 * All the Data Pins have to be on the same Port.
 * All the Cmd Pins have to be on the same Port. In 4 Bit mode,
 * they can share one Port.
 * For output of the display contents, use hd44780_print2x16 or a opengl function.
 ******************/

#define ENABLE_PIN 5
#define RW_PIN 6
#define RS_PIN 4
/* Use -1 for not connected */
#define DATA_PIN_0 -1
#define DATA_PIN_1 -1
#define DATA_PIN_2 -1
#define DATA_PIN_3 -1
#define DATA_PIN_4 0
#define DATA_PIN_5 1
#define DATA_PIN_6 2
#define DATA_PIN_7 3

/* #define HD44780_DEBUG */

enum {
    HD44780_FLAG_F = 0,         //5x7 Font, 5x10 Font
    HD44780_FLAG_N,             //1-zeiliges Display, 2/4-zeiliges Display
    HD44780_FLAG_D_L,           //4-Bit Interface, 8-Bit Interface
    HD44780_FLAG_R_L,           // Nach links schieben,Nach rechts schieben
    HD44780_FLAG_S_C,           //Cursor bewegen, Displayinhalt schieben
    HD44780_FLAG_B,             //Cursor blinkt nicht, Cursor blinkt
    HD44780_FLAG_C,             //Cursor aus,Cursor an
    HD44780_FLAG_D,             //Display aus,Display an
    HD44780_FLAG_S,             //Displayinhalt fest, Displayinhalt weiterschieben
    HD44780_FLAG_I_D 
};

enum {
    IRQ_HD44780_IN = 0,
    IRQ_HD44780_COUNT
};


typedef struct hd44780_t {
    avr_irq_t * irq;
    struct avr_t * avr;
    char *cursor;
    char ddram[80];
    char cgram[64];
    uint32_t rw;            /* R/W pin */
    uint32_t rs;            /* RS pin */
    int portstate;
    char mode_4bit;     /* 0=disabled, 1=waitingforfirstnibble, 2=waitingforsecondnibble */
    uint32_t flags;
    int four_bit_cache;
    char inits_recv;    /* num of init sequences received */
} hd44780_t;

void hd44780_init( struct avr_t *avr, struct hd44780_t * b);
void hd44780_print2x16( struct hd44780_t *b);

void hd44780_data_changed_hook( struct avr_irq_t * irq, uint32_t value, void *param);
void hd44780_cmd_changed_hook( struct avr_irq_t * irq, uint32_t value, void *param);

#endif 
