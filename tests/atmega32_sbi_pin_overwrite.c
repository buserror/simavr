/*
	atmega32_sbi_pin_overwrite.c

 */

#ifndef F_CPU
#define F_CPU 8000000
#endif
#include <avr/io.h>
#include <stdio.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

#define __ASSERT_USE_STDERR 1
#include <assert.h>

#include "avr/iom32.h"

#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega32");

/*
 * This small section tells simavr to generate a VCD trace dump with changes to these
 * registers.
 * Opening it with gtkwave will show you the data being pumped out into the data register
 * UDR0, and the UDRE0 bit being set, then cleared
 */
const struct avr_mmcu_vcd_trace_t _mytrace[]  _MMCU_ = {
	{ AVR_MCU_VCD_SYMBOL("PORTD.6"), .what = (void*)&PORTD, .mask= ( 1 << 6) },	
	{ AVR_MCU_VCD_SYMBOL("DDRD.6"), .what = (void*)&DDRD, .mask= ( 1 << 6) },	
	{ AVR_MCU_VCD_SYMBOL("PIND.6"), .what = (void*)&PIND, .mask= ( 1 << 6) },	
	{ AVR_MCU_VCD_SYMBOL("PORTA.0"), .what = (void*)&PORTA, .mask= ( 1 << 0) },	
	{ AVR_MCU_VCD_SYMBOL("PORTB.0"), .what = (void*)&PORTB, .mask= ( 1 << 0) },	
};


static int uart_putchar(char c, FILE *stream) {
	if (c == '\n')
		uart_putchar('\r', stream);
	loop_until_bit_is_set(UCSRA, UDRE);
	UDR = c;
	return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);

#define BYTETOBINARYPATTERN "%d%d%d%d%d%d%d%d"
#define BYTETOBINARY(byte)  \
  (byte & 0x80 ? 1 : 0), \
    (byte & 0x40 ? 1 : 0), \
    (byte & 0x20 ? 1 : 0), \
    (byte & 0x10 ? 1 : 0), \
    (byte & 0x08 ? 1 : 0), \
    (byte & 0x04 ? 1 : 0), \
    (byte & 0x02 ? 1 : 0), \
    (byte & 0x01 ? 1 : 0) 

#define GET_PIND6 (PIND & _BV(6) ? 1 : 0)

static char check_count = 1;

void
check_pind6(char val) 
{
  if (GET_PIND6 == val) {
    printf("OK%d, ", check_count++);
  }
  else {
    printf("\nFAIL %d\n", check_count++);
    printf("PIND.6 %d != %d\n", GET_PIND6, val);
    printf("PIND: " BYTETOBINARYPATTERN "\n", BYTETOBINARY( PIND));
  }
}

int main()
{

	stdout = &mystdout;

  // set pullup on PORTD pin 6
  asm volatile ("sbi %0, 6": : "I" (_SFR_IO_ADDR(PORTD)) );
  // pullup works, pind.6 is 1
  check_pind6(1);  // ok1

  // setting pind.6 to 0 will not work
  asm volatile ("cbi %0, 6": : "I" (_SFR_IO_ADDR(PIND)) );
  // now pind.6 is still 1
  check_pind6(1); // ok2

  // setting portd.6 to 0 will work
  asm volatile ("cbi %0, 6": : "I" (_SFR_IO_ADDR(PORTD)) );
  // now pind.6 is 0 
  check_pind6(0); // ok3


  // read portd and write it back again
  asm volatile ("in R24, %0" : : "I" (_SFR_IO_ADDR(PORTD)) );
  asm volatile ("out %0, R24" : : "I" (_SFR_IO_ADDR(PORTD)) );
  // pind.6 should still be 0
  check_pind6(0); // ok4

  // set pullup on PORTD pin 6
  asm volatile ("sbi %0, 6": : "I" (_SFR_IO_ADDR(PORTD)) );
  // pullup works, pind.6 is 1
  check_pind6(1); // ok5

  // in test, there is an irq handler connected to PORTA.0 that sets pind.6 to 0
  asm volatile ("sbi %0, 0": : "I" (_SFR_IO_ADDR(PORTA)) );
  // now pind.6 is 0 
  check_pind6(0); // ok6

  // read portd and write it back again
  asm volatile ("in R24, %0" : : "I" (_SFR_IO_ADDR(PORTD)) );
  asm volatile ("out %0, R24" : : "I" (_SFR_IO_ADDR(PORTD)) );

  // pind.6 should still be 0
  check_pind6(0); // ok7



  // set pullup on PORTD pin 6
  asm volatile ("sbi %0, 6": : "I" (_SFR_IO_ADDR(PORTD)) );
  // pullup does not work anymore, pind.6 is 0
  check_pind6(0); // ok8

  // in test, there is an irq handler connected to PORTB.0 that sets portd to 01000000
  asm volatile ("sbi %0, 0": : "I" (_SFR_IO_ADDR(PORTB)) );
  // now pind.6 is 1 
  check_pind6(1); // ok9

  // disable pullup on PORTD pin 6
  asm volatile ("cbi %0, 6": : "I" (_SFR_IO_ADDR(PORTD)) );
  // pin values is still fixed, pind.6 is 1
  check_pind6(1); // ok10

  // read portd and write it back again
  asm volatile ("in R24, %0" : : "I" (_SFR_IO_ADDR(PORTD)) );
  asm volatile ("out %0, R24" : : "I" (_SFR_IO_ADDR(PORTD)) );

  // pind.6 should still be 1
  check_pind6(1); // ok11


	sleep_cpu();
}
