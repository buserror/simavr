/*
	avr_bitbang.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
			  2011 Stephan Veigl <veig@gmx.net>

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

#ifndef __AVR_BITBANG_H__
#define __AVR_BITBANG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>

#include "avr_bitbang.h"

#include "sim_regbit.h"
#include "sim_core.h"
#include "avr_ioport.h"

///@todo refactor SPI to bitbang

#define BITBANG_MASK	0xFFFFFFFFUL

/**
 * read (sample) data from input pin
 *
 * @param p		internal bitbang structure
 */
static void avr_bitbang_read_bit(avr_bitbang_t *p)
{
	avr_ioport_state_t iostate;
	uint8_t bit = 0;

	if ( !p->enabled )
		return;

	// read from HW pin
	if ( p->p_in.port ) {
		avr_ioctl(p->avr, AVR_IOCTL_IOPORT_GETSTATE( p->p_in.port ), &iostate);
		bit = ( iostate.pin >> p->p_in.pin ) & 1;

		if ( p->data_order ) {
			// data order: shift right
			p->data = (p->data >> 1) | ( bit << (p->buffer_size-1));
		} else {
			// data order: shift left
			p->data = (p->data << 1) | bit;
		}

	}

	// module callback
	if ( p->callback_bit_read ) {
		p->callback_bit_read(bit, p->callback_param);
	}

	// data sanitary
	p->data = p->data & ~(BITBANG_MASK << p->buffer_size);
}

/**
 * write data to output pin
 *
 * @param p		bitbang structure
 */
static void avr_bitbang_write_bit(avr_bitbang_t *p)
{
	uint8_t	bit = 0;

	if ( !p->enabled )
		return;

	if ( p->data_order ) {
		// data order: shift right
		bit = p->data & 1;
	} else {
		// data order: shift left
		bit = (p->data >> (p->buffer_size-1)) & 1;
	}

	// output to HW pin
	if ( p->p_out.port ) {
		avr_raise_irq(avr_io_getirq(p->avr, AVR_IOCTL_IOPORT_GETIRQ( p->p_out.port ), p->p_out.pin), bit);
	}

	// module callback
	if ( p->callback_bit_write ) {
		p->callback_bit_write(bit, p->callback_param);
	}
}


/**
 * process clock edges (both: positive and negative edges)
 *
 * @param p		bitbang structure
 *
 */
static void avr_bitbang_clk_edge(avr_bitbang_t *p)
{
	uint8_t phase = (p->clk_count & 1) ^ p->clk_phase;
	uint8_t clk = (p->clk_count & 1) ^ p->clk_pol;

	if ( !p->enabled )
		return;

	// increase clock
	p->clk_count++;
	clk ^= 1;
	phase ^= 1;

	// generate clock output on HW pin
	if ( p->clk_generate && p->p_clk.port ) {
		avr_raise_irq(avr_io_getirq(p->avr, AVR_IOCTL_IOPORT_GETIRQ( p->p_clk.port ), p->p_clk.pin), clk);
	}

	if ( phase ) {
		// read data in
		avr_bitbang_read_bit(p);

	} else {
		// write data out
		avr_bitbang_write_bit(p);
	}

	if ( p->clk_count >= (p->buffer_size*2) ) {
		// transfer finished
		if ( p->callback_transfer_finished ) {
			p->data = p->callback_transfer_finished(p->data, p->callback_param);
		}
		p->clk_count = 0;
	}
}

static avr_cycle_count_t avr_bitbang_clk_timer(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_bitbang_t * p = (avr_bitbang_t *)param;

	avr_bitbang_clk_edge(p);

	if ( p->enabled )
		return when + p->clk_cycles/2;
	else
		return 0;
}

static void avr_bitbang_clk_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_bitbang_t * p = (avr_bitbang_t *)param;
	uint8_t clk = (p->clk_count & 1) ^ p->clk_pol;

	// no clock change
	if ( clk == value )
		return;

	avr_bitbang_clk_edge(p);
}

/**
 * reset bitbang sub-module
 *
 * @param avr	avr attached to
 * @param p		bitbang structure
 */
void avr_bitbang_reset(avr_t *avr, avr_bitbang_t * p)
{
	p->avr = avr;
	p->enabled = 0;
	p->clk_count = 0;
	p->data = 0;

	if ( p->buffer_size < 1 || p->buffer_size > 32 ) {
		AVR_LOG(avr, LOG_ERROR,
				"Error: bitbang buffer size should be between 1 and 32. set value: %d\n", p->buffer_size);
		abort();
	}

}

/**
 * start bitbang transfer
 *
 * buffers should be written / cleared in advanced
 * timers and interrupts are connected
 *
 * @param p			bitbang structure
 */
void avr_bitbang_start(avr_bitbang_t * p)
{
	p->enabled = 1;
	p->clk_count = 0;

	if ( p->clk_phase == 0 ) {
		// write first bit
		avr_bitbang_write_bit(p);
	}

	if ( p->clk_generate ) {
		// master mode, generate clock -> set timer
		avr_cycle_timer_register(p->avr, (p->clk_cycles/2), avr_bitbang_clk_timer, p);
	} else {
		// slave mode -> attach clock function to clock pin
		///@todo test
		avr_irq_register_notify( avr_io_getirq(p->avr, AVR_IOCTL_IOPORT_GETIRQ( p->p_clk.port ), p->p_clk.pin), avr_bitbang_clk_hook, p);
	}

}


/**
 * stop bitbang transfer
 *
 * timers and interrupts are disabled
 *
 * @param p			bitbang structure
 */
void avr_bitbang_stop(avr_bitbang_t * p)
{

	p->enabled = 0;
	avr_cycle_timer_cancel(p->avr, avr_bitbang_clk_timer, p);
	avr_irq_unregister_notify( avr_io_getirq(p->avr, AVR_IOCTL_IOPORT_GETIRQ( p->p_clk.port ), p->p_clk.pin), avr_bitbang_clk_hook, p);
}

#ifdef __cplusplus
};
#endif

#endif /*__AVR_BITBANG_H__*/
