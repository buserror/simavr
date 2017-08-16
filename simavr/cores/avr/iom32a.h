/*****************************************************************************
 *
 * Copyright (C) 2014 Atmel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * * Neither the name of the copyright holders nor the names of
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************/


/* $Id: iom32a.h 2456 2014-11-19 09:57:29Z saaadhu $ */

#ifndef _AVR_ATMEGA32A_H_INCLUDED
#define _AVR_ATMEGA32A_H_INCLUDED


#ifndef _AVR_IO_H_
#  error "Include <avr/io.h> instead of this file."
#endif

#ifndef _AVR_IOXXX_H_
#  define _AVR_IOXXX_H_ "iom32a.h"
#else
#  error "Attempt to include more than one <avr/ioXXX.h> file."
#endif

/* Registers and associated bit numbers */

#define TWBR    _SFR_IO8(0x00)

#define TWSR    _SFR_IO8(0x01)
#define TWPS0   0
#define TWPS1   1
#define TWS3    3
#define TWS4    4
#define TWS5    5
#define TWS6    6
#define TWS7    7

#define TWAR    _SFR_IO8(0x02)

#define TWDR    _SFR_IO8(0x03)

/* Combine ADCL and ADCH */
#ifndef __ASSEMBLER__
#define ADC     _SFR_IO16(0x04)
#endif
#define ADCW    _SFR_IO16(0x04)

#define ADCL    _SFR_IO8(0x04)
#define ADCH    _SFR_IO8(0x05)

#define ADCSRA  _SFR_IO8(0x06)
#define ADPS0   0
#define ADPS1   1
#define ADPS2   2
#define ADIE    3
#define ADIF    4
#define ADATE   5
#define ADSC    6
#define ADEN    7

#define ADMUX   _SFR_IO8(0x07)
#define MUX0    0
#define MUX1    1
#define MUX2    2
#define MUX3    3
#define MUX4    4
#define ADLAR   5
#define REFS0   6
#define REFS1   7

#define ACSR    _SFR_IO8(0x08)
#define ACIS0   0
#define ACIS1   1
#define ACIC    2
#define ACIE    3
#define ACI     4
#define ACO     5
#define ACBG    6
#define ACD     7

#define UBRRL   _SFR_IO8(0x09)

#define UCSRB   _SFR_IO8(0x0A)
#define TXB8    0
#define RXB8    1
#define UCSZ2   2
#define TXEN    3
#define RXEN    4
#define UDRIE   5
#define TXCIE   6
#define RXCIE   7

#define UCSRA   _SFR_IO8(0x0B)
#define MPCM    0
#define U2X     1
#define UPE     2
#define DOR     3
#define FE      4
#define UDRE    5
#define TXC     6
#define RXC     7

#define UDR     _SFR_IO8(0x0C)

#define SPCR    _SFR_IO8(0x0D)
#define SPR0    0
#define SPR1    1
#define CPHA    2
#define CPOL    3
#define MSTR    4
#define DORD    5
#define SPE     6
#define SPIE    7

#define SPSR    _SFR_IO8(0x0E)
#define SPI2X   0
#define WCOL    6
#define SPIF    7

#define SPDR    _SFR_IO8(0x0F)

#define PIND    _SFR_IO8(0x10)
#define PIND7   7
#define PIND6   6
#define PIND5   5
#define PIND4   4
#define PIND3   3
#define PIND2   2
#define PIND1   1
#define PIND0   0

#define DDRD    _SFR_IO8(0x11)
#define DDRD7   7
#define DDRD6   6
#define DDRD5   5
#define DDRD4   4
#define DDRD3   3
#define DDRD2   2
#define DDRD1   1
#define DDRD0   0

#define PORTD   _SFR_IO8(0x12)
#define PORTD7  7
#define PORTD6  6
#define PORTD5  5
#define PORTD4  4
#define PORTD3  3
#define PORTD2  2
#define PORTD1  1
#define PORTD0  0

#define PINC    _SFR_IO8(0x13)
#define PINC7   7
#define PINC6   6
#define PINC5   5
#define PINC4   4
#define PINC3   3
#define PINC2   2
#define PINC1   1
#define PINC0   0

#define DDRC    _SFR_IO8(0x14)
#define DDRC7   7
#define DDRC6   6
#define DDRC5   5
#define DDRC4   4
#define DDRC3   3
#define DDRC2   2
#define DDRC1   1
#define DDRC0   0

#define PORTC   _SFR_IO8(0x15)
#define PORTC7  7
#define PORTC6  6
#define PORTC5  5
#define PORTC4  4
#define PORTC3  3
#define PORTC2  2
#define PORTC1  1
#define PORTC0  0

#define PINB    _SFR_IO8(0x16)
#define PINB7   7
#define PINB6   6
#define PINB5   5
#define PINB4   4
#define PINB3   3
#define PINB2   2
#define PINB1   1
#define PINB0   0

#define DDRB    _SFR_IO8(0x17)
#define DDRB7   7
#define DDRB6   6
#define DDRB5   5
#define DDRB4   4
#define DDRB3   3
#define DDRB2   2
#define DDRB1   1
#define DDRB0   0

#define PORTB   _SFR_IO8(0x18)
#define PORTB7  7
#define PORTB6  6
#define PORTB5  5
#define PORTB4  4
#define PORTB3  3
#define PORTB2  2
#define PORTB1  1
#define PORTB0  0

#define PINA    _SFR_IO8(0x19)
#define PINA7   7
#define PINA6   6
#define PINA5   5
#define PINA4   4
#define PINA3   3
#define PINA2   2
#define PINA1   1
#define PINA0   0

#define DDRA    _SFR_IO8(0x1A)
#define DDRA7   7
#define DDRA6   6
#define DDRA5   5
#define DDRA4   4
#define DDRA3   3
#define DDRA2   2
#define DDRA1   1
#define DDRA0   0

#define PORTA   _SFR_IO8(0x1B)
#define PORTA7  7
#define PORTA6  6
#define PORTA5  5
#define PORTA4  4
#define PORTA3  3
#define PORTA2  2
#define PORTA1  1
#define PORTA0  0

#define EECR    _SFR_IO8(0x1C)
#define EERE    0
#define EEWE    1
#define EEMWE   2
#define EERIE   3

#define EEDR    _SFR_IO8(0x1D)

/* Combine EEARL and EEARH */
#define EEAR    _SFR_IO16(0x1E)

#define EEARL   _SFR_IO8(0x1E)
#define EEARH   _SFR_IO8(0x1F)

#define UCSRC   _SFR_IO8(0x20)
#define UCPOL   0
#define UCSZ0   1
#define UCSZ1   2
#define USBS    3
#define UPM0    4
#define UPM1    5
#define UMSEL   6
#define URSEL   7

#define UBRRH   _SFR_IO8(0x20)

#define WDTCR   _SFR_IO8(0x21)
#define WDP0    0
#define WDP1    1
#define WDP2    2
#define WDE     3
#define WDTOE   4

#define ASSR    _SFR_IO8(0x22)
#define TCR2UB  0
#define OCR2UB  1
#define TCN2UB  2
#define AS2     3

#define OCR2    _SFR_IO8(0x23)

#define TCNT2   _SFR_IO8(0x24)

#define TCCR2   _SFR_IO8(0x25)
#define CS20    0
#define CS21    1
#define CS22    2
#define WGM21   3
#define COM20   4
#define COM21   5
#define WGM20   6
#define FOC2    7

/* Combine ICR1L and ICR1H */
#define ICR1    _SFR_IO16(0x26)

#define ICR1L   _SFR_IO8(0x26)
#define ICR1H   _SFR_IO8(0x27)

/* Combine OCR1BL and OCR1BH */
#define OCR1B   _SFR_IO16(0x28)

#define OCR1BL  _SFR_IO8(0x28)
#define OCR1BH  _SFR_IO8(0x29)

/* Combine OCR1AL and OCR1AH */
#define OCR1A   _SFR_IO16(0x2A)

#define OCR1AL  _SFR_IO8(0x2A)
#define OCR1AH  _SFR_IO8(0x2B)

/* Combine TCNT1L and TCNT1H */
#define TCNT1   _SFR_IO16(0x2C)

#define TCNT1L  _SFR_IO8(0x2C)
#define TCNT1H  _SFR_IO8(0x2D)

#define TCCR1B  _SFR_IO8(0x2E)
#define CS10    0
#define CS11    1
#define CS12    2
#define WGM12   3
#define WGM13   4
#define ICES1   6
#define ICNC1   7

#define TCCR1A  _SFR_IO8(0x2F)
#define WGM10   0
#define WGM11   1
#define FOC1B   2
#define FOC1A   3
#define COM1B0  4
#define COM1B1  5
#define COM1A0  6
#define COM1A1  7

#define SFIOR   _SFR_IO8(0x30)
#define PSR2    0
#define PSR10   0
#define PUD     2
#define ACME    3
#define ADTS0   5
#define ADTS1   6
#define ADTS2   7

#define OSCCAL  _SFR_IO8(0x31)
#define OSCCAL0 0
#define OSCCAL1 1
#define OSCCAL2 2
#define OSCCAL3 3
#define OSCCAL4 4
#define OSCCAL5 5
#define OSCCAL6 6
#define OSCCAL7 7

#define TCNT0   _SFR_IO8(0x32)

#define TCCR0   _SFR_IO8(0x33)
#define CS00    0
#define CS01    1
#define CS02    2
#define WGM01   3
#define COM00   4
#define COM01   5
#define WGM00   6
#define FOC0    7

#define MCUCSR  _SFR_IO8(0x34)
#define ISC2    6
#define PORF    0
#define EXTRF   1
#define BORF    2
#define WDRF    3
#define JTRF    4
#define JTD     7

#define MCUCR   _SFR_IO8(0x35)
#define ISC00   0
#define ISC01   1
#define ISC10   2
#define ISC11   3
#define SM0     4
#define SM1     5
#define SM2     6
#define SE      7

#define TWCR    _SFR_IO8(0x36)
#define TWIE    0
#define TWEN    2
#define TWWC    3
#define TWSTO   4
#define TWSTA   5
#define TWEA    6
#define TWINT   7

#define SPMCR   _SFR_IO8(0x37)
#define SPMEN   0
#define PGERS   1
#define PGWRT   2
#define BLBSET  3
#define RWWSRE  4
#define RWWSB   6
#define SPMIE   7

#define TIFR    _SFR_IO8(0x38)
#define TOV0    0
#define OCF0    1
#define TOV2    6
#define OCF2    7
#define TOV1    2
#define OCF1B   3
#define OCF1A   4
#define ICF1    5

#define TIMSK   _SFR_IO8(0x39)
#define TOIE0   0
#define OCIE0   1
#define TOIE2   6
#define OCIE2   7
#define TOIE1   2
#define OCIE1B  3
#define OCIE1A  4
#define TICIE1  5

#define GIFR    _SFR_IO8(0x3A)
#define INTF2   5
#define INTF0   6
#define INTF1   7

#define GICR    _SFR_IO8(0x3B)
#define IVCE    0
#define IVSEL   1
#define INT2    5
#define INT0    6
#define INT1    7

#define OCR0    _SFR_IO8(0x3C)

/* SP [0x3D..0x3E] */

/* SREG [0x3F] */



/* Interrupt vectors */
/* Vector 0 is the reset vector */
/* External Interrupt Request 0 */
#define INT0_vect            _VECTOR(1)
#define INT0_vect_num        1

/* External Interrupt Request 1 */
#define INT1_vect            _VECTOR(2)
#define INT1_vect_num        2

/* External Interrupt Request 2 */
#define INT2_vect            _VECTOR(3)
#define INT2_vect_num        3

/* Timer/Counter2 Compare Match */
#define TIMER2_COMP_vect            _VECTOR(4)
#define TIMER2_COMP_vect_num        4

/* Timer/Counter2 Overflow */
#define TIMER2_OVF_vect            _VECTOR(5)
#define TIMER2_OVF_vect_num        5

/* Timer/Counter1 Capture Event */
#define TIMER1_CAPT_vect            _VECTOR(6)
#define TIMER1_CAPT_vect_num        6

/* Timer/Counter1 Compare Match A */
#define TIMER1_COMPA_vect            _VECTOR(7)
#define TIMER1_COMPA_vect_num        7

/* Timer/Counter1 Compare Match B */
#define TIMER1_COMPB_vect            _VECTOR(8)
#define TIMER1_COMPB_vect_num        8

/* Timer/Counter1 Overflow */
#define TIMER1_OVF_vect            _VECTOR(9)
#define TIMER1_OVF_vect_num        9

/* Timer/Counter0 Compare Match */
#define TIMER0_COMP_vect            _VECTOR(10)
#define TIMER0_COMP_vect_num        10

/* Timer/Counter0 Overflow */
#define TIMER0_OVF_vect            _VECTOR(11)
#define TIMER0_OVF_vect_num        11

/* Serial Transfer Complete */
#define SPI_STC_vect            _VECTOR(12)
#define SPI_STC_vect_num        12

/* USART, Rx Complete */
#define USART_RXC_vect            _VECTOR(13)
#define USART_RXC_vect_num        13

/* USART Data Register Empty */
#define USART_UDRE_vect            _VECTOR(14)
#define USART_UDRE_vect_num        14

/* USART, Tx Complete */
#define USART_TXC_vect            _VECTOR(15)
#define USART_TXC_vect_num        15

/* ADC Conversion Complete */
#define ADC_vect            _VECTOR(16)
#define ADC_vect_num        16

/* EEPROM Ready */
#define EE_RDY_vect            _VECTOR(17)
#define EE_RDY_vect_num        17

/* Analog Comparator */
#define ANA_COMP_vect            _VECTOR(18)
#define ANA_COMP_vect_num        18

/* 2-wire Serial Interface */
#define TWI_vect            _VECTOR(19)
#define TWI_vect_num        19

/* Store Program Memory Ready */
#define SPM_RDY_vect            _VECTOR(20)
#define SPM_RDY_vect_num        20

#define _VECTORS_SIZE 84


/* Constants */

#define SPM_PAGESIZE 128
#define FLASHSTART   0x0000
#define FLASHEND     0x7FFF
#define RAMSTART     0x0060
#define RAMSIZE      2048
#define RAMEND       0x085F
#define E2START     0
#define E2SIZE      1024
#define E2PAGESIZE  4
#define E2END       0x03FF
#define XRAMEND      RAMEND


/* Fuses */

#define FUSE_MEMORY_SIZE 2

/* Low Fuse Byte */
#define FUSE_SUT_CKSEL0  (unsigned char)~_BV(0)
#define FUSE_SUT_CKSEL1  (unsigned char)~_BV(1)
#define FUSE_SUT_CKSEL2  (unsigned char)~_BV(2)
#define FUSE_SUT_CKSEL3  (unsigned char)~_BV(3)
#define FUSE_SUT_CKSEL4  (unsigned char)~_BV(4)
#define FUSE_SUT_CKSEL5  (unsigned char)~_BV(5)
#define FUSE_BODEN       (unsigned char)~_BV(6)
#define FUSE_BODLEVEL    (unsigned char)~_BV(7)

/* High Fuse Byte */
#define FUSE_BOOTRST     (unsigned char)~_BV(0)
#define FUSE_BOOTSZ0     (unsigned char)~_BV(1)
#define FUSE_BOOTSZ1     (unsigned char)~_BV(2)
#define FUSE_EESAVE      (unsigned char)~_BV(3)
#define FUSE_CKOPT       (unsigned char)~_BV(4)
#define FUSE_SPIEN       (unsigned char)~_BV(5)
#define FUSE_JTAGEN      (unsigned char)~_BV(6)
#define FUSE_OCDEN       (unsigned char)~_BV(7)


/* Lock Bits */
#define __LOCK_BITS_EXIST
#define __BOOT_LOCK_BITS_0_EXIST
#define __BOOT_LOCK_BITS_1_EXIST


/* Signature */
#define SIGNATURE_0 0x1E
#define SIGNATURE_1 0x95
#define SIGNATURE_2 0x02



#define SLEEP_MODE_IDLE (0x00<<4)
#define SLEEP_MODE_ADC (0x01<<4)
#define SLEEP_MODE_PWR_DOWN (0x02<<4)
#define SLEEP_MODE_PWR_SAVE (0x03<<4)
#define SLEEP_MODE_STANDBY (0x06<<4)
#define SLEEP_MODE_EXT_STANDBY (0x07<<4)

#endif /* #ifdef _AVR_ATMEGA32A_H_INCLUDED */

