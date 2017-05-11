/* Copyright (c) 2008 Atmel Corporation
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

/* $Id: iom32u4.h 2225 2011-03-02 16:27:26Z arcanum $ */

/* avr/iom32u4.h - definitions for ATmega32U4. */

/* This file should only be included from <avr/io.h>, never directly. */

#ifndef _AVR_IO_H_
#  error "Include <avr/io.h> instead of this file."
#endif

#ifndef _AVR_IOXXX_H_
#  define _AVR_IOXXX_H_ "iom32u4.h"
#else
#  error "Attempt to include more than one <avr/ioXXX.h> file."
#endif 


#ifndef _AVR_IOM32U4_H_
#define _AVR_IOM32U4_H_ 1


/* Registers and associated bit numbers */

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
#define PINC6 6
#define PINC7 7

#define DDRC _SFR_IO8(0x07)
#define DDC6 6
#define DDC7 7

#define PORTC _SFR_IO8(0x08)
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

#define PINE _SFR_IO8(0x0C)
#define PINE2 2
#define PINE6 6

#define DDRE _SFR_IO8(0x0D)
#define DDE2 2
#define DDE6 6

#define PORTE _SFR_IO8(0x0E)
#define PORTE2 2
#define PORTE6 6

#define PINF _SFR_IO8(0x0F)
#define PINF0 0
#define PINF1 1
#define PINF4 4
#define PINF5 5
#define PINF6 6
#define PINF7 7

#define DDRF _SFR_IO8(0x10)
#define DDF0 0
#define DDF1 1
#define DDF4 4
#define DDF5 5
#define DDF6 6
#define DDF7 7

#define PORTF _SFR_IO8(0x11)
#define PORTF0 0
#define PORTF1 1
#define PORTF4 4
#define PORTF5 5
#define PORTF6 6
#define PORTF7 7

#define TIFR0 _SFR_IO8(0x15)
#define TOV0 0
#define OCF0A 1
#define OCF0B 2

#define TIFR1 _SFR_IO8(0x16)
#define TOV1 0
#define OCF1A 1
#define OCF1B 2
#define OCF1C 3
#define ICF1 5

#define TIFR2 _SFR_IO8(0x17)
#define TOV2 0
#define OCF2A 1
#define OCF2B 2

#define TIFR3 _SFR_IO8(0x18)
#define TOV3 0
#define OCF3A 1
#define OCF3B 2
#define OCF3C 3
#define ICF3 5

#define TIFR4 _SFR_IO8(0x19)
#define TOV4 2
#define OCF4B 5
#define OCF4A 6
#define OCF4D 7

#define TIFR5 _SFR_IO8(0x1A)

#define PCIFR _SFR_IO8(0x1B)
#define PCIF0 0

#define EIFR _SFR_IO8(0x1C)
#define INTF0 0
#define INTF1 1
#define INTF2 2
#define INTF3 3
#define INTF4 4
#define INTF5 5
#define INTF6 6
#define INTF7 7

#define EIMSK _SFR_IO8(0x1D)
#define INT0 0
#define INT1 1
#define INT2 2
#define INT3 3
#define INT4 4
#define INT5 5
#define INT6 6
#define INT7 7

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

#define PLLCSR _SFR_IO8(0x29)
#define PLOCK 0
#define PLLE 1
#define PINDIV 4

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

#define PLLFRQ _SFR_IO8(0x32)
#define PDIV0 0
#define PDIV1 1
#define PDIV2 2
#define PDIV3 3
#define PLLTM0 4
#define PLLTM1 5
#define PLLUSB 6
#define PINMUX 7

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

#define EIND _SFR_IO8(0x3C)
#define EIND0 0

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
#define PRTIM0 5
#define PRTIM2 6
#define PRTWI 7

#define PRR1 _SFR_MEM8(0x65)
#define PRUSART1 0
#define PRTIM3 3
#define PRUSB 7

#define OSCCAL _SFR_MEM8(0x66)
#define CAL0 0
#define CAL1 1
#define CAL2 2
#define CAL3 3
#define CAL4 4
#define CAL5 5
#define CAL6 6
#define CAL7 7

#define RCCTRL _SFR_MEM8(0x67)
#define RCFREQ 0

#define PCICR _SFR_MEM8(0x68)
#define PCIE0 0

#define EICRA _SFR_MEM8(0x69)
#define ISC00 0
#define ISC01 1
#define ISC10 2
#define ISC11 3
#define ISC20 4
#define ISC21 5
#define ISC30 6
#define ISC31 7

#define EICRB _SFR_MEM8(0x6A)
#define ISC40 0
#define ISC41 1
#define ISC50 2
#define ISC51 3
#define ISC60 4
#define ISC61 5
#define ISC70 6
#define ISC71 7

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

#define PCMSK2 _SFR_MEM8(0x6D)

#define TIMSK0 _SFR_MEM8(0x6E)
#define TOIE0 0
#define OCIE0A 1
#define OCIE0B 2

#define TIMSK1 _SFR_MEM8(0x6F)
#define TOIE1 0
#define OCIE1A 1
#define OCIE1B 2
#define OCIE1C 3
#define ICIE1 5

#define TIMSK2 _SFR_MEM8(0x70)
#define TOIE2 0
#define OCIE2A 1
#define OCIE2B 2

#define TIMSK3 _SFR_MEM8(0x71)
#define TOIE3 0
#define OCIE3A 1
#define OCIE3B 2
#define OCIE3C 3
#define ICIE3 5

#define TIMSK4 _SFR_MEM8(0x72)
#define TOIE4 2
#define OCIE4B 5
#define OCIE4A 6
#define OCIE4D 7

#define TIMSK5 _SFR_MEM8(0x73)

#ifndef _ASSEMBLER_
#define ADC _SFR_MEM16(0x78)
#endif
#define ADCW _SFR_MEM16(0x78)

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
#define ADTS3 3
#define MUX5 5
#define ACME 6
#define ADHSM 7

#define ADMUX _SFR_MEM8(0x7C)
#define MUX0 0
#define MUX1 1
#define MUX2 2
#define MUX3 3
#define MUX4 4
#define ADLAR 5
#define REFS0 6
#define REFS1 7

#define DIDR2 _SFR_MEM8(0x7D)
#define ADC8D 0
#define ADC9D 1
#define ADC10D 2
#define ADC11D 3
#define ADC12D 4
#define ADC13D 5

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
#define COM1C0 2
#define COM1C1 3
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
#define FOC1C 5 
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
#define OCR1BL0 0
#define OCR1BL1 1
#define OCR1BL2 2
#define OCR1BL3 3
#define OCR1BL4 4
#define OCR1BL5 5
#define OCR1BL6 6
#define OCR1BL7 7

#define OCR1BH _SFR_MEM8(0x8B)
#define OCR1BH0 0
#define OCR1BH1 1
#define OCR1BH2 2
#define OCR1BH3 3
#define OCR1BH4 4
#define OCR1BH5 5
#define OCR1BH6 6
#define OCR1BH7 7

#define OCR1C _SFR_MEM16(0x8C)

#define OCR1CL _SFR_MEM8(0x8C)
#define OCR1CL0 0
#define OCR1CL1 1
#define OCR1CL2 2
#define OCR1CL3 3
#define OCR1CL4 4
#define OCR1CL5 5
#define OCR1CL6 6
#define OCR1CL7 7

#define OCR1CH _SFR_MEM8(0x8D)
#define OCR1CH0 0
#define OCR1CH1 1
#define OCR1CH2 2
#define OCR1CH3 3
#define OCR1CH4 4
#define OCR1CH5 5
#define OCR1CH6 6
#define OCR1CH7 7

#define TCCR3A _SFR_MEM8(0x90)
#define WGM30 0
#define WGM31 1
#define COM3C0 2
#define COM3C1 3
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
#define FOC3C 5
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
#define OCR3BL0 0
#define OCR3BL1 1
#define OCR3BL2 2
#define OCR3BL3 3
#define OCR3BL4 4
#define OCR3BL5 5
#define OCR3BL6 6
#define OCR3BL7 7

#define OCR3BH _SFR_MEM8(0x9B)
#define OCR3BH0 0
#define OCR3BH1 1
#define OCR3BH2 2
#define OCR3BH3 3
#define OCR3BH4 4
#define OCR3BH5 5
#define OCR3BH6 6
#define OCR3BH7 7

#define OCR3C _SFR_MEM16(0x9C)

#define OCR3CL _SFR_MEM8(0x9C)
#define OCR3CL0 0
#define OCR3CL1 1
#define OCR3CL2 2
#define OCR3CL3 3
#define OCR3CL4 4
#define OCR3CL5 5
#define OCR3CL6 6
#define OCR3CL7 7

#define OCR3CH _SFR_MEM8(0x9D)
#define OCR3CH0 0
#define OCR3CH1 1
#define OCR3CH2 2
#define OCR3CH3 3
#define OCR3CH4 4
#define OCR3CH5 5
#define OCR3CH6 6
#define OCR3CH7 7

#define UHCON _SFR_MEM8(0x9E)

#define UHINT _SFR_MEM8(0x9F)

#define UHIEN _SFR_MEM8(0xA0)

#define UHADDR _SFR_MEM8(0xA1)

#define UHFNUM _SFR_MEM16(0xA2)

#define UHFNUML _SFR_MEM8(0xA2)

#define UHFNUMH _SFR_MEM8(0xA3)

#define UHFLEN _SFR_MEM8(0xA4)

#define UPINRQX _SFR_MEM8(0xA5)

#define UPINTX _SFR_MEM8(0xA6)

#define UPNUM _SFR_MEM8(0xA7)

#define UPRST _SFR_MEM8(0xA8)

#define UPCONX _SFR_MEM8(0xA9)

#define UPCFG0X _SFR_MEM8(0xAA)

#define UPCFG1X _SFR_MEM8(0xAB)

#define UPSTAX _SFR_MEM8(0xAC)

#define UPCFG2X _SFR_MEM8(0xAD)

#define UPIENX _SFR_MEM8(0xAE)

#define UPDATX _SFR_MEM8(0xAF)

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

#define TCNT4 _SFR_MEM16(0xBE)

#define TCNT4L _SFR_MEM8(0xBE)
#define TC40 0
#define TC41 1
#define TC42 2
#define TC43 3
#define TC44 4
#define TC45 5
#define TC46 6
#define TC47 7

#define TCNT4H _SFR_MEM8(0xBF)  /* Alias for naming consistency. */
#define TC4H _SFR_MEM8(0xBF)    /* Per XML device file. */
#define TC48 0
#define TC49 1
#define TC410 2

#define TCCR4A _SFR_MEM8(0xC0)
#define PWM4B 0
#define PWM4A 1
#define FOC4B 2
#define FOC4A 3
#define COM4B0 4
#define COM4B1 5
#define COM4A0 6
#define COM4A1 7

#define TCCR4B _SFR_MEM8(0xC1)
#define CS40 0
#define CS41 1
#define CS42 2
#define CS43 3
#define DTPS40 4
#define DTPS41 5
#define PSR4 6
#define PWM4X 7

#define TCCR4C _SFR_MEM8(0xC2)
#define PWM4D 0
#define FOC4D 1
#define COM4D0 2
#define COM4D1 3
#define COM4B0S 4
#define COM4B1S 5
#define COM4A0S 6
#define COM4A1S 7

#define TCCR4D _SFR_MEM8(0xC3)
#define WGM40 0
#define WGM41 1
#define FPF4 2
#define FPAC4 3
#define FPES4 4
#define FPNC4 5
#define FPEN4 6
#define FPIE4 7

#define TCCR4E _SFR_MEM8(0xC4)
#define OC4OE0 0
#define OC4OE1 1
#define OC4OE2 2
#define OC4OE3 3
#define OC4OE4 4
#define OC4OE5 5
#define ENHC4 6
#define TLOCK4 7

#define CLKSEL0 _SFR_MEM8(0xC5)
#define CLKS 0
#define EXTE 2
#define RCE 3
#define EXSUT0 4
#define EXSUT1 5
#define RCSUT0 6
#define RCSUT1 7

#define CLKSEL1 _SFR_MEM8(0xC6)
#define EXCKSEL0 0
#define EXCKSEL1 1
#define EXCKSEL2 2
#define EXCKSEL3 3
#define RCCKSEL0 4
#define RCCKSEL1 5
#define RCCKSEL2 6
#define RCCKSEL3 7

#define CLKSTA _SFR_MEM8(0xC7)
#define EXTON 0
#define RCON 1

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

#define UBRR1H _SFR_MEM8(0xCD)

#define UDR1 _SFR_MEM8(0xCE)
#define UDR1_0 0
#define UDR1_1 1
#define UDR1_2 2
#define UDR1_3 3
#define UDR1_4 4
#define UDR1_5 5
#define UDR1_6 6
#define UDR1_7 7

#define OCR4A _SFR_MEM8(0xCF)
#define OCR4A0 0
#define OCR4A1 1
#define OCR4A2 2
#define OCR4A3 3
#define OCR4A4 4
#define OCR4A5 5
#define OCR4A6 6
#define OCR4A7 7

#define OCR4B _SFR_MEM8(0xD0)
#define OCR4B0 0
#define OCR4B1 1
#define OCR4B2 2
#define OCR4B3 3
#define OCR4B4 4
#define OCR4B5 5
#define OCR4B6 6
#define OCR4B7 7

#define OCR4C _SFR_MEM8(0xD1)
#define OCR4C0 0
#define OCR4C1 1
#define OCR4C2 2
#define OCR4C3 3
#define OCR4C4 4
#define OCR4C5 5
#define OCR4C6 6
#define OCR4C7 7

#define OCR4D _SFR_MEM8(0xD2)
#define OCR4D0 0
#define OCR4D1 1
#define OCR4D2 2
#define OCR4D3 3
#define OCR4D4 4
#define OCR4D5 5
#define OCR4D6 6
#define OCR4D7 7

#define DT4 _SFR_MEM8(0xD4)
#define DT4L0 0
#define DT4L1 1
#define DT4L2 2
#define DT4L3 3
#define DT4L4 4
#define DT4L5 5
#define DT4L6 6
#define DT4L7 7

#define UHWCON _SFR_MEM8(0xD7)
#define UVREGE 0

#define USBCON _SFR_MEM8(0xD8)
#define VBUSTE 0
#define OTGPADE 4
#define FRZCLK 5
#define USBE 7

#define USBSTA _SFR_MEM8(0xD9)
#define VBUS 0
#define SPEED 3

#define USBINT _SFR_MEM8(0xDA)
#define VBUSTI 0

#define OTGCON _SFR_MEM8(0xDD)

#define OTGIEN _SFR_MEM8(0xDE)

#define OTGINT _SFR_MEM8(0xDF)

#define UDCON _SFR_MEM8(0xE0)
#define DETACH 0
#define RMWKUP 1
#define LSM 2
#define RSTCPU 3

#define UDINT _SFR_MEM8(0xE1)
#define SUSPI 0
#define SOFI 2
#define EORSTI 3
#define WAKEUPI 4
#define EORSMI 5
#define UPRSMI 6

#define UDIEN _SFR_MEM8(0xE2)
#define SUSPE 0
#define SOFE 2
#define EORSTE 3
#define WAKEUPE 4
#define EORSME 5
#define UPRSME 6

#define UDADDR _SFR_MEM8(0xE3)
#define UADD0 0
#define UADD1 1
#define UADD2 2
#define UADD3 3
#define UADD4 4
#define UADD5 5
#define UADD6 6
#define ADDEN 7

#define UDFNUM _SFR_MEM16(0xE4)

#define UDFNUML _SFR_MEM8(0xE4)
#define FNUM0 0
#define FNUM1 1
#define FNUM2 2
#define FNUM3 3
#define FNUM4 4
#define FNUM5 5
#define FNUM6 6
#define FNUM7 7

#define UDFNUMH _SFR_MEM8(0xE5)
#define FNUM8 0
#define FNUM9 1
#define FNUM10 2

#define UDMFN _SFR_MEM8(0xE6)
#define FNCERR 4

#define UDTST _SFR_MEM8(0xE7)

#define UEINTX _SFR_MEM8(0xE8)
#define TXINI 0
#define STALLEDI 1
#define RXOUTI 2
#define RXSTPI 3
#define NAKOUTI 4
#define RWAL 5
#define NAKINI 6
#define FIFOCON 7

#define UENUM _SFR_MEM8(0xE9)
#define UENUM_0 0
#define UENUM_1 1
#define UENUM_2 2

#define UERST _SFR_MEM8(0xEA)
#define EPRST0 0
#define EPRST1 1
#define EPRST2 2
#define EPRST3 3
#define EPRST4 4
#define EPRST5 5
#define EPRST6 6

#define UECONX _SFR_MEM8(0xEB)
#define EPEN 0
#define RSTDT 3
#define STALLRQC 4
#define STALLRQ 5

#define UECFG0X _SFR_MEM8(0xEC)
#define EPDIR 0
#define EPTYPE0 6
#define EPTYPE1 7

#define UECFG1X _SFR_MEM8(0xED)
#define ALLOC 1
#define EPBK0 2
#define EPBK1 3
#define EPSIZE0 4
#define EPSIZE1 5
#define EPSIZE2 6

#define UESTA0X _SFR_MEM8(0xEE)
#define NBUSYBK0 0
#define NBUSYBK1 1
#define DTSEQ0 2
#define DTSEQ1 3
#define UNDERFI 5
#define OVERFI 6
#define CFGOK 7

#define UESTA1X _SFR_MEM8(0xEF)
#define CURRBK0 0
#define CURRBK1 1
#define CTRLDIR 2

#define UEIENX _SFR_MEM8(0xF0)
#define TXINE 0
#define STALLEDE 1
#define RXOUTE 2
#define RXSTPE 3
#define NAKOUTE 4
#define NAKINE 6
#define FLERRE 7

#define UEDATX _SFR_MEM8(0xF1)
#define DAT0 0
#define DAT1 1
#define DAT2 2
#define DAT3 3
#define DAT4 4
#define DAT5 5
#define DAT6 6
#define DAT7 7

#define UEBCX _SFR_MEM16(0xF2)

#define UEBCLX _SFR_MEM8(0xF2)
#define BYCT0 0
#define BYCT1 1
#define BYCT2 2
#define BYCT3 3
#define BYCT4 4
#define BYCT5 5
#define BYCT6 6
#define BYCT7 7

#define UEBCHX _SFR_MEM8(0xF3)

#define UEINT _SFR_MEM8(0xF4)
#define EPINT0 0
#define EPINT1 1
#define EPINT2 2
#define EPINT3 3
#define EPINT4 4
#define EPINT5 5
#define EPINT6 6

#define UPERRX _SFR_MEM8(0xF5)

#define UPBCLX _SFR_MEM8(0xF6)

#define UPBCHX _SFR_MEM8(0xF7)

#define UPINT _SFR_MEM8(0xF8)

#define OTGTCON _SFR_MEM8(0xF9)



/* Interrupt Vectors */
/* Interrupt Vector 0 is the reset vector. */

#define INT0_vect_num       1
#define INT0_vect           _VECTOR(1)  /* External Interrupt Request 0 */

#define INT1_vect_num       2
#define INT1_vect           _VECTOR(2)  /* External Interrupt Request 1 */

#define INT2_vect_num       3
#define INT2_vect           _VECTOR(3)  /* External Interrupt Request 2 */

#define INT3_vect_num       4
#define INT3_vect           _VECTOR(4)  /* External Interrupt Request 3 */

#define INT6_vect_num       7
#define INT6_vect           _VECTOR(7)  /* External Interrupt Request 6 */

#define PCINT0_vect_num     9
#define PCINT0_vect         _VECTOR(9)  /* Pin Change Interrupt Request 0 */

#define USB_GEN_vect_num    10
#define USB_GEN_vect        _VECTOR(10)  /* USB General Interrupt Request */

#define USB_COM_vect_num    11
#define USB_COM_vect        _VECTOR(11)  /* USB Endpoint/Pipe Interrupt Communication Request */

#define WDT_vect_num        12
#define WDT_vect            _VECTOR(12)  /* Watchdog Time-out Interrupt */

#define TIMER1_CAPT_vect_num  16
#define TIMER1_CAPT_vect    _VECTOR(16)  /* Timer/Counter1 Capture Event */

#define TIMER1_COMPA_vect_num  17
#define TIMER1_COMPA_vect   _VECTOR(17)  /* Timer/Counter1 Compare Match A */

#define TIMER1_COMPB_vect_num   18
#define TIMER1_COMPB_vect   _VECTOR(18)  /* Timer/Counter1 Compare Match B */

#define TIMER1_COMPC_vect_num   19
#define TIMER1_COMPC_vect   _VECTOR(19)  /* Timer/Counter1 Compare Match C */

#define TIMER1_OVF_vect_num 20
#define TIMER1_OVF_vect     _VECTOR(20)  /* Timer/Counter1 Overflow */

#define TIMER0_COMPA_vect_num   21
#define TIMER0_COMPA_vect   _VECTOR(21)  /* Timer/Counter0 Compare Match A */

#define TIMER0_COMPB_vect_num   22
#define TIMER0_COMPB_vect   _VECTOR(22)  /* Timer/Counter0 Compare Match B */

#define TIMER0_OVF_vect_num 23
#define TIMER0_OVF_vect     _VECTOR(23)  /* Timer/Counter0 Overflow */

#define SPI_STC_vect_num    24
#define SPI_STC_vect        _VECTOR(24)  /* SPI Serial Transfer Complete */

#define USART1_RX_vect_num  25
#define USART1_RX_vect      _VECTOR(25)  /* USART1, Rx Complete */

#define USART1_UDRE_vect_num 26
#define USART1_UDRE_vect    _VECTOR(26)  /* USART1 Data register Empty */

#define USART1_TX_vect_num  27
#define USART1_TX_vect      _VECTOR(27)  /* USART1, Tx Complete */

#define ANALOG_COMP_vect_num 28
#define ANALOG_COMP_vect    _VECTOR(28)  /* Analog Comparator */

#define ADC_vect_num        29
#define ADC_vect            _VECTOR(29)  /* ADC Conversion Complete */

#define EE_READY_vect_num   30
#define EE_READY_vect       _VECTOR(30)  /* EEPROM Ready */

#define TIMER3_CAPT_vect_num 31
#define TIMER3_CAPT_vect    _VECTOR(31)  /* Timer/Counter3 Capture Event */

#define TIMER3_COMPA_vect_num 32
#define TIMER3_COMPA_vect   _VECTOR(32)  /* Timer/Counter3 Compare Match A */

#define TIMER3_COMPB_vect_num 33
#define TIMER3_COMPB_vect   _VECTOR(33)  /* Timer/Counter3 Compare Match B */

#define TIMER3_COMPC_vect_num 34
#define TIMER3_COMPC_vect   _VECTOR(34)  /* Timer/Counter3 Compare Match C */

#define TIMER3_OVF_vect_num 35
#define TIMER3_OVF_vect     _VECTOR(35)  /* Timer/Counter3 Overflow */

#define TWI_vect_num        36
#define TWI_vect            _VECTOR(36)  /* 2-wire Serial Interface         */

#define SPM_READY_vect_num  37
#define SPM_READY_vect      _VECTOR(37)  /* Store Program Memory Read */

#define TIMER4_COMPA_vect_num 38
#define TIMER4_COMPA_vect   _VECTOR(38)  /* Timer/Counter4 Compare Match A */

#define TIMER4_COMPB_vect_num 39
#define TIMER4_COMPB_vect   _VECTOR(39)  /* Timer/Counter4 Compare Match B */

#define TIMER4_COMPD_vect_num 40
#define TIMER4_COMPD_vect   _VECTOR(40)  /* Timer/Counter4 Compare Match D */

#define TIMER4_OVF_vect_num 41
#define TIMER4_OVF_vect     _VECTOR(41)  /* Timer/Counter4 Overflow */

#define TIMER4_FPF_vect_num 42
#define TIMER4_FPF_vect     _VECTOR(42)  /* Timer/Counter4 Fault Protection Interrupt */

#define _VECTORS_SIZE (43 * 4)



/* Constants */
#define SPM_PAGESIZE (128)
#define RAMSTART     (0x100)
#define RAMSIZE      (0xA00)
#define RAMEND       (RAMSTART + RAMSIZE - 1)  /* Last On-Chip SRAM Location */
#define XRAMSTART    (0x2200)
#define XRAMSIZE     (0x10000)
#define XRAMEND      (XRAMSIZE - 1)
#define E2END        (0x3FF)
#define E2PAGESIZE   (4) 
#define FLASHEND     (0x7FFF)



/* Fuses */
#define FUSE_MEMORY_SIZE 3

/* Low Fuse Byte */
#define FUSE_CKSEL0 (unsigned char)~_BV(0)  /* Select Clock Source */
#define FUSE_CKSEL1 (unsigned char)~_BV(1)  /* Select Clock Source */
#define FUSE_CKSEL2 (unsigned char)~_BV(2)  /* Select Clock Source */
#define FUSE_CKSEL3 (unsigned char)~_BV(3)  /* Select Clock Source */
#define FUSE_SUT0   (unsigned char)~_BV(4)  /* Select start-up time */
#define FUSE_SUT1   (unsigned char)~_BV(5)  /* Select start-up time */
#define FUSE_CKOUT  (unsigned char)~_BV(6)  /* Oscillator options */
#define FUSE_CKDIV8 (unsigned char)~_BV(7)  /* Divide clock by 8 */
#define LFUSE_DEFAULT (FUSE_CKSEL1 & FUSE_CKSEL2 & FUSE_CKSEL3 & FUSE_SUT1 & FUSE_CKDIV8)

/* High Fuse Byte */
#define FUSE_BOOTRST (unsigned char)~_BV(0)  /* Select Reset Vector */
#define FUSE_BOOTSZ0 (unsigned char)~_BV(1)  /* Select Boot Size */
#define FUSE_BOOTSZ1 (unsigned char)~_BV(2)  /* Select Boot Size */
#define FUSE_EESAVE  (unsigned char)~_BV(3)  /* EEPROM memory is preserved through chip erase */
#define FUSE_WDTON   (unsigned char)~_BV(4)  /* Watchdog timer always on */
#define FUSE_SPIEN   (unsigned char)~_BV(5)  /* Enable Serial programming and Data Downloading */
#define FUSE_JTAGEN  (unsigned char)~_BV(6)  /* Enable JTAG */
#define FUSE_OCDEN   (unsigned char)~_BV(7)  /* Enable OCD */
#define HFUSE_DEFAULT (FUSE_BOOTSZ0 & FUSE_SPIEN)

/* Extended Fuse Byte */
#define FUSE_BODLEVEL0 (unsigned char)~_BV(0)  /* Brown-out Detector trigger level */
#define FUSE_BODLEVEL1 (unsigned char)~_BV(1)  /* Brown-out Detector trigger level */
#define FUSE_BODLEVEL2 (unsigned char)~_BV(2)  /* Brown-out Detector trigger level */
#define FUSE_HWBE      (unsigned char)~_BV(3)  /* Hardware Boot Enable */
#define EFUSE_DEFAULT (0xFF)



/* Lock Bits */
#define __LOCK_BITS_EXIST
#define __BOOT_LOCK_BITS_0_EXIST
#define __BOOT_LOCK_BITS_1_EXIST 



/* Signature */
#define SIGNATURE_0 0x1E
#define SIGNATURE_1 0x95
#define SIGNATURE_2 0x87



#endif  /* _AVR_IOM32U4_H_ */
