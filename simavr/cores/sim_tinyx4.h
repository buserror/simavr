/*
    sim_tinyx4.h

    Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
                         Jon Escombe <lists@dresco.co.uk>

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


#ifndef __SIM_TINYX4_H__
#define __SIM_TINYX4_H__

#include "sim_core_declare.h"
#include "avr_eeprom.h"
#include "avr_watchdog.h"
#include "avr_extint.h"
#include "avr_ioport.h"
#include "avr_adc.h"
#include "avr_timer.h"

void tx4_init(struct avr_t * avr);
void tx4_reset(struct avr_t * avr);

/*
 * This is a template for all of the tinyx4 devices, hopefully
 */
struct mcu_t {
    avr_t core;
    avr_eeprom_t     eeprom;
    avr_watchdog_t    watchdog;
    avr_extint_t    extint;
    avr_ioport_t    porta, portb;
    avr_adc_t        adc;
    avr_timer_t    timer0, timer1;
};

#ifdef SIM_CORENAME

#ifndef SIM_VECTOR_SIZE
#error SIM_VECTOR_SIZE is not declared
#endif
#ifndef SIM_MMCU
#error SIM_MMCU is not declared
#endif

const struct mcu_t SIM_CORENAME = {
    .core = {
        .mmcu = SIM_MMCU,
        DEFAULT_CORE(SIM_VECTOR_SIZE),

        .init = tx4_init,
        .reset = tx4_reset,
    },
    AVR_EEPROM_DECLARE(EE_RDY_vect),
    AVR_WATCHDOG_DECLARE(WDTCSR, WDT_vect),
    .extint = {
        AVR_EXTINT_TINY_DECLARE(0, 'B', PB2, GIFR),
    },
    .porta = {
        .name = 'A',  .r_port = PORTA, .r_ddr = DDRA, .r_pin = PINA,
        .pcint = {
            .enable = AVR_IO_REGBIT(GIMSK, PCIE0),
            .raised = AVR_IO_REGBIT(GIFR, PCIF0),
            .vector = PCINT0_vect,
        },
        .r_pcint = PCMSK0,
    },
    .portb = {
        .name = 'B',  .r_port = PORTB, .r_ddr = DDRB, .r_pin = PINB,
        .pcint = {
            .enable = AVR_IO_REGBIT(GIMSK, PCIE1),
            .raised = AVR_IO_REGBIT(GIFR, PCIF1),
            .vector = PCINT1_vect,
        },
        .r_pcint = PCMSK1,
    },
    .adc = {
        .r_admux = ADMUX,
        .mux = { AVR_IO_REGBIT(ADMUX, MUX0), AVR_IO_REGBIT(ADMUX, MUX1),
                    AVR_IO_REGBIT(ADMUX, MUX2), AVR_IO_REGBIT(ADMUX, MUX3),},
        .ref = { AVR_IO_REGBIT(ADMUX, REFS0), AVR_IO_REGBIT(ADMUX, REFS1), },
        .ref_values = {
                [0] = ADC_VREF_VCC, [1] = ADC_VREF_AREF,
                [2] = ADC_VREF_V110,
        },

        .adlar = AVR_IO_REGBIT(ADMUX, ADLAR),
        .r_adcsra = ADCSRA,
        .aden = AVR_IO_REGBIT(ADCSRA, ADEN),
        .adsc = AVR_IO_REGBIT(ADCSRA, ADSC),
        .adate = AVR_IO_REGBIT(ADCSRA, ADATE),
        .adps = { AVR_IO_REGBIT(ADCSRA, ADPS0), AVR_IO_REGBIT(ADCSRA, ADPS1), AVR_IO_REGBIT(ADCSRA, ADPS2),},

        .r_adch = ADCH,
        .r_adcl = ADCL,

        .r_adcsrb = ADCSRB,
        .adts = { AVR_IO_REGBIT(ADCSRB, ADTS0), AVR_IO_REGBIT(ADCSRB, ADTS1), AVR_IO_REGBIT(ADCSRB, ADTS2),},
        .adts_op = {
          [0] = avr_adts_free_running,
          [1] = avr_adts_analog_comparator_0,
          [2] = avr_adts_external_interrupt_0,
          [3] = avr_adts_timer_0_compare_match_a,
          [4] = avr_adts_timer_0_overflow,
          [5] = avr_adts_timer_1_compare_match_b,
          [6] = avr_adts_timer_1_overflow,
          [7] = avr_adts_timer_1_capture_event,
        },
        .bin = AVR_IO_REGBIT(ADCSRB, BIN),

        .muxmode = {
            [0] = AVR_ADC_SINGLE(0), [1] = AVR_ADC_SINGLE(1),
            [2] = AVR_ADC_SINGLE(2), [3] = AVR_ADC_SINGLE(3),

            [ 4] = AVR_ADC_DIFF(2, 2,   1), [ 5] = AVR_ADC_DIFF(2, 2,  20),
            [ 6] = AVR_ADC_DIFF(2, 3,   1), [ 7] = AVR_ADC_DIFF(2, 3,  20),
            [ 8] = AVR_ADC_DIFF(0, 0,   1), [ 9] = AVR_ADC_DIFF(0, 0,  20),
            [10] = AVR_ADC_DIFF(0, 1,   1), [11] = AVR_ADC_DIFF(0, 1,  20),
            [12] = AVR_ADC_REF(1100),    // Vbg
            [13] = AVR_ADC_REF(0),        // GND
            [15] = AVR_ADC_TEMP(),
        },

        .adc = {
            .enable = AVR_IO_REGBIT(ADCSRA, ADIE),
            .raised = AVR_IO_REGBIT(ADCSRA, ADIF),
            .vector = ADC_vect,
        },
    },
    .timer0 = {
        .name = '0',
        .wgm = { AVR_IO_REGBIT(TCCR0A, WGM00), AVR_IO_REGBIT(TCCR0A, WGM01), AVR_IO_REGBIT(TCCR0B, WGM02) },
        .wgm_op = {
            [0] = AVR_TIMER_WGM_NORMAL8(),
            [2] = AVR_TIMER_WGM_CTC(),
            [3] = AVR_TIMER_WGM_FASTPWM8(),
            [7] = AVR_TIMER_WGM_OCPWM(),
        },
        .cs = { AVR_IO_REGBIT(TCCR0B, CS00), AVR_IO_REGBIT(TCCR0B, CS01), AVR_IO_REGBIT(TCCR0B, CS02) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */, AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE },
		.ext_clock_pin = AVR_IO_REGBIT(PORTA, 3), /* External clock pin */

        .r_tcnt = TCNT0,

        .overflow = {
            .enable = AVR_IO_REGBIT(TIMSK0, TOIE0),
            .raised = AVR_IO_REGBIT(TIFR0, TOV0),
            .vector = TIM0_OVF_vect,
        },
        .comp = {
            [AVR_TIMER_COMPA] = {
                .r_ocr = OCR0A,
                .com = AVR_IO_REGBITS(TCCR0A, COM0A0, 0x3),
                .com_pin = AVR_IO_REGBIT(PORTB, 0),
                .interrupt = {
                    .enable = AVR_IO_REGBIT(TIMSK0, OCIE0A),
                    .raised = AVR_IO_REGBIT(TIFR0, OCF0A),
                    .vector = TIM0_COMPA_vect,
                },
            },
            [AVR_TIMER_COMPB] = {
                .r_ocr = OCR0B,
                .com = AVR_IO_REGBITS(TCCR0A, COM0B0, 0x3),
                .com_pin = AVR_IO_REGBIT(PORTB, 1),
                .interrupt = {
                    .enable = AVR_IO_REGBIT(TIMSK0, OCIE0B),
                    .raised = AVR_IO_REGBIT(TIFR0, OCF0B),
                    .vector = TIM0_COMPB_vect,
                },
            },
        },
    },
    .timer1 = {
        .name = '1',
        .disabled = AVR_IO_REGBIT(PRR,PRTIM1),
        .wgm = { AVR_IO_REGBIT(TCCR1A, WGM10), AVR_IO_REGBIT(TCCR1A, WGM11),
                    AVR_IO_REGBIT(TCCR1B, WGM12), AVR_IO_REGBIT(TCCR1B, WGM13) },
        .wgm_op = {
            [0] = AVR_TIMER_WGM_NORMAL16(),
            [4] = AVR_TIMER_WGM_CTC(),
            [5] = AVR_TIMER_WGM_FASTPWM8(),
            [6] = AVR_TIMER_WGM_FASTPWM9(),
            [7] = AVR_TIMER_WGM_FASTPWM10(),
            [12] = AVR_TIMER_WGM_ICCTC(),
            [14] = AVR_TIMER_WGM_ICPWM(),
            [15] = AVR_TIMER_WGM_OCPWM(),
        },
        .cs = { AVR_IO_REGBIT(TCCR1B, CS10), AVR_IO_REGBIT(TCCR1B, CS11), AVR_IO_REGBIT(TCCR1B, CS12) },
		.cs_div = { 0, 0, 3 /* 8 */, 6 /* 64 */, 8 /* 256 */, 10 /* 1024 */, AVR_TIMER_EXTCLK_CHOOSE, AVR_TIMER_EXTCLK_CHOOSE },
		.ext_clock_pin = AVR_IO_REGBIT(PORTA, 4), /* External clock pin */

        .r_tcnt = TCNT1L,
        .r_tcnth = TCNT1H,
        .r_icr = ICR1L,
        .r_icrh = ICR1H,

        .ices = AVR_IO_REGBIT(TCCR1B, ICES1),
        .icp = AVR_IO_REGBIT(PORTB, 0),

        .overflow = {
            .enable = AVR_IO_REGBIT(TIMSK1, TOIE1),
            .raised = AVR_IO_REGBIT(TIFR1, TOV1),
            .vector = TIM1_OVF_vect,
        },
        .icr = {
            .enable = AVR_IO_REGBIT(TIMSK1, ICIE1),
            .raised = AVR_IO_REGBIT(TIFR1, ICF1),
            .vector = TIM1_CAPT_vect,
        },
        .comp = {
            [AVR_TIMER_COMPA] = {
                .r_ocr = OCR1AL,
                .r_ocrh = OCR1AH,    // 16 bits timers have two bytes of it
                .com = AVR_IO_REGBITS(TCCR1A, COM1A0, 0x3),
                .com_pin = AVR_IO_REGBIT(PORTB, 1),
                .interrupt = {
                    .enable = AVR_IO_REGBIT(TIMSK1, OCIE1A),
                    .raised = AVR_IO_REGBIT(TIFR1, OCF1A),
                    .vector = TIM1_COMPA_vect,
                },
            },
            [AVR_TIMER_COMPB] = {
                .r_ocr = OCR1BL,
                .r_ocrh = OCR1BH,
                .com = AVR_IO_REGBITS(TCCR1A, COM1B0, 0x3),
                .com_pin = AVR_IO_REGBIT(PORTB, 2),
                .interrupt = {
                    .enable = AVR_IO_REGBIT(TIMSK1, OCIE1B),
                    .raised = AVR_IO_REGBIT(TIFR1, OCF1B),
                    .vector = TIM1_COMPB_vect,
                },
            },
        },
    },
};
#endif /* SIM_CORENAME */

#endif /* __SIM_TINYX4_H__ */
