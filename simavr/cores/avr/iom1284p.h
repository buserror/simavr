/* Copyright (c) 2007 Atmel Corporation
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   * Neither the name of the copyright holders nor the names of
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE. 
*/

/* $Id: iom1284p.h 2460 2014-12-03 05:39:25Z pitchumani $ */

/* avr/iom1284p.h - definitions for ATmega1284P. */

/* This file should only be included from <avr/io.h>, never directly. */

#ifndef _AVR_IO_H_
#  error "Include <avr/io.h> instead of this file."
#endif

#ifndef _AVR_IOXXX_H_
#  define _AVR_IOXXX_H_ "iom1284p.h"
#else
#  error "Attempt to include more than one <avr/ioXXX.h> file."
#endif 


#ifndef _AVR_IOM1284P_H_
#define _AVR_IOM1284P_H_ 1


/* Registers and associated bit numbers */

#define PINA _SFR_IO8(0x00)
#define PINA0 0
#define PINA1 1
#define PINA2 2
#define PINA3 3
#define PINA4 4
#define PINA5 5
#define PINA6 6
#define PINA7 7

#define DDRA _SFR_IO8(0x01)
#define DDA0 0
#define DDA1 1
#define DDA2 2
#define DDA3 3
#define DDA4 4
#define DDA5 5
#define DDA6 6
#define DDA7 7

#define PORTA _SFR_IO8(0x02)
#define PORTA0 0
#define PORTA1 1
#define PORTA2 2
#define PORTA3 3
#define PORTA4 4
#define PORTA5 5
#define PORTA6 6
#define PORTA7 7

#define PINB _SFR_IO8(0x03)
#define PINB0 0
#define PINB1 1
#define PINB2 2
#define PINB3 3
#define PINB4 4
#define PINB5 5
#define PINB6 6
#define PINB7 7

#define DDRB _SFR_IO8(0x04)
#define DDB0 0
#define DDB1 1
#define DDB2 2
#define DDB3 3
#define DDB4 4
#define DDB5 5
#define DDB6 6
#define DDB7 7

#define PORTB _SFR_IO8(0x05)
#define PORTB0 0
#define PORTB1 1
#define PORTB2 2
#define PORTB3 3
#define PORTB4 4
#define PORTB5 5
#define PORTB6 6
#define PORTB7 7

#define PINC _SFR_IO8(0x06)
#define PINC0 0
#define PINC1 1
#define PINC2 2
#define PINC3 3
#define PINC4 4
#define PINC5 5
#define PINC6 6
#define PINC7 7

#define DDRC _SFR_IO8(0x07)
#define DDC0 0
#define DDC1 1
#define DDC2 2
#define DDC3 3
#define DDC4 4
#define DDC5 5
#define DDC6 6
#define DDC7 7

#define PORTC _SFR_IO8(0x08)
#define PORTC0 0
#define PORTC1 1
#define PORTC2 2
#define PORTC3 3
#define PORTC4 4
#define PORTC5 5
#define PORTC6 6
#define PORTC7 7

#define PIND _SFR_IO8(0x09)
#define PIND0 0
#define PIND1 1
#define PIND2 2
#define PIND3 3
#define PIND4 4
#define PIND5 5
#define PIND6 6
#define PIND7 7

#define DDRD _SFR_IO8(0x0A)
#define DDD0 0
#define DDD1 1
#define DDD2 2
#define DDD3 3
#define DDD4 4
#define DDD5 5
#define DDD6 6
#define DDD7 7

#define PORTD _SFR_IO8(0x0B)
#define PORTD0 0
#define PORTD1 1
#define PORTD2 2
#define PORTD3 3
#define PORTD4 4
#define PORTD5 5
#define PORTD6 6
#define PORTD7 7

#define TIFR0 _SFR_IO8(0x15)
#define TOV0 0
#define OCF0A 1
#define OCF0B 2

#define TIFR1 _SFR_IO8(0x16)
#define TOV1 0
#define OCF1A 1
#define OCF1B 2
#define ICF1 5

#define TIFR2 _SFR_IO8(0x17)
#define TOV2 0
#define OCF2A 1
#define OCF2B 2

#define TIFR3 _SFR_IO8(0x18)
#define TOV3 0
#define OCF3A 1
#define OCF3B 2
#define ICF3 5

#define PCIFR _SFR_IO8(0x1B)
#define PCIF0 0
#define PCIF1 1
#define PCIF2 2
#define PCIF3 3

#define EIFR _SFR_IO8(0x1C)
#define INTF0 0
#define INTF1 1
#define INTF2 2

#define EIMSK _SFR_IO8(0x1D)
#define INT0 0
#define INT1 1
#define INT2 2

#define GPIOR0 _SFR_IO8(0x1E)
#define GPIOR00 0
#define GPIOR01 1
#define GPIOR02 2
#define GPIOR03 3
#define GPIOR04 4
#define GPIOR05 5
#define GPIOR06 6
#define GPIOR07 7

#define EECR _SFR_IO8(0x1F)
#define EERE 0
#define EEPE 1
#define EEMPE 2
#define EERIE 3
#define EEPM0 4
#define EEPM1 5

#define EEDR _SFR_IO8(0x20)
#define EEDR0 0
#define EEDR1 1
#define EEDR2 2
#define EEDR3 3
#define EEDR4 4
#define EEDR5 5
#define EEDR6 6
#define EEDR7 7

#define EEAR _SFR_IO16(0x21)

#define EEARL _SFR_IO8(0x21)
#define EEAR0 0
#define EEAR1 1
#define EEAR2 2
#define EEAR3 3
#define EEAR4 4
#define EEAR5 5
#define EEAR6 6
#define EEAR7 7

#define EEARH _SFR_IO8(0x22)
#define EEAR8 0
#define EEAR9 1
#define EEAR10 2
#define EEAR11 3

#define GTCCR _SFR_IO8(0x23)
#define PSRSYNC 0
#define PSRASY 1
#define TSM 7

#define TCCR0A _SFR_IO8(0x24)
#define WGM00 0
#define WGM01 1
#define COM0B0 4
#define COM0B1 5
#define COM0A0 6
#define COM0A1 7

#define TCCR0B _SFR_IO8(0x25)
#define CS00 0
#define CS01 1
#define CS02 2
#define WGM02 3
#define FOC0B 6
#define FOC0A 7

#define TCNT0 _SFR_IO8(0x26)
#define TCNT0_0 0
#define TCNT0_1 1
#define TCNT0_2 2
#define TCNT0_3 3
#define TCNT0_4 4
#define TCNT0_5 5
#define TCNT0_6 6
#define TCNT0_7 7

#define OCR0A _SFR_IO8(0x27)
#define OCR0A_0 0
#define OCR0A_1 1
#define OCR0A_2 2
#define OCR0A_3 3
#define OCR0A_4 4
#define OCR0A_5 5
#define OCR0A_6 6
#define OCR0A_7 7

#define OCR0B _SFR_IO8(0x28)
#define OCR0B_0 0
#define OCR0B_1 1
#define OCR0B_2 2
#define OCR0B_3 3
#define OCR0B_4 4
#define OCR0B_5 5
#define OCR0B_6 6
#define OCR0B_7 7

#define GPIOR1 _SFR_IO8(0x2A)
#define GPIOR10 0
#define GPIOR11 1
#define GPIOR12 2
#define GPIOR13 3
#define GPIOR14 4
#define GPIOR15 5
#define GPIOR16 6
#define GPIOR17 7

#define GPIOR2 _SFR_IO8(0x2B)
#define GPIOR20 0
#define GPIOR21 1
#define GPIOR22 2
#define GPIOR23 3
#define GPIOR24 4
#define GPIOR25 5
#define GPIOR26 6
#define GPIOR27 7

#define SPCR _SFR_IO8(0x2C)
#define SPR0 0
#define SPR1 1
#define CPHA 2
#define CPOL 3
#define MSTR 4
#define DORD 5
#define SPE 6
#define SPIE 7

#define SPSR _SFR_IO8(0x2D)
#define SPI2X 0
#define WCOL 6
#define SPIF 7

#define SPDR _SFR_IO8(0x2E)
#define SPDR0 0
#define SPDR1 1
#define SPDR2 2
#define SPDR3 3
#define SPDR4 4
#define SPDR5 5
#define SPDR6 6
#define SPDR7 7

#define ACSR _SFR_IO8(0x30)
#define ACIS0 0
#define ACIS1 1
#define ACIC 2
#define ACIE 3
#define ACI 4
#define ACO 5
#define ACBG 6
#define ACD 7

#define OCDR _SFR_IO8(0x31)
#define OCDR0 0
#define OCDR1 1
#define OCDR2 2
#define OCDR3 3
#define OCDR4 4
#define OCDR5 5
#define OCDR6 6
#define OCDR7 7

#define SMCR _SFR_IO8(0x33)
#define SE 0
#define SM0 1
#define SM1 2
#define SM2 3

#define MCUSR _SFR_IO8(0x34)
#define PORF 0
#define EXTRF 1
#define BORF 2
#define WDRF 3
#define JTRF 4

#define MCUCR _SFR_IO8(0x35)
#define IVCE 0
#define IVSEL 1
#define PUD 4
#define BODSE 5
#define BODS 6
#define JTD 7

#define SPMCSR _SFR_IO8(0x37)
#define SPMEN 0
#define PGERS 1
#define PGWRT 2
#define BLBSET 3
#define RWWSRE 4
#define SIGRD 5
#define RWWSB 6
#define SPMIE 7

#define RAMPZ _SFR_IO8(0x3B)
#define RAMPZ0 0

#define WDTCSR _SFR_MEM8(0x60)
#define WDP0 0
#define WDP1 1
#define WDP2 2
#define WDE 3
#define WDCE 4
#define WDP3 5
#define WDIE 6
#define WDIF 7

#define CLKPR _SFR_MEM8(0x61)
#define CLKPS0 0
#define CLKPS1 1
#define CLKPS2 2
#define CLKPS3 3
#define CLKPCE 7

#define PRR0 _SFR_MEM8(0x64)
#define PRADC 0
#define PRUSART0 1
#define PRSPI 2
#define PRTIM1 3
#define PRUSART1 4
#define PRTIM0 5
#define PRTIM2 6
#define PRTWI 7

#define __AVR_HAVE_PRR0	((1<<PRADC)|(1<<PRSPI)|(1<<PRTIM1)|(1<<PRUSART0)|(1<<PRUSART1)|(1<<PRTIM0)|(1<<PRTIM2)|(1<<PRTWI))
#define __AVR_HAVE_PRR0_PRADC
#define __AVR_HAVE_PRR0_PRSPI
#define __AVR_HAVE_PRR0_PRTIM1
#define __AVR_HAVE_PRR0_PRUSART0
#define __AVR_HAVE_PRR0_PRUSART1
#define __AVR_HAVE_PRR0_PRTIM0
#define __AVR_HAVE_PRR0_PRTIM2
#define __AVR_HAVE_PRR0_PRTWI

#define PRR1 _SFR_MEM8(0x65)
#define PRTIM3 0

#define __AVR_HAVE_PRR1	(1<<PRTIM3)
#define __AVR_HAVE_PRR1_PRTIM3

#define OSCCAL _SFR_MEM8(0x66)
#define CAL0 0
#define CAL1 1
#define CAL2 2
#define CAL3 3
#define CAL4 4
#define CAL5 5
#define CAL6 6
#define CAL7 7

#define PCICR _SFR_MEM8(0x68)
#define PCIE0 0
#define PCIE1 1
#define PCIE2 2
#define PCIE3 3

#define EICRA _SFR_MEM8(0x69)
#define ISC00 0
#define ISC01 1
#define ISC10 2
#define ISC11 3
#define ISC20 4
#define ISC21 5

#define PCMSK0 _SFR_MEM8(0x6B)
#define PCINT0 0
#define PCINT1 1
#define PCINT2 2
#define PCINT3 3
#define PCINT4 4
#define PCINT5 5
#define PCINT6 6
#define PCINT7 7

#define PCMSK1 _SFR_MEM8(0x6C)
#define PCINT8 0
#define PCINT9 1
#define PCINT10 2
#define PCINT11 3
#define PCINT12 4
#define PCINT13 5
#define PCINT14 6
#define PCINT15 7

#define PCMSK2 _SFR_MEM8(0x6D)
#define PCINT16 0
#define PCINT17 1
#define PCINT18 2
#define PCINT19 3
#define PCINT20 4
#define PCINT21 5
#define PCINT22 6
#define PCINT23 7

#define TIMSK0 _SFR_MEM8(0x6E)
#define TOIE0 0
#define OCIE0A 1
#define OCIE0B 2

#define TIMSK1 _SFR_MEM8(0x6F)
#define TOIE1 0
#define OCIE1A 1
#define OCIE1B 2
#define ICIE1 5

#define TIMSK2 _SFR_MEM8(0x70)
#define TOIE2 0
#define OCIE2A 1
#define OCIE2B 2

#define TIMSK3 _SFR_MEM8(0x71)
#define TOIE3 0
#define OCIE3A 1
#define OCIE3B 2
#define ICIE3 5

#define PCMSK3 _SFR_MEM8(0x73)
#define PCINT24 0
#define PCINT25 1
#define PCINT26 2
#define PCINT27 3
#define PCINT28 4
#define PCINT29 5
#define PCINT30 6
#define PCINT31 7

#ifndef __ASSEMBLER__
#define ADC     _SFR_MEM16(0x78)
#endif
#define ADCW    _SFR_MEM16(0x78)

#define ADCL _SFR_MEM8(0x78)
#define ADCL0 0
#define ADCL1 1
#define ADCL2 2
#define ADCL3 3
#define ADCL4 4
#define ADCL5 5
#define ADCL6 6
#define ADCL7 7

#define ADCH _SFR_MEM8(0x79)
#define ADCH0 0
#define ADCH1 1
#define ADCH2 2
#define ADCH3 3
#define ADCH4 4
#define ADCH5 5
#define ADCH6 6
#define ADCH7 7

#define ADCSRA _SFR_MEM8(0x7A)
#define ADPS0 0
#define ADPS1 1
#define ADPS2 2
#define ADIE 3
#define ADIF 4
#define ADATE 5
#define ADSC 6
#define ADEN 7

#define ADCSRB _SFR_MEM8(0x7B)
#define ADTS0 0
#define ADTS1 1
#define ADTS2 2
#define ACME 6

#define ADMUX _SFR_MEM8(0x7C)
#define MUX0 0
#define MUX1 1
#define MUX2 2
#define MUX3 3
#define MUX4 4
#define ADLAR 5
#define REFS0 6
#define REFS1 7

#define DIDR0 _SFR_MEM8(0x7E)
#define ADC0D 0
#define ADC1D 1
#define ADC2D 2
#define ADC3D 3
#define ADC4D 4
#define ADC5D 5
#define ADC6D 6
#define ADC7D 7

#define DIDR1 _SFR_MEM8(0x7F)
#define AIN0D 0
#define AIN1D 1

#define TCCR1A _SFR_MEM8(0x80)
#define WGM10 0
#define WGM11 1
#define COM1B0 4
#define COM1B1 5
#define COM1A0 6
#define COM1A1 7

#define TCCR1B _SFR_MEM8(0x81)
#define CS10 0
#define CS11 1
#define CS12 2
#define WGM12 3
#define WGM13 4
#define ICES1 6
#define ICNC1 7

#define TCCR1C _SFR_MEM8(0x82)
#define FOC1B 6
#define FOC1A 7

#define TCNT1 _SFR_MEM16(0x84)

#define TCNT1L _SFR_MEM8(0x84)
#define TCNT1L0 0
#define TCNT1L1 1
#define TCNT1L2 2
#define TCNT1L3 3
#define TCNT1L4 4
#define TCNT1L5 5
#define TCNT1L6 6
#define TCNT1L7 7

#define TCNT1H _SFR_MEM8(0x85)
#define TCNT1H0 0
#define TCNT1H1 1
#define TCNT1H2 2
#define TCNT1H3 3
#define TCNT1H4 4
#define TCNT1H5 5
#define TCNT1H6 6
#define TCNT1H7 7

#define ICR1 _SFR_MEM16(0x86)

#define ICR1L _SFR_MEM8(0x86)
#define ICR1L0 0
#define ICR1L1 1
#define ICR1L2 2
#define ICR1L3 3
#define ICR1L4 4
#define ICR1L5 5
#define ICR1L6 6
#define ICR1L7 7

#define ICR1H _SFR_MEM8(0x87)
#define ICR1H0 0
#define ICR1H1 1
#define ICR1H2 2
#define ICR1H3 3
#define ICR1H4 4
#define ICR1H5 5
#define ICR1H6 6
#define ICR1H7 7

#define OCR1A _SFR_MEM16(0x88)

#define OCR1AL _SFR_MEM8(0x88)
#define OCR1AL0 0
#define OCR1AL1 1
#define OCR1AL2 2
#define OCR1AL3 3
#define OCR1AL4 4
#define OCR1AL5 5
#define OCR1AL6 6
#define OCR1AL7 7

#define OCR1AH _SFR_MEM8(0x89)
#define OCR1AH0 0
#define OCR1AH1 1
#define OCR1AH2 2
#define OCR1AH3 3
#define OCR1AH4 4
#define OCR1AH5 5
#define OCR1AH6 6
#define OCR1AH7 7

#define OCR1B _SFR_MEM16(0x8A)

#define OCR1BL _SFR_MEM8(0x8A)
#define OCR1AL0 0
#define OCR1AL1 1
#define OCR1AL2 2
#define OCR1AL3 3
#define OCR1AL4 4
#define OCR1AL5 5
#define OCR1AL6 6
#define OCR1AL7 7

#define OCR1BH _SFR_MEM8(0x8B)
#define OCR1AH0 0
#define OCR1AH1 1
#define OCR1AH2 2
#define OCR1AH3 3
#define OCR1AH4 4
#define OCR1AH5 5
#define OCR1AH6 6
#define OCR1AH7 7

#define TCCR3A _SFR_MEM8(0x90)
#define WGM30 0
#define WGM31 1
#define COM3B0 4
#define COM3B1 5
#define COM3A0 6
#define COM3A1 7

#define TCCR3B _SFR_MEM8(0x91)
#define CS30 0
#define CS31 1
#define CS32 2
#define WGM32 3
#define WGM33 4
#define ICES3 6
#define ICNC3 7

#define TCCR3C _SFR_MEM8(0x92)
#define FOC3B 6
#define FOC3A 7

#define TCNT3 _SFR_MEM16(0x94)

#define TCNT3L _SFR_MEM8(0x94)
#define TCNT3L0 0
#define TCNT3L1 1
#define TCNT3L2 2
#define TCNT3L3 3
#define TCNT3L4 4
#define TCNT3L5 5
#define TCNT3L6 6
#define TCNT3L7 7

#define TCNT3H _SFR_MEM8(0x95)
#define TCNT3H0 0
#define TCNT3H1 1
#define TCNT3H2 2
#define TCNT3H3 3
#define TCNT3H4 4
#define TCNT3H5 5
#define TCNT3H6 6
#define TCNT3H7 7

#define ICR3 _SFR_MEM16(0x96)

#define ICR3L _SFR_MEM8(0x96)
#define ICR3L0 0
#define ICR3L1 1
#define ICR3L2 2
#define ICR3L3 3
#define ICR3L4 4
#define ICR3L5 5
#define ICR3L6 6
#define ICR3L7 7

#define ICR3H _SFR_MEM8(0x97)
#define ICR3H0 0
#define ICR3H1 1
#define ICR3H2 2
#define ICR3H3 3
#define ICR3H4 4
#define ICR3H5 5
#define ICR3H6 6
#define ICR3H7 7

#define OCR3A _SFR_MEM16(0x98)

#define OCR3AL _SFR_MEM8(0x98)
#define OCR3AL0 0
#define OCR3AL1 1
#define OCR3AL2 2
#define OCR3AL3 3
#define OCR3AL4 4
#define OCR3AL5 5
#define OCR3AL6 6
#define OCR3AL7 7

#define OCR3AH _SFR_MEM8(0x99)
#define OCR3AH0 0
#define OCR3AH1 1
#define OCR3AH2 2
#define OCR3AH3 3
#define OCR3AH4 4
#define OCR3AH5 5
#define OCR3AH6 6
#define OCR3AH7 7

#define OCR3B _SFR_MEM16(0x9A)

#define OCR3BL _SFR_MEM8(0x9A)
#define OCR3AL0 0
#define OCR3AL1 1
#define OCR3AL2 2
#define OCR3AL3 3
#define OCR3AL4 4
#define OCR3AL5 5
#define OCR3AL6 6
#define OCR3AL7 7

#define OCR3BH _SFR_MEM8(0x9B)
#define OCR3AH0 0
#define OCR3AH1 1
#define OCR3AH2 2
#define OCR3AH3 3
#define OCR3AH4 4
#define OCR3AH5 5
#define OCR3AH6 6
#define OCR3AH7 7

#define TCCR2A _SFR_MEM8(0xB0)
#define WGM20 0
#define WGM21 1
#define COM2B0 4
#define COM2B1 5
#define COM2A0 6
#define COM2A1 7

#define TCCR2B _SFR_MEM8(0xB1)
#define CS20 0
#define CS21 1
#define CS22 2
#define WGM22 3
#define FOC2B 6
#define FOC2A 7

#define TCNT2 _SFR_MEM8(0xB2)
#define TCNT2_0 0
#define TCNT2_1 1
#define TCNT2_2 2
#define TCNT2_3 3
#define TCNT2_4 4
#define TCNT2_5 5
#define TCNT2_6 6
#define TCNT2_7 7

#define OCR2A _SFR_MEM8(0xB3)
#define OCR2_0 0
#define OCR2_1 1
#define OCR2_2 2
#define OCR2_3 3
#define OCR2_4 4
#define OCR2_5 5
#define OCR2_6 6
#define OCR2_7 7

#define OCR2B _SFR_MEM8(0xB4)
#define OCR2_0 0
#define OCR2_1 1
#define OCR2_2 2
#define OCR2_3 3
#define OCR2_4 4
#define OCR2_5 5
#define OCR2_6 6
#define OCR2_7 7

#define ASSR _SFR_MEM8(0xB6)
#define TCR2BUB 0
#define TCR2AUB 1
#define OCR2BUB 2
#define OCR2AUB 3
#define TCN2UB 4
#define AS2 5
#define EXCLK 6

#define TWBR _SFR_MEM8(0xB8)
#define TWBR0 0
#define TWBR1 1
#define TWBR2 2
#define TWBR3 3
#define TWBR4 4
#define TWBR5 5
#define TWBR6 6
#define TWBR7 7

#define TWSR _SFR_MEM8(0xB9)
#define TWPS0 0
#define TWPS1 1
#define TWS3 3
#define TWS4 4
#define TWS5 5
#define TWS6 6
#define TWS7 7

#define TWAR _SFR_MEM8(0xBA)
#define TWGCE 0
#define TWA0 1
#define TWA1 2
#define TWA2 3
#define TWA3 4
#define TWA4 5
#define TWA5 6
#define TWA6 7

#define TWDR _SFR_MEM8(0xBB)
#define TWD0 0
#define TWD1 1
#define TWD2 2
#define TWD3 3
#define TWD4 4
#define TWD5 5
#define TWD6 6
#define TWD7 7

#define TWCR _SFR_MEM8(0xBC)
#define TWIE 0
#define TWEN 2
#define TWWC 3
#define TWSTO 4
#define TWSTA 5
#define TWEA 6
#define TWINT 7

#define TWAMR _SFR_MEM8(0xBD)
#define TWAM0 1
#define TWAM1 2
#define TWAM2 3
#define TWAM3 4
#define TWAM4 5
#define TWAM5 6
#define TWAM6 7

#define UCSR0A _SFR_MEM8(0xC0)
#define MPCM0 0
#define U2X0 1
#define UPE0 2
#define DOR0 3
#define FE0 4
#define UDRE0 5
#define TXC0 6
#define RXC0 7

#define UCSR0B _SFR_MEM8(0xC1)
#define TXB80 0
#define RXB80 1
#define UCSZ02 2
#define TXEN0 3
#define RXEN0 4
#define UDRIE0 5
#define TXCIE0 6
#define RXCIE0 7

#define UCSR0C _SFR_MEM8(0xC2)
#define UCPOL0 0
#define UCSZ00 1
#define UCSZ01 2
#define USBS0 3
#define UPM00 4
#define UPM01 5
#define UMSEL00 6
#define UMSEL01 7

#define UBRR0 _SFR_MEM16(0xC4)

#define UBRR0L _SFR_MEM8(0xC4)
#define UBRR0_0 0
#define UBRR0_1 1
#define UBRR0_2 2
#define UBRR0_3 3
#define UBRR0_4 4
#define UBRR0_5 5
#define UBRR0_6 6
#define UBRR0_7 7

#define UBRR0H _SFR_MEM8(0xC5)
#define UBRR0_8 0
#define UBRR0_9 1
#define UBRR0_10 2
#define UBRR0_11 3

#define UDR0 _SFR_MEM8(0xC6)
#define UDR0_0 0
#define UDR0_1 1
#define UDR0_2 2
#define UDR0_3 3
#define UDR0_4 4
#define UDR0_5 5
#define UDR0_6 6
#define UDR0_7 7

#define UCSR1A _SFR_MEM8(0xC8)
#define MPCM1 0
#define U2X1 1
#define UPE1 2
#define DOR1 3
#define FE1 4
#define UDRE1 5
#define TXC1 6
#define RXC1 7

#define UCSR1B _SFR_MEM8(0xC9)
#define TXB81 0
#define RXB81 1
#define UCSZ12 2
#define TXEN1 3
#define RXEN1 4
#define UDRIE1 5
#define TXCIE1 6
#define RXCIE1 7

#define UCSR1C _SFR_MEM8(0xCA)
#define UCPOL1 0
#define UCSZ10 1
#define UCSZ11 2
#define USBS1 3
#define UPM10 4
#define UPM11 5
#define UMSEL10 6
#define UMSEL11 7

#define UBRR1 _SFR_MEM16(0xCC)

#define UBRR1L _SFR_MEM8(0xCC)
#define UBRR1_0 0
#define UBRR1_1 1
#define UBRR1_2 2
#define UBRR1_3 3
#define UBRR1_4 4
#define UBRR1_5 5
#define UBRR1_6 6
#define UBRR1_7 7

#define UBRR1H _SFR_MEM8(0xCD)
#define UBRR1_8 0
#define UBRR1_9 1
#define UBRR1_10 2
#define UBRR1_11 3

#define UDR1 _SFR_MEM8(0xCE)
#define UDR1_0 0
#define UDR1_1 1
#define UDR1_2 2
#define UDR1_3 3
#define UDR1_4 4
#define UDR1_5 5
#define UDR1_6 6
#define UDR1_7 7


/* Interrupt Vectors */
/* Interrupt Vector 0 is the reset vector. */

#define INT0_vect_num     1
#define INT0_vect         _VECTOR(1)  /* External Interrupt Request 0 */

#define INT1_vect_num     2
#define INT1_vect         _VECTOR(2)  /* External Interrupt Request 1 */

#define INT2_vect_num     3
#define INT2_vect         _VECTOR(3)  /* External Interrupt Request 2 */

#define PCINT0_vect_num   4
#define PCINT0_vect       _VECTOR(4)  /* Pin Change Interrupt Request 0 */

#define PCINT1_vect_num   5
#define PCINT1_vect       _VECTOR(5)  /* Pin Change Interrupt Request 1 */

#define PCINT2_vect_num   6
#define PCINT2_vect       _VECTOR(6)  /* Pin Change Interrupt Request 2 */

#define PCINT3_vect_num   7
#define PCINT3_vect       _VECTOR(7)  /* Pin Change Interrupt Request 3 */

#define WDT_vect_num      8
#define WDT_vect          _VECTOR(8)  /* Watchdog Time-out Interrupt */

#define TIMER2_COMPA_vect_num  9
#define TIMER2_COMPA_vect _VECTOR(9)  /* Timer/Counter2 Compare Match A */

#define TIMER2_COMPB_vect_num  10
#define TIMER2_COMPB_vect _VECTOR(10)  /* Timer/Counter2 Compare Match B */

#define TIMER2_OVF_vect_num  11
#define TIMER2_OVF_vect   _VECTOR(11)  /* Timer/Counter2 Overflow */

#define TIMER1_CAPT_vect_num  12
#define TIMER1_CAPT_vect  _VECTOR(12)  /* Timer/Counter1 Capture Event */

#define TIMER1_COMPA_vect_num  13
#define TIMER1_COMPA_vect _VECTOR(13)  /* Timer/Counter1 Compare Match A */

#define TIMER1_COMPB_vect_num  14
#define TIMER1_COMPB_vect _VECTOR(14)  /* Timer/Counter1 Compare Match B */

#define TIMER1_OVF_vect_num  15
#define TIMER1_OVF_vect   _VECTOR(15)  /* Timer/Counter1 Overflow */

#define TIMER0_COMPA_vect_num  16
#define TIMER0_COMPA_vect _VECTOR(16)  /* Timer/Counter0 Compare Match A */

#define TIMER0_COMPB_vect_num  17
#define TIMER0_COMPB_vect _VECTOR(17)  /* Timer/Counter0 Compare Match B */

#define TIMER0_OVF_vect_num  18
#define TIMER0_OVF_vect   _VECTOR(18)  /* Timer/Counter0 Overflow */

#define SPI_STC_vect_num  19
#define SPI_STC_vect      _VECTOR(19)  /* SPI Serial Transfer Complete */

#define USART0_RX_vect_num  20
#define USART0_RX_vect    _VECTOR(20)  /* USART0, Rx Complete */

#define USART0_UDRE_vect_num  21
#define USART0_UDRE_vect  _VECTOR(21)  /* USART0 Data register Empty */

#define USART0_TX_vect_num  22
#define USART0_TX_vect    _VECTOR(22)  /* USART0, Tx Complete */

#define ANALOG_COMP_vect_num  23
#define ANALOG_COMP_vect  _VECTOR(23)  /* Analog Comparator */

#define ADC_vect_num      24
#define ADC_vect          _VECTOR(24)  /* ADC Conversion Complete */

#define EE_READY_vect_num 25
#define EE_READY_vect     _VECTOR(25)  /* EEPROM Ready */

#define TWI_vect_num      26
#define TWI_vect          _VECTOR(26)  /* 2-wire Serial Interface */

#define SPM_READY_vect_num  27
#define SPM_READY_vect    _VECTOR(27)  /* Store Program Memory Read */

#define USART1_RX_vect_num  28
#define USART1_RX_vect    _VECTOR(28)  /* USART1 RX complete */

#define USART1_UDRE_vect_num  29
#define USART1_UDRE_vect  _VECTOR(29)  /* USART1 Data Register Empty */

#define USART1_TX_vect_num  30
#define USART1_TX_vect    _VECTOR(30)  /* USART1 TX complete */

#define TIMER3_CAPT_vect_num  31
#define TIMER3_CAPT_vect  _VECTOR(31)  /* Timer/Counter3 Capture Event */

#define TIMER3_COMPA_vect_num  32
#define TIMER3_COMPA_vect _VECTOR(32)  /* Timer/Counter3 Compare Match A */

#define TIMER3_COMPB_vect_num  33
#define TIMER3_COMPB_vect _VECTOR(33)  /* Timer/Counter3 Compare Match B */

#define TIMER3_OVF_vect_num  34
#define TIMER3_OVF_vect   _VECTOR(34)  /* Timer/Counter3 Overflow */

#define _VECTORS_SIZE (35 * 4)


/* Constants */
#define SPM_PAGESIZE 256
#define RAMSTART     (0x100)
#define RAMEND       0x40FF    /* Last On-Chip SRAM Location */
#define XRAMSIZE     0
#define XRAMEND      RAMEND
#define E2END        0xFFF
#define E2PAGESIZE   8
#define FLASHEND     0x1FFFF


/* Fuses */
#define FUSE_MEMORY_SIZE 3

/* Low Fuse Byte */
#define FUSE_CKSEL0 (unsigned char)~_BV(0)  /* Select Clock Source */
#define FUSE_CKSEL1 (unsigned char)~_BV(1)  /* Select Clock Source */
#define FUSE_CKSEL2 (unsigned char)~_BV(2)  /* Select Clock Source */
#define FUSE_CKSEL3 (unsigned char)~_BV(3)  /* Select Clock Source */
#define FUSE_SUT0   (unsigned char)~_BV(4)  /* Select start-up time */
#define FUSE_SUT1   (unsigned char)~_BV(5)  /* Select start-up time */
#define FUSE_CKOUT  (unsigned char)~_BV(6)  /* Clock output */
#define FUSE_CKDIV8 (unsigned char)~_BV(7)  /* Divide clock by 8 */
#define LFUSE_DEFAULT (FUSE_CKSEL0 & FUSE_CKSEL2 & FUSE_CKSEL3 & FUSE_SUT0 & FUSE_SUT1 & FUSE_CKDIV8)

/* High Fuse Byte */
#define FUSE_BOOTRST (unsigned char)~_BV(0)  /* Select Reset Vector */
#define FUSE_BOOTSZ0 (unsigned char)~_BV(1)  /* Select Boot Size */
#define FUSE_BOOTSZ1 (unsigned char)~_BV(2)  /* Select Boot Size */
#define FUSE_EESAVE  (unsigned char)~_BV(3)  /* EEPROM memory is preserved through chip erase */
#define FUSE_WDTON   (unsigned char)~_BV(4)  /* Watchdog timer always on */
#define FUSE_SPIEN   (unsigned char)~_BV(5)  /* Enable Serial programming and Data Downloading */
#define FUSE_JTAGEN  (unsigned char)~_BV(6)  /* Enable JTAG */
#define FUSE_OCDEN   (unsigned char)~_BV(7)  /* Enable OCD */
#define HFUSE_DEFAULT (FUSE_BOOTSZ0 & FUSE_SPIEN & FUSE_JTAGEN)

/* Extended Fuse Byte */
#define FUSE_BODLEVEL0 (unsigned char)~_BV(0)  /* Brown-out Detector trigger level */
#define FUSE_BODLEVEL1 (unsigned char)~_BV(1)  /* Brown-out Detector trigger level */
#define FUSE_BODLEVEL2 (unsigned char)~_BV(2)  /* Brown-out Detector trigger level */
#define EFUSE_DEFAULT (0xFF)


/* Lock Bits */
#define __LOCK_BITS_EXIST
#define __BOOT_LOCK_BITS_0_EXIST
#define __BOOT_LOCK_BITS_1_EXIST 


/* Signature */
#define SIGNATURE_0 0x1E
#define SIGNATURE_1 0x97
#define SIGNATURE_2 0x05


#define SLEEP_MODE_IDLE (0x00<<1)
#define SLEEP_MODE_ADC (0x01<<1)
#define SLEEP_MODE_PWR_DOWN (0x02<<1)
#define SLEEP_MODE_PWR_SAVE (0x03<<1)
#define SLEEP_MODE_STANDBY (0x06<<1)
#define SLEEP_MODE_EXT_STANDBY (0x07<<1)


#endif  /* _AVR_IOM1284P_H_ */
