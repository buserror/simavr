/*
    sim_mega32u4.c

    Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

     This file is part of simavr.

    simavr is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    simavr is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#define SIM_VECTOR_SIZE    4
#define SIM_MMCU        "atmega32u4"
#define SIM_CORENAME    mcu_mega32u4
#define USBRF 5 // missing in avr/iom32u4.h

#define PD7     7
#define PD6     6
#define PD5     5
#define PD4     4
#define PD3     3
#define PD2     2
#define PD1     1
#define PD0     0

#define PE6     6

#include "sim_avr.h"
#include "sim_core_declare.h"
#include "avr_eeprom.h"
#include "avr_flash.h"
#include "avr_watchdog.h"
#include "avr_extint.h"
#include "avr_ioport.h"
#include "avr_uart.h"
#include "avr_adc.h"
#include "avr_timer.h"
#include "avr_spi.h"
#include "avr_twi.h"
#include "avr_acomp.h"
#include "avr_usb.h"

void m32u4_init(struct avr_t * avr);
void m32u4_reset(struct avr_t * avr);

#define _AVR_IO_H_
#define __ASSEMBLER__
#ifndef __AVR_ATmega32u4__
#define __AVR_ATmega32u4__
#endif
#include "avr/iom32u4.h"

/*
 * ATmega32u4 definitions
 */
const struct mcu_t {
  avr_t            core;
  avr_eeprom_t     eeprom;
  avr_flash_t      selfprog;
  avr_watchdog_t   watchdog;
  avr_extint_t     extint;
  avr_ioport_t     portb, portc, portd, porte, portf;
  avr_uart_t       uart1;
  avr_acomp_t      acomp;
  avr_adc_t		   adc;
  avr_timer_t      timer0, timer1, timer3;
  avr_spi_t        spi;
  avr_twi_t		   twi;
  avr_usb_t        usb;
} mcu_mega32u4 = {
  .core = {
    .mmcu = "atmega32u4",
    DEFAULT_CORE(4),
    
    .init = m32u4_init,
    .reset = m32u4_reset,

    .rampz = RAMPZ,
  },
  AVR_EEPROM_DECLARE(EE_READY_vect),
  AVR_SELFPROG_DECLARE(SPMCSR, SPMEN, SPM_READY_vect),
  AVR_WATCHDOG_DECLARE(WDTCSR, WDT_vect),
  .extint = {
    AVR_EXTINT_MEGA_DECLARE(0, 'D', PD0, A),
    AVR_EXTINT_MEGA_DECLARE(1, 'D', PD1, A),
    AVR_EXTINT_MEGA_DECLARE(2, 'D', PD2, A),
    AVR_EXTINT_MEGA_DECLARE(3, 'D', PD3, A),
    AVR_EXTINT_MEGA_DECLARE(6, 'E', PE6, B),
  },
    .portb = {
        .name = 'B', .r_port = PORTB, .r_ddr = DDRB, .r_pin = PINB,
        .pcint = {
            .enable = AVR_IO_REGBIT(PCICR, PCIE0),
            .raised = AVR_IO_REGBIT(PCIFR, PCIF0),
            .vector = PCINT0_vect,
        },
        .r_pcint = PCMSK0,
    },
    AVR_IOPORT_DECLARE(c, 'C', C),
    AVR_IOPORT_DECLARE(d, 'D', D),
    AVR_IOPORT_DECLARE(e, 'E', E),
    AVR_IOPORT_DECLARE(f, 'F', F),

	AVR_UARTX_DECLARE(1, PRR1, PRUSART1),

	.acomp = {
		.mux_inputs = 8,
		.mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1),
				AVR_IO_REGBIT(ADMUX, MUX2) },
		.pradc = AVR_IO_REGBIT(PRR0, PRADC),
		.aden = AVR_IO_REGBIT(ADCSRA, ADEN),
		.acme = AVR_IO_REGBIT(ADCSRB, ACME),

		.r_acsr = ACSR,
		.acis = { AVR_IO_REGBIT(ACSR, ACIS0), AVR_IO_REGBIT(ACSR, ACIS1) },
		.acic = AVR_IO_REGBIT(ACSR, ACIC),
		.aco = AVR_IO_REGBIT(ACSR, ACO),
		.acbg = AVR_IO_REGBIT(ACSR, ACBG),
		.disabled = AVR_IO_REGBIT(ACSR, ACD),

		.timer_name = '1',

		.ac = {
			.enable = AVR_IO_REGBIT(ACSR, ACIE),
			.raised = AVR_IO_REGBIT(ACSR, ACI),
			.vector = ANALOG_COMP_vect,
		}
	},
	.adc = {
		.r_admux = ADMUX,
		.mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1),
					AVR_IO_REGBIT(ADMUX, MUX2), AVR_IO_REGBIT(ADMUX, MUX3),
					AVR_IO_REGBIT(ADMUX, MUX4),AVR_IO_REGBIT(ADCSRB, MUX5),},
		.ref = { AVR_IO_REGBIT(ADMUX, REFS0), AVR_IO_REGBIT(ADMUX, REFS1)},
		.ref_values = { [0] = ADC_VREF_AREF, [1] = ADC_VREF_AVCC,
			//[2] = Reserved: any constant to mark this?
			[3] = ADC_VREF_V256 },

		.adlar = AVR_IO_REGBIT(ADMUX, ADLAR),
		.r_adcsra = ADCSRA,
		.aden = AVR_IO_REGBIT(ADCSRA, ADEN),
		.adsc = AVR_IO_REGBIT(ADCSRA, ADSC),
		.adate = AVR_IO_REGBIT(ADCSRA, ADATE),
		.adps = { AVR_IO_REGBIT(ADCSRA, ADPS0), AVR_IO_REGBIT(ADCSRA, ADPS1), AVR_IO_REGBIT(ADCSRA, ADPS2),},

		.r_adch = ADCH,
		.r_adcl = ADCL,

		.r_adcsrb = ADCSRB,
		.adts = { AVR_IO_REGBIT(ADCSRB, ADTS0), AVR_IO_REGBIT(ADCSRB, ADTS1),
				  AVR_IO_REGBIT(ADCSRB, ADTS2), AVR_IO_REGBIT(ADCSRB, ADTS3),},
		.adts_op = {
			[0] = avr_adts_free_running,
			[1] = avr_adts_analog_comparator_0,
			[2] = avr_adts_external_interrupt_0,
			[3] = avr_adts_timer_0_compare_match_a,
			[4] = avr_adts_timer_0_overflow,
			[5] = avr_adts_timer_1_compare_match_b,
			[6] = avr_adts_timer_1_overflow,
			[7] = avr_adts_timer_1_capture_event,
			// follows 4 sources for unsupported fast timer4
		},

		.muxmode = {
			[0] = AVR_ADC_SINGLE(0), [1] = AVR_ADC_SINGLE(1),

			// Not available: 2 items
			[2] = AVR_ADC_DIFF(0, 0,  1), [3] = AVR_ADC_DIFF(0, 0,  1),

			[4] = AVR_ADC_SINGLE(4), [5] = AVR_ADC_SINGLE(5),
			[6] = AVR_ADC_SINGLE(6), [7] = AVR_ADC_SINGLE(7),

			// Not available: 1 item
			[ 8] = AVR_ADC_DIFF(0, 0,  10),

			[ 9] = AVR_ADC_DIFF(1, 0,  10),

			// Not available: 1 item
			[10] = AVR_ADC_DIFF(0, 0, 200),

			[11] = AVR_ADC_DIFF(1, 0, 200),

			// Not available: 4 items
			[12] = AVR_ADC_DIFF(0, 0,  1), [13] = AVR_ADC_DIFF(0, 0,  1),
			[14] = AVR_ADC_DIFF(0, 0,  1), [15] = AVR_ADC_DIFF(0, 0,  1),

			[16] = AVR_ADC_DIFF(0, 1,   1),

			// Not available: 3 items
			[17] = AVR_ADC_DIFF(0, 0,   1),
			[18] = AVR_ADC_DIFF(0, 0,   1),
			[19] = AVR_ADC_DIFF(0, 0,   1),

			[20] = AVR_ADC_DIFF(4, 1,   1), [21] = AVR_ADC_DIFF(5, 1,   1),
			[22] = AVR_ADC_DIFF(6, 1,   1), [23] = AVR_ADC_DIFF(7, 1,   1),

			// Not available: 6 items
			[24] = AVR_ADC_DIFF(0, 0,   1), [25] = AVR_ADC_DIFF(0, 0,   1),
			[26] = AVR_ADC_DIFF(0, 0,   1), [27] = AVR_ADC_DIFF(0, 0,   1),
			[28] = AVR_ADC_DIFF(0, 0,   1), [29] = AVR_ADC_DIFF(0, 0,   1),

			[30] = AVR_ADC_REF(1100),	// 1.1V
			[31] = AVR_ADC_REF(0),		// GND

			[32] = AVR_ADC_SINGLE( 8), [33] = AVR_ADC_SINGLE( 9),
			[34] = AVR_ADC_SINGLE(10), [35] = AVR_ADC_SINGLE(11),
			[36] = AVR_ADC_SINGLE(12), [37] = AVR_ADC_SINGLE(13),

			[38] = AVR_ADC_DIFF(1, 0, 40),
			[39] = AVR_ADC_TEMP(),

			[40] = AVR_ADC_DIFF(4, 0,   10), [41] = AVR_ADC_DIFF(5, 0,   10),
			[42] = AVR_ADC_DIFF(6, 0,   10), [43] = AVR_ADC_DIFF(7, 0,   10),
			[44] = AVR_ADC_DIFF(4, 1,   10), [45] = AVR_ADC_DIFF(5, 1,   10),
			[46] = AVR_ADC_DIFF(6, 1,   10), [47] = AVR_ADC_DIFF(7, 1,   10),

			[48] = AVR_ADC_DIFF(4, 0,   40), [49] = AVR_ADC_DIFF(5, 0,   40),
			[50] = AVR_ADC_DIFF(6, 0,   40), [51] = AVR_ADC_DIFF(7, 0,   40),
			[52] = AVR_ADC_DIFF(4, 1,   40), [53] = AVR_ADC_DIFF(5, 1,   40),
			[54] = AVR_ADC_DIFF(6, 1,   40), [55] = AVR_ADC_DIFF(7, 1,   40),

			[56] = AVR_ADC_DIFF(4, 0,   200), [57] = AVR_ADC_DIFF(5, 0,   200),
			[58] = AVR_ADC_DIFF(6, 0,   200), [59] = AVR_ADC_DIFF(7, 0,   200),
			[60] = AVR_ADC_DIFF(4, 1,   200), [61] = AVR_ADC_DIFF(5, 1,   200),
			[62] = AVR_ADC_DIFF(6, 1,   200), [63] = AVR_ADC_DIFF(7, 1,   200),
		},

		.adc = {
			.enable = AVR_IO_REGBIT(ADCSRA, ADIE),
			.raised = AVR_IO_REGBIT(ADCSRA, ADIF),
			.vector = ADC_vect,
		},
	},

	.timer0 = {
        .name = '0',
        .disabled = AVR_IO_REGBIT(PRR0, PRTIM0),
        .wgm = { AVR_IO_REGBIT(TCCR0A, WGM00), AVR_IO_REGBIT(TCCR0A, WGM01), AVR_IO_REGBIT(TCCR0B, WGM02) },
        .wgm_op = {
            [0] = AVR_TIMER_WGM_NORMAL8(),
            [2] = AVR_TIMER_WGM_CTC(),
            [3] = AVR_TIMER_WGM_FASTPWM8(),
            [7] = AVR_TIMER_WGM_OCPWM(),
        },
        .cs = { AVR_IO_REGBIT(TCCR0B, CS00), AVR_IO_REGBIT(TCCR0B, CS01), AVR_IO_REGBIT(TCCR0B, CS02) },
        .cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */ },

        .r_tcnt = TCNT0,

        .overflow = {
            .enable = AVR_IO_REGBIT(TIMSK0, TOIE0),
            .raised = AVR_IO_REGBIT(TIFR0, TOV0),
            .vector = TIMER0_OVF_vect,
        },
        .comp = {
            [AVR_TIMER_COMPA] = {
                .r_ocr = OCR0A,
                .com = AVR_IO_REGBITS(TCCR0A, COM0A0, 0x3),
                .com_pin = AVR_IO_REGBIT(PORTB, 7),
                .interrupt = {
                    .enable = AVR_IO_REGBIT(TIMSK0, OCIE0A),
                    .raised = AVR_IO_REGBIT(TIFR0, OCF0A),
                    .vector = TIMER0_COMPA_vect,
                },
            },
            [AVR_TIMER_COMPB] = {
                .r_ocr = OCR0B,
                .com = AVR_IO_REGBITS(TCCR0A, COM0B0, 0x3),
                .com_pin = AVR_IO_REGBIT(PORTD, 0),
                .interrupt = {
                    .enable = AVR_IO_REGBIT(TIMSK0, OCIE0B),
                    .raised = AVR_IO_REGBIT(TIFR0, OCF0B),
                    .vector = TIMER0_COMPB_vect,
                }
            }
        }
    },
    .timer1 = {
        .name = '1',
        .disabled = AVR_IO_REGBIT(PRR0, PRTIM1),
        .wgm = { AVR_IO_REGBIT(TCCR1A, WGM10), AVR_IO_REGBIT(TCCR1A, WGM11),
                    AVR_IO_REGBIT(TCCR1B, WGM12), AVR_IO_REGBIT(TCCR1B, WGM13) },
        .wgm_op = {
            [0] = AVR_TIMER_WGM_NORMAL16(),
            [1] = AVR_TIMER_WGM_FCPWM8(),
            [2] = AVR_TIMER_WGM_FCPWM9(),
            [3] = AVR_TIMER_WGM_FCPWM10(),
            [4] = AVR_TIMER_WGM_CTC(),
            [5] = AVR_TIMER_WGM_FASTPWM8(),
            [6] = AVR_TIMER_WGM_FASTPWM9(),
            [7] = AVR_TIMER_WGM_FASTPWM10(),
            [12] = AVR_TIMER_WGM_ICCTC(),
            [14] = AVR_TIMER_WGM_ICPWM(),
            [15] = AVR_TIMER_WGM_OCPWM(),
        },
        .cs = { AVR_IO_REGBIT(TCCR1B, CS10), AVR_IO_REGBIT(TCCR1B, CS11), AVR_IO_REGBIT(TCCR1B, CS12) },
        .cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */  /* External clock T1 is not handled */},

        .r_tcnt = TCNT1L,
        .r_tcnth = TCNT1H,
        .r_icr = ICR1L,
        .r_icrh = ICR1H,

        .ices = AVR_IO_REGBIT(TCCR1B, ICES1),
        .icp = AVR_IO_REGBIT(PORTD, 4),

        .overflow = {
            .enable = AVR_IO_REGBIT(TIMSK1, TOIE1),
            .raised = AVR_IO_REGBIT(TIFR1, TOV1),
            .vector = TIMER1_OVF_vect,
        },
        .icr = {
            .enable = AVR_IO_REGBIT(TIMSK1, ICIE1),
            .raised = AVR_IO_REGBIT(TIFR1, ICF1),
            .vector = TIMER1_CAPT_vect,
        },
        .comp = {
            [AVR_TIMER_COMPA] = {
                .r_ocr = OCR1AL,
                .r_ocrh = OCR1AH,    // 16 bits timers have two bytes of it
                .com = AVR_IO_REGBITS(TCCR1A, COM1A0, 0x3),
                .com_pin = AVR_IO_REGBIT(PORTB, 5),
                .interrupt = {
                    .enable = AVR_IO_REGBIT(TIMSK1, OCIE1A),
                    .raised = AVR_IO_REGBIT(TIFR1, OCF1A),
                    .vector = TIMER1_COMPA_vect,
                }
            },
            [AVR_TIMER_COMPB] = {
                .r_ocr = OCR1BL,
                .r_ocrh = OCR1BH,
                .com = AVR_IO_REGBITS(TCCR1A, COM1B0, 0x3),
                .com_pin = AVR_IO_REGBIT(PORTB, 6),
                .interrupt = {
                    .enable = AVR_IO_REGBIT(TIMSK1, OCIE1B),
                    .raised = AVR_IO_REGBIT(TIFR1, OCF1B),
                    .vector = TIMER1_COMPB_vect,
                }
            }
        }
    },
    .timer3 = {
        .name = '3',
        .disabled = AVR_IO_REGBIT(PRR1, PRTIM3),
        .wgm = { AVR_IO_REGBIT(TCCR3A, WGM30), AVR_IO_REGBIT(TCCR3A, WGM31),
                    AVR_IO_REGBIT(TCCR3B, WGM32), AVR_IO_REGBIT(TCCR3B, WGM33) },
        .wgm_op = {
            [0] = AVR_TIMER_WGM_NORMAL16(),
            [1] = AVR_TIMER_WGM_FCPWM8(),
            [2] = AVR_TIMER_WGM_FCPWM9(),
            [3] = AVR_TIMER_WGM_FCPWM10(),
            [4] = AVR_TIMER_WGM_CTC(),
            [5] = AVR_TIMER_WGM_FASTPWM8(),
            [6] = AVR_TIMER_WGM_FASTPWM9(),
            [7] = AVR_TIMER_WGM_FASTPWM10(),
            [12] = AVR_TIMER_WGM_ICCTC(),
            [14] = AVR_TIMER_WGM_ICPWM(),
            [15] = AVR_TIMER_WGM_OCPWM(),
        },
        .cs = { AVR_IO_REGBIT(TCCR3B, CS30), AVR_IO_REGBIT(TCCR3B, CS31), AVR_IO_REGBIT(TCCR3B, CS32) },
        .cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */  /* External clock T1 is not handled */},

        .r_tcnt = TCNT3L,
        .r_tcnth = TCNT3H,
        .r_icr = ICR3L,
        .r_icrh = ICR3H,

        .ices = AVR_IO_REGBIT(TCCR3B, ICES3),
        .icp = AVR_IO_REGBIT(PORTC, 7),

        .overflow = {
            .enable = AVR_IO_REGBIT(TIMSK3, TOIE3),
            .raised = AVR_IO_REGBIT(TIFR3, TOV3),
            .vector = TIMER3_OVF_vect,
        },
        .icr = {
            .enable = AVR_IO_REGBIT(TIMSK3, ICIE3),
            .raised = AVR_IO_REGBIT(TIFR3, ICF3),
            .vector = TIMER3_CAPT_vect,
        },
        .comp = {
            [AVR_TIMER_COMPA] = {
                .r_ocr = OCR3AL,
                .r_ocrh = OCR3AH,    // 16 bits timers have two bytes of it
                .com = AVR_IO_REGBITS(TCCR3A, COM3A0, 0x3),
                .com_pin = AVR_IO_REGBIT(PORTC, 6),
                .interrupt = {
                    .enable = AVR_IO_REGBIT(TIMSK3, OCIE3A),
                    .raised = AVR_IO_REGBIT(TIFR3, OCF3A),
                    .vector = TIMER3_COMPA_vect,
                }
            },
            [AVR_TIMER_COMPB] = {
                .r_ocr = OCR3BL,
                .r_ocrh = OCR3BH,
                .com = AVR_IO_REGBITS(TCCR3A, COM3B0, 0x3),
                .com_pin = AVR_IO_REGBIT(PORTC, 6), // WTF nothing in doc about this
                .interrupt = {
                    .enable = AVR_IO_REGBIT(TIMSK3, OCIE3B),
                    .raised = AVR_IO_REGBIT(TIFR3, OCF3B),
                    .vector = TIMER3_COMPB_vect,
                }
            }
        }
    },
  //  .timer4 = { /* TODO 10 bits realtime timer */ },
	AVR_SPI_DECLARE(PRR0, PRSPI, 'B', 1, 3, 2, 0),
	.twi = {

		.r_twcr = TWCR,
		.r_twsr = TWSR,
		.r_twbr = TWBR,
		.r_twdr = TWDR,
		.r_twar = TWAR,
		.r_twamr = TWAMR,

		.twen = AVR_IO_REGBIT(TWCR, TWEN),
		.twea = AVR_IO_REGBIT(TWCR, TWEA),
		.twsta = AVR_IO_REGBIT(TWCR, TWSTA),
		.twsto = AVR_IO_REGBIT(TWCR, TWSTO),
		.twwc = AVR_IO_REGBIT(TWCR, TWWC),

		.twsr = AVR_IO_REGBITS(TWSR, TWS3, 0x1f),	// 5 bits
		.twps = AVR_IO_REGBITS(TWSR, TWPS0, 0x3),	// 2 bits

		.twi = {
			.enable = AVR_IO_REGBIT(TWCR, TWIE),
			.raised = AVR_IO_REGBIT(TWCR, TWINT),
			.raise_sticky = 1,
			.vector = TWI_vect,
		},
	},
	.usb = {
        .name='1',
        .disabled = AVR_IO_REGBIT(PRR1, PRUSB), // bit in the PRR

        .usbrf = AVR_IO_REGBIT(MCUSR, USBRF),    // bit in the MCUSR

        .r_usbcon = USBCON,
        .r_pllcsr = PLLCSR,

        .usb_com_vect = USB_COM_vect,
        .usb_gen_vect = USB_GEN_vect,
    },
};


static avr_t * make()
{
    return avr_core_allocate(&mcu_mega32u4.core, sizeof(struct mcu_t));
}

avr_kind_t mega32u4 = {
    .names = { "atmega32u4", },
    .make = make
};

void m32u4_init(struct avr_t * avr)
{
    struct mcu_t * mcu = (struct mcu_t*)avr;

    avr_eeprom_init(avr, &mcu->eeprom);
    avr_flash_init(avr, &mcu->selfprog);
    avr_extint_init(avr, &mcu->extint);
    avr_watchdog_init(avr, &mcu->watchdog);
    avr_ioport_init(avr, &mcu->portb);
    avr_ioport_init(avr, &mcu->portc);
    avr_ioport_init(avr, &mcu->portd);
    avr_ioport_init(avr, &mcu->porte);
    avr_ioport_init(avr, &mcu->portf);
	avr_uart_init(avr, &mcu->uart1);
	avr_acomp_init(avr, &mcu->acomp);
	avr_adc_init(avr, &mcu->adc);
	avr_timer_init(avr, &mcu->timer0);
    avr_timer_init(avr, &mcu->timer1);
    avr_timer_init(avr, &mcu->timer3);
    avr_spi_init(avr, &mcu->spi);
	avr_twi_init(avr, &mcu->twi);
	avr_usb_init(avr, &mcu->usb);
}

void m32u4_reset(struct avr_t * avr)
{
//    struct mcu_t * mcu = (struct mcu_t*)avr;
}
