/*
	avr_spi.c

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

#include <stdio.h>
#include "avr_spi.h"
#include "sim_gdb.h"

static avr_cycle_count_t avr_spi_raise(struct avr_t * avr, avr_cycle_count_t when, void * param)
{
	avr_spi_t * p = (avr_spi_t *)param;

	if (avr_regbit_get(avr, p->spe)) {
		avr_raise_interrupt(avr, &p->spi);
		if (avr_regbit_get(avr, p->mstr)) {
			avr_raise_irq(p->io.irq + SPI_IRQ_OUTPUT, p->output_data_register);
		}
		// the last received byte will be echoed back the next time, unless user overrides it
		p->output_data_register = p->input_data_register;
		if (p->flags & AVR_SPI_FLAG_BITBANG_ON)
			p->bit_bang.data = p->output_data_register;
	}
	return 0;
}

static uint32_t avr_spi_transfer_finished(uint32_t data, void *param)
{
	avr_spi_t * p = (avr_spi_t *)param;
	if (p->bit_bang.clk_generate)
		avr_bitbang_stop(&(p->bit_bang));
	p->input_data_register = data & 0xFF;
	p->bit_bang.avr->data[p->r_spdr] = p->input_data_register; // not necessary, but for fullness..
	avr_spi_raise(p->bit_bang.avr, 0, p);
	return data;
}

static void avr_spi_bitbang_switch_mode(avr_spi_t * p, uint8_t master)
{
	if (!(p->flags & AVR_SPI_FLAG_IOPIN_DIR_CTRL))
		return;
	if (master) {
		//p->bit_bang.p_in = p->p_miso;
		//p->bit_bang.p_out = p->p_mosi;
		avr_bitbang_defpins(&(p->bit_bang), &(p->p_sck), &(p->p_miso), &(p->p_mosi));
		avr_irq_t *irq;
		uint32_t irq_val;
		irq = avr_io_getirq( p->bit_bang.avr,
										AVR_IOCTL_IOPORT_GETIRQ(p->p_miso.port),
										IOPORT_IRQ_DIRECTION_ALL );
        irq_val = irq->value & ~(1 << p->p_miso.pin);
		avr_raise_irq(irq, irq_val);
		p->bit_bang.clk_generate = 1;
	} else {
		//p->bit_bang.p_in = p->p_mosi;
		//p->bit_bang.p_out = p->p_miso;
		avr_bitbang_defpins(&(p->bit_bang), &(p->p_sck), &(p->p_mosi), &(p->p_miso));
		avr_irq_t *irq;
		uint32_t irq_val;
		irq = avr_io_getirq( p->bit_bang.avr,
										AVR_IOCTL_IOPORT_GETIRQ(p->bit_bang.p_clk.port),
										IOPORT_IRQ_DIRECTION_ALL );
        irq_val = irq->value & ~(1 << p->bit_bang.p_clk.pin);
		avr_raise_irq(irq, irq_val);
		irq = avr_io_getirq( p->bit_bang.avr,
										AVR_IOCTL_IOPORT_GETIRQ(p->p_mosi.port),
										IOPORT_IRQ_DIRECTION_ALL );
        irq_val = irq->value & ~(1 << p->p_mosi.pin);
		avr_raise_irq(irq, irq_val);
		irq = avr_io_getirq( p->bit_bang.avr,
										AVR_IOCTL_IOPORT_GETIRQ(p->p_ss.port),
										IOPORT_IRQ_DIRECTION_ALL );
        irq_val = irq->value & ~(1 << p->p_ss.pin);
		avr_raise_irq(irq, irq_val);
		p->bit_bang.clk_generate = 0;
	}
}

static uint8_t avr_spi_read(struct avr_t * avr, avr_io_addr_t addr, void * param)
{
	avr_spi_t * p = (avr_spi_t *)param;
	uint8_t v = p->input_data_register;
	p->input_data_register = 0; // this seems is not correct
	avr_regbit_clear(avr, p->spi.raised);
//	printf("avr_spi_read = %02x\n", v);
	return v;
}

static void avr_spi_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
	avr_spi_t * p = (avr_spi_t *)param;

	if (addr == p->r_spdr) {
		/* Clear the SPIF bit. See ATmega164/324/644 manual, Section 18.5.2. */
		avr_regbit_clear(avr, p->spi.raised);

		// The byte to be sent should NOT be written there,
		// the value written could never be read back.
		//avr_core_watch_write(avr, addr, v);
		p->output_data_register = v;
		if (avr->gdb) {
			avr_gdb_handle_watchpoints(avr, addr, AVR_GDB_WATCH_WRITE);
		}
		if (avr_regbit_get(avr, p->spe)) {
			if (p->flags & AVR_SPI_FLAG_BITBANG_ON) {
				avr_bitbang_stop(&(p->bit_bang));
				avr_bitbang_reset(avr, &(p->bit_bang));
				p->bit_bang.data = v;
				if (avr_regbit_get(avr, p->mstr)) {
					p->bit_bang.clk_generate = 1;
					avr_bitbang_start(&(p->bit_bang));
				} else {
					p->bit_bang.clk_generate = 0;
				}
			} else {
				if (avr_regbit_get(avr, p->mstr))
					avr_cycle_timer_register(avr, p->bit_bang.clk_cycles, avr_spi_raise, p);
			}
		}
	} else if ((addr == p->r_spcr) || (addr == p->r_spsr)) {
		avr_core_watch_write(avr, addr, v);
		avr_bitbang_stop(&(p->bit_bang));
		avr_bitbang_reset(avr, &(p->bit_bang));
        p->bit_bang.data = p->output_data_register;
		if (avr_regbit_get(avr, p->spe)) {
			p->bit_bang.clk_phase = avr_regbit_get(avr, p->cpha);
			p->bit_bang.clk_pol = avr_regbit_get(avr, p->cpol);
			p->bit_bang.data_order = avr_regbit_get(avr, p->dord);
			if (avr_regbit_get(avr, p->mstr)) {
				int clock_divider_ix = (avr_regbit_get(avr, p->spr[0]) | avr_regbit_get(avr, p->spr[1]));
				static const char clock_div_values[] = { 4,16,64,128 };
				int clock_divider = clock_div_values[clock_divider_ix];
				if (avr_regbit_get(avr, p->spr[2]))
					clock_divider /= 2;
				p->bit_bang.clk_cycles = clock_divider;
				avr_spi_bitbang_switch_mode(p, 1);

			} else {
				avr_spi_bitbang_switch_mode(p, 0);
			}
		}
	}
}

static void avr_spi_ss_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_spi_t * p = (avr_spi_t *)param;

	if (avr_regbit_get(p->bit_bang.avr, p->mstr)) { // master mode
		avr_ioport_state_t iostate;
		uint8_t dir = 0;

		avr_ioctl(p->bit_bang.avr, AVR_IOCTL_IOPORT_GETSTATE( p->p_ss.port ), &iostate);
		dir = ( iostate.ddr >> p->p_ss.pin ) & 1;
		if (!dir) {
			if (!value) {
				// other master is active, reset to slave mode
				avr_bitbang_stop(&(p->bit_bang));
				avr_bitbang_reset(p->bit_bang.avr, &(p->bit_bang));
				avr_regbit_setto(p->bit_bang.avr, p->mstr, 0);
				avr_spi_bitbang_switch_mode(p, 0);
				avr_spi_raise(p->bit_bang.avr, 0, p);
			}
		}
	} else { // slave mode
		if (value) {
			avr_bitbang_stop(&(p->bit_bang));
			avr_bitbang_reset(p->bit_bang.avr, &(p->bit_bang));
		} else {
            if (avr_regbit_get(p->bit_bang.avr, p->spe))
				avr_bitbang_start(&(p->bit_bang));
		}
	}
}

static void avr_spi_irq_input(struct avr_irq_t * irq, uint32_t value, void * param)
{
	avr_spi_t * p = (avr_spi_t *)param;
	avr_t * avr = p->io.avr;

	// check to see if receiver is enabled
	if (!avr_regbit_get(avr, p->spe))
		return;

	// double buffer the input.. ?
	p->input_data_register = value;
	avr->data[p->r_spdr] = p->input_data_register; // not necessary, but for fullness..
	avr_raise_interrupt(avr, &p->spi);

	// if in slave mode, 
	// 'output' the byte only when we received one...
	if (!avr_regbit_get(avr, p->mstr)) {
		avr_raise_irq(p->io.irq + SPI_IRQ_OUTPUT, p->output_data_register); // avr->data[p->r_spdr]);
	}
	// the last received byte will be echoed next time, unless user overrides it
	p->output_data_register = p->input_data_register;
}

void avr_spi_reset(struct avr_io_t *io)
{
	avr_spi_t * p = (avr_spi_t *)io;
	avr_bitbang_reset(p->bit_bang.avr, &(p->bit_bang));
	avr_irq_register_notify(p->io.irq + SPI_IRQ_INPUT, avr_spi_irq_input, p);
}

static int
avr_spi_ioctl(
		struct avr_io_t * port,
		uint32_t ctl,
		void * io_param)
{
	avr_spi_t * p = (avr_spi_t *)port;
	int res = -1;

	if (!io_param)
		return res;

	if (ctl == AVR_IOCTL_SPI_SET_FLAGS(p->name)) {
		p->flags = *(uint32_t*)io_param;
		if (p->flags & AVR_SPI_FLAG_BITBANG_ON) {
			avr_irq_register_notify( avr_io_getirq(p->bit_bang.avr, AVR_IOCTL_IOPORT_GETIRQ( p->p_ss.port ), p->p_ss.pin),
					avr_spi_ss_hook, p);
			p->flags |= AVR_SPI_FLAG_IOPIN_DIR_CTRL | AVR_SPI_FLAG_SSPIN_HANDLE;
		} else {
			p->bit_bang.clk_generate = 0;
			if (!(p->flags & AVR_SPI_FLAG_SSPIN_HANDLE))
				avr_irq_unregister_notify( avr_io_getirq(p->bit_bang.avr, AVR_IOCTL_IOPORT_GETIRQ( p->p_ss.port ), p->p_ss.pin),
						avr_spi_ss_hook, p);
		}
		res = 0;
	}
	if (ctl == AVR_IOCTL_SPI_GET_FLAGS(p->name)) {
		*(uint32_t*)io_param = p->flags;
		res = 0;
	}

	return res;
}

static const char * irq_names[SPI_IRQ_COUNT] = {
	[SPI_IRQ_INPUT] = "8<in",
	[SPI_IRQ_OUTPUT] = "8<out",
};

static	avr_io_t	_io = {
	.kind = "spi",
	.reset = avr_spi_reset,
	.ioctl = avr_spi_ioctl,
	.irq_names = irq_names,
};

void avr_spi_init(avr_t * avr, avr_spi_t * p)
{
	p->io = _io;

	p->bit_bang.avr = avr;
	p->bit_bang.callback_transfer_finished = avr_spi_transfer_finished;
	p->bit_bang.callback_param = p;
	p->bit_bang.buffer_size = 8;
	p->bit_bang.clk_cycles = 4;
	p->flags = 0;

	avr_register_io(avr, &p->io);
	avr_register_vector(avr, &p->spi);
	// allocate this module's IRQ
	avr_io_setirqs(&p->io, AVR_IOCTL_SPI_GETIRQ(p->name), SPI_IRQ_COUNT, NULL);

	avr_register_io_write(avr, p->r_spdr, avr_spi_write, p);
	avr_register_io_write(avr, p->r_spcr, avr_spi_write, p);
	avr_register_io_write(avr, p->r_spsr, avr_spi_write, p);
	avr_register_io_read(avr, p->r_spdr, avr_spi_read, p);

}

