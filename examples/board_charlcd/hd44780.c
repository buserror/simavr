#include "hd44780.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

void debug_print( const char* format, ... ) {

    va_list args;
    va_start( args, format );
    #ifdef HD44780_DEBUG
    vprintf( format, args );
    #endif
    va_end( args );
}

int check_flag( hd44780_t *b, int flag){
    return (b->flags>>flag)&1;
}

void reset_cursor( hd44780_t *b){
    b->cursor = b->ddram;
}

void clear_screen( hd44780_t *b){
    memset(b->ddram,' ',80);
}

void handle_function_8bit( hd44780_t *b, int realportstate ){
    debug_print("handle_function_8bit  realportstate 0x%x\n",realportstate);
    /*
    if( realportstate == 0x22){
        debug_print("Activating 4Bit Mode\n");
        b->mode_4bit = 1;
    }
    */
    if( realportstate == 1) {
        clear_screen(b);
        debug_print("Clear Screen\n");
    }
    if( realportstate >> 1 == 1) {
        debug_print("Move Cursor to startpos\n");
        reset_cursor(b);
    }
    if( realportstate >> 2 == 1) {
        debug_print("Setting I/S mode\n");
        b->flags &= ~(3<<HD44780_FLAG_S); //Clear S, I/D bits
        b->flags |= (realportstate & 3)<<HD44780_FLAG_S;
    }
    if( realportstate >> 3 == 1) {
        debug_print("Setting Display/Cursor Mode\n");
        b->flags &= ~(7<<HD44780_FLAG_B); //Clear B,C,D bits
        b->flags |= realportstate&7 << HD44780_FLAG_B; // Set B,C,D bits
    }
    if( realportstate >> 4 == 1) {
        debug_print("Moving Display/Cursor Mode\n");
        b->flags &= ~(3<<HD44780_FLAG_R_L); //Clear R/L,S/C
        b->flags |= (realportstate & 3)<<HD44780_FLAG_R_L;
    }
    if( realportstate >> 5 == 1) {
        debug_print("Functions\n");
        b->flags &= ~(7<<HD44780_FLAG_F); //Clear F,N,DL bits
        b->flags |= (realportstate>>2)&7 << HD44780_FLAG_F; // Set F,N,DL bits
        if( !check_flag(b, HD44780_FLAG_D_L) ){
            debug_print("Enabling 4Bit Mode\n");
            b->mode_4bit = 1;  
        }
    }
    if( realportstate >> 6 == 1) printf("Set CGRAM Address\n");
    if( realportstate >> 7 == 1) {
        debug_print("Set DDRAM Address\n");
        b->cursor = b->ddram + (realportstate & 0x7f);
    }
    debug_print("Flags are 0x%x\n",b->flags);
}

void handle_data_8bit( hd44780_t *b, int realportstate ){
    /* printf("handle_data_8bit realportstate 0x%x\n",realportstate); */
    *(b->cursor) = realportstate & 0xff;
    if( !check_flag(b, HD44780_FLAG_S_C) ){
        // Move Cursor
        if( check_flag(b, HD44780_FLAG_I_D) ){
            b->cursor++;
        }
        else{
            b->cursor--;
        }
    }
    else {
        printf("!! NOT IMPLEMENTED\n");
        // Move Display
        if( check_flag(b, HD44780_FLAG_R_L) ){
            //Move right
            
        }
        else {
            // Move left
        }
    }
    if( b->cursor < b->ddram || b->cursor > b->ddram+80){
        debug_print("Cursor out of range!\n");
        reset_cursor(b);
    }
    #ifdef HD44780_DEBUG
    hd44780_print2x16( b );
    #endif 
}

void hd44780_init( struct avr_t *avr, struct hd44780_t * b) {
    b->irq = avr_alloc_irq(0,IRQ_HD44780_COUNT);
    b->portstate = 0x00;
    b->mode_4bit = 0;
    b->four_bit_cache = 0;
    b->inits_recv = 0;
    b->flags = 0;
    reset_cursor(b);
    clear_screen(b);
}

void hd44780_print2x16( struct hd44780_t *b){
    printf("/******************\\\n| ");
    fwrite( b->ddram, 1, 16, stdout);
    printf(" |\n| ");
//    fputc('\n',stdout);
    fwrite( b->ddram + 0x40, 1, 16, stdout);
    printf(" |\n\\******************/\n");
}

void hd44780_data_changed_hook( struct avr_irq_t * irq, uint32_t value, void *param){
    hd44780_t *b = (hd44780_t *)param;
    int datapos = -1;
    if( irq->irq == DATA_PIN_0) datapos = 0;
    if( irq->irq == DATA_PIN_1) datapos = 1;
    if( irq->irq == DATA_PIN_2) datapos = 2;
    if( irq->irq == DATA_PIN_3) datapos = 3;
    if( irq->irq == DATA_PIN_4) datapos = 4;
    if( irq->irq == DATA_PIN_5) datapos = 5;
    if( irq->irq == DATA_PIN_6) datapos = 6;
    if( irq->irq == DATA_PIN_7) datapos = 7;
    if( datapos >= 0){
        if( value == 1) b->portstate |= 1<<datapos;
        else b->portstate &= ~(1<<datapos);
    }
}


void hd44780_cmd_changed_hook( struct avr_irq_t * irq, uint32_t value, void *param){
    hd44780_t *b = (hd44780_t *)param;
    if( irq->irq == RS_PIN){
        b->rs=value;
    }
    if( irq->irq == RW_PIN){
        b->rw=value;
    }
    if( irq->irq == ENABLE_PIN && value == 0){
        debug_print("enable pulse! portstate 0x%x\n", b->portstate);
        if( b->inits_recv < 3 ){
            if( b->portstate == 0x30 ){
                debug_print("Init received\n");
                b->inits_recv++;
            }
            else debug_print("Uuups, received command before fully initialized?\n");
        }
        else {
            int realportstate=0;
            int received = 1;
            if( b->mode_4bit == 1) b->four_bit_cache = b->portstate, b->mode_4bit++,received=0;
            else if( b->mode_4bit == 2) {
                realportstate = (b->four_bit_cache&0xf0) | (b->portstate&0xf0)>>4;
                b->mode_4bit=1;
            }
            else realportstate = b->portstate;
            debug_print("four bit cache is 0x%x\n",b->four_bit_cache);

            if(received){
                if( !b->rs ) handle_function_8bit( b, realportstate );
                else handle_data_8bit( b, realportstate );
            }
        }
    }
//    printf("pin %i is now %i, portstate 0x%x\n", irq->irq, value, b->portstate);
}



