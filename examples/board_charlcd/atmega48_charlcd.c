#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "stdlib.h"
#include "string.h"

#include "atmega48_lcd.h"

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega48");

static uint8_t subsecct = 0;
static uint8_t hour = 0;
static uint8_t minute = 0;
static uint8_t second = 0;
static volatile uint8_t update_needed = 0;

ISR( INT0_vect ){
    /* External interrupt on pin D2 */
    subsecct++;
    if( subsecct == 50 ){
        second++;
        update_needed = 1;
        if(second ==60){
            minute++;
            second = 0;
            if(minute==60){
                minute =0;
                hour++;
                if(hour==24) hour =0;
            }
        }
    }
}

int main(){
    lcd_init();
   

    EICRA = (1<<ISC00);
    EIMSK = (1<<INT0);
 
    sei();

    while(1)
    {
          while(!update_needed);
          update_needed = 0;
          char buffer[16];
          lcd_clear();
          set_cursor(4,1);
          itoa(hour,buffer,10);
          lcd_string(buffer);
          lcd_data(':');
          itoa(minute,buffer,10);
          lcd_string(buffer);
          lcd_data(':');
          itoa(second,buffer,10);
          lcd_string(buffer);
    }

}
