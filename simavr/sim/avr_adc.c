/*
	avr_adc.c

	Copyright 2008, 2010 Michel Pollet <buserror@gmail.com>

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_time.h"
#include "avr_adc.h"

static avr_cycle_count_t
avr_adc_int_raise(
		struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_adc_t * p = (avr_adc_t *)param;
	if (avr_regbit_get(avr, p->aden)) {
		// if the interrupts are not used, still raised the UDRE and TXC flag
		avr_raise_interrupt(avr, &p->adc);
		avr_regbit_clear(avr, p->adsc);
		p->first = 0;
		p->read_status = 0;
		if( p->adts_mode == avr_adts_free_running )
			avr_raise_irq(p->io.irq + ADC_IRQ_IN_TRIGGER, 1);
	}
	return 0;
}

static uint8_t
avr_adc_read_l(
		struct avr_t * avr, avr_io_addr_t addr, void * param)
{
	avr_adc_t * p = (avr_adc_t *)param;

	if (p->read_status)	// conversion already done
		return avr_core_watch_read(avr, addr);

	uint8_t refi = avr_regbit_get_array(avr, p->ref, ARRAY_SIZE(p->ref));
	uint16_t ref = p->ref_values[refi];
	uint8_t muxi = avr_regbit_get_array(avr, p->mux, ARRAY_SIZE(p->mux));
	avr_adc_mux_t mux = p->muxmode[muxi];
	// optional shift left/right
	uint8_t shift = avr_regbit_get(avr, p->adlar) ? 6 : 0; // shift LEFT

	uint32_t reg = 0;
	switch (mux.kind) {
		case ADC_MUX_SINGLE:
			reg = p->adc_values[mux.src];
			break;
		case ADC_MUX_DIFF:
			if (mux.gain == 0)
				mux.gain = 1;
			reg = ((uint32_t)p->adc_values[mux.src] * mux.gain) -
					((uint32_t)p->adc_values[mux.diff] * mux.gain);
			break;
		case ADC_MUX_TEMP:
			reg = p->temp; // assumed to be already calibrated somehow
			break;
		case ADC_MUX_REF:
			reg = mux.src; // reference voltage
			break;
		case ADC_MUX_VCC4:
			if ( !avr->vcc) {
				AVR_LOG(avr, LOG_WARNING, "ADC: missing VCC analog voltage\n");
			} else
				reg = avr->vcc / 4;
			break;
	}
	uint32_t vref = 3300;
	switch (ref) {
		case ADC_VREF_VCC:
			if (!avr->vcc)
				AVR_LOG(avr, LOG_WARNING, "ADC: missing VCC analog voltage\n");
			else
				vref = avr->vcc;
			break;
		case ADC_VREF_AREF:
			if (!avr->aref)
				AVR_LOG(avr, LOG_WARNING, "ADC: missing AREF analog voltage\n");
			else
				vref = avr->aref;
			break;
		case ADC_VREF_AVCC:
			if (!avr->avcc)
				AVR_LOG(avr, LOG_WARNING, "ADC: missing AVCC analog voltage\n");
			else
				vref = avr->avcc;
			break;
		default:
			vref = ref;
	}
//	printf("ADCL %d:%3d:%3d read %4d vref %d:%d=%d\n",
//			mux.kind, mux.diff, mux.src,
//			reg, refi, ref, vref);
	reg = (reg * 0x3ff) / vref;	// scale to 10 bits ADC
//	printf("ADC to 10 bits 0x%x %d\n", reg, reg);
	if (reg > 0x3ff) {
		AVR_LOG(avr, LOG_WARNING, "ADC: channel %d clipped %u/%u VREF %d\n", mux.kind, reg, 0x3ff, vref);
		reg = 0x3ff;
	}
	reg <<= shift;
//	printf("ADC to 10 bits %x shifted %d\n", reg, shift);
	avr->data[p->r_adcl] = reg;
	avr->data[p->r_adch] = reg >> 8;
	p->read_status = 1;
	return avr_core_watch_read(avr, addr);
}

/*
 * From Datasheet:
 * "When ADCL is read, the ADC Data Register is not updated until ADCH is read.
 * Consequently, if the result is left adjusted and no more than 8-bit
 * precision is required, it is sufficient to read ADCH.
 * Otherwise, ADCL must be read first, then ADCH."
 * So here if the H is read before the L, we still call the L to update the
 * register value.
 */
static uint8_t
avr_adc_read_h(
		struct avr_t * avr, avr_io_addr_t addr, void * param)
{
	avr_adc_t * p = (avr_adc_t *)param;
	// no "break" here on purpose
	switch (p->read_status) {
		case 0:
			avr_adc_read_l(avr, p->r_adcl, param);
			FALLTHROUGH
		case 1:
			p->read_status = 2;
			FALLTHROUGH
		default:
			return avr_core_watch_read(avr, addr);
	}
}

static void
avr_adc_configure_trigger(
		struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_adc_t * p = (avr_adc_t *)param;
	
	uint8_t adate = avr_regbit_get(avr, p->adate);
	uint8_t old_adts = p->adts_mode;
	
	static char * auto_trigger_names[] = {
		"none",
		"free_running",
		"analog_comparator_0",
		"analog_comparator_1",
		"analog_comparator_2",
		"analog_comparator_3",
		"external_interrupt_0",
		"timer_0_compare_match_a",
		"timer_0_compare_match_b",
		"timer_0_overflow",
		"timer_1_compare_match_b",
		"timer_1_overflow",
		"timer_1_capture_event",
		"pin_change_interrupt",
		"psc_module_0_sync_signal",
		"psc_module_1_sync_signal",
		"psc_module_2_sync_signal",
	};
	
	if( adate ) {
		uint8_t adts = avr_regbit_get_array(avr, p->adts, ARRAY_SIZE(p->adts));
		p->adts_mode = p->adts_op[adts];
		
		switch(p->adts_mode) {
			case avr_adts_free_running: {
				// do nothing at free running mode
			}	break;
			// TODO: implement the other auto trigger modes
			default: {
				AVR_LOG(avr, LOG_WARNING,
						"ADC: unimplemented auto trigger mode: %s\n",
						auto_trigger_names[p->adts_mode]);
				p->adts_mode = avr_adts_none;
			}	break;
		}
	} else {
		// TODO: remove previously configured auto triggers
		p->adts_mode = avr_adts_none;
	}
	
	if( old_adts != p->adts_mode )
		AVR_LOG(avr, LOG_TRACE, "ADC: auto trigger configured: %s\n",
				auto_trigger_names[p->adts_mode]);
}

static void
avr_adc_write_adcsra(
		struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	
	avr_adc_t * p = (avr_adc_t *)param;
	uint8_t adsc = avr_regbit_get(avr, p->adsc);
	uint8_t aden = avr_regbit_get(avr, p->aden);

	avr->data[p->adsc.reg] = v;

	// can't write zero to adsc
	if (adsc && !avr_regbit_get(avr, p->adsc)) {
		avr_regbit_set(avr, p->adsc);
		v = avr->data[p->adsc.reg];
	}
	if (!aden && avr_regbit_get(avr, p->aden)) {
		// first conversion
		p->first = 1;
		AVR_LOG(avr, LOG_TRACE, "ADC: Start AREF %d AVCC %d\n", avr->aref, avr->avcc);
	}
	if (aden && !avr_regbit_get(avr, p->aden)) {
		// stop ADC
		avr_cycle_timer_cancel(avr, avr_adc_int_raise, p);
		avr_regbit_clear(avr, p->adsc);
		v = avr->data[p->adsc.reg];	// Peter Ross pross@xvid.org
	}
	if (!adsc && avr_regbit_get(avr, p->adsc)) {
		// start one!
		uint8_t muxi = avr_regbit_get_array(avr, p->mux, ARRAY_SIZE(p->mux));
		union {
			avr_adc_mux_t mux;
			uint32_t v;
		} e = { .mux = p->muxmode[muxi] };
		avr_raise_irq(p->io.irq + ADC_IRQ_OUT_TRIGGER, e.v);

		// clock prescaler are just a bit shift.. and 0 means 1
		uint32_t div = avr_regbit_get_array(avr, p->adps, ARRAY_SIZE(p->adps));
		if (!div) div++;

		div = avr->frequency >> div;
		if (p->first)
			AVR_LOG(avr, LOG_TRACE, "ADC: starting at %uKHz\n", div / 13 / 100);
		div /= p->first ? 25 : 13;	// first cycle is longer

		avr_cycle_timer_register(avr,
				avr_hz_to_cycles(avr, div),
				avr_adc_int_raise, p);
	}
	avr_core_watch_write(avr, addr, v);
	avr_adc_configure_trigger(avr, addr, v, param);
}

static void
avr_adc_write_adcsrb(
		struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_core_watch_write(avr, addr, v);
	avr_adc_configure_trigger(avr, addr, v, param);
}

static void
avr_adc_irq_notify(
		struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_adc_t * p = (avr_adc_t *)param;
	avr_t * avr = p->io.avr;

	switch (irq->irq) {
		case ADC_IRQ_ADC0 ... ADC_IRQ_ADC7: {
			p->adc_values[irq->irq] = value;
		} 	break;
		case ADC_IRQ_TEMP: {
			p->temp = value;
		}	break;
		case ADC_IRQ_IN_TRIGGER: {
			if (avr_regbit_get(avr, p->adate)) {
				// start a conversion only if it's not running
				// otherwise ignore the trigger
				if(!avr_regbit_get(avr, p->adsc) ) {
			  		uint8_t addr = p->adsc.reg;
					if (addr) {
						uint8_t val = avr->data[addr] | (1 << p->adsc.bit);
						// write ADSC to ADCSRA
						avr_adc_write_adcsra(avr, addr, val, param);
					}
				}
			}
		}	break;
	}
}

static void avr_adc_reset(avr_io_t * port)
{
	avr_adc_t * p = (avr_adc_t *)port;

	// stop ADC
	avr_cycle_timer_cancel(p->io.avr, avr_adc_int_raise, p);
	avr_regbit_clear(p->io.avr, p->adsc);

	for (int i = 0; i < ADC_IRQ_COUNT; i++)
		avr_irq_register_notify(p->io.irq + i, avr_adc_irq_notify, p);
}

static const char * irq_names[ADC_IRQ_COUNT] = {
	[ADC_IRQ_ADC0] = "16<adc0",
	[ADC_IRQ_ADC1] = "16<adc1",
	[ADC_IRQ_ADC2] = "16<adc2",
	[ADC_IRQ_ADC3] = "16<adc3",
	[ADC_IRQ_ADC4] = "16<adc4",
	[ADC_IRQ_ADC5] = "16<adc5",
	[ADC_IRQ_ADC6] = "16<adc6",
	[ADC_IRQ_ADC7] = "16<adc7",
	[ADC_IRQ_ADC8] = "16<adc0",
	[ADC_IRQ_ADC9] = "16<adc9",
	[ADC_IRQ_ADC10] = "16<adc10",
	[ADC_IRQ_ADC11] = "16<adc11",
	[ADC_IRQ_ADC12] = "16<adc12",
	[ADC_IRQ_ADC13] = "16<adc13",
	[ADC_IRQ_ADC14] = "16<adc14",
	[ADC_IRQ_ADC15] = "16<adc15",
	[ADC_IRQ_TEMP] = "16<temp",
	[ADC_IRQ_IN_TRIGGER] = "<trigger_in",
	[ADC_IRQ_OUT_TRIGGER] = ">trigger_out",
};

static	avr_io_t	_io = {
	.kind = "adc",
	.reset = avr_adc_reset,
	.irq_names = irq_names,
};

void avr_adc_init(avr_t * avr, avr_adc_t * p)
{
	p->io = _io;

	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->adc);
	// allocate this module's IRQ
	avr_io_setirqs(&p->io, AVR_IOCTL_ADC_GETIRQ, ADC_IRQ_COUNT, NULL);

	avr_register_io_write(avr, p->r_adcsra, avr_adc_write_adcsra, p);
	// some ADCs don't have ADCSRB (atmega8/16/32)
	if (p->r_adcsrb)
		avr_register_io_write(avr, p->r_adcsrb, avr_adc_write_adcsrb, p);
	avr_register_io_read(avr, p->r_adcl, avr_adc_read_l, p);
	avr_register_io_read(avr, p->r_adch, avr_adc_read_h, p);
}
