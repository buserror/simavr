/*
	avr_bitbang.h

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

/**
	@defgroup avr_bitbang	Generic BitBang Module
	@{

	Generic BitBang Module of simavr AVR simulator.

	@par Features / Implementation Status
		- easy buffer access with push() / pop() functions
		- one input and one output pin (can be the same HW pin for I2C)

	@todo
		- one input and one output pin (can be the same HW pin for I2C)
		- one clock pin which can be configured as input or output
			when the clock is output, the clock signal is generated with a
			configured frequency (master / slave mode)
		- 2x 32-bit buffers (input / output) (allows start, stop bits for UART, etc.)
		- on each read / write a callback is executed to notify the master module

*/


#ifndef AVR_BITBANG_H_
#define AVR_BITBANG_H_

#include "sim_avr.h"
#include "avr_ioport.h"




/// SPI Module initialization and state structure
typedef struct avr_bitbang_t {
	avr_t *				avr;		///< avr we are attached to

	uint8_t enabled;		///< bit-bang enabled flag
	uint8_t clk_generate;	///< generate clock and write to clock pin (if available) -> master / slave mode
	uint8_t clk_pol;		///< clock polarity, base (inactive) value of clock
	uint8_t clk_phase;		///< clock phase / data sampling edge
							///		- 0: data are sampled at first clock edge
							///		- 1: data are sampled at second clock edge
	uint32_t clk_cycles;	///< cycles per clock period - must be multiple of 2! (used if clk_generate is enabled)
	uint8_t data_order;		///< data order / shift
							///		- 0: shift left
							///		- 1: shift right

	uint8_t buffer_size;	///< size of buffer in bits (1...32)

	void *callback_param;	/// anonymous parameter for callback functions
	void (*callback_bit_read)(uint8_t bit, void *param); 	///< callback function to notify about bit read
	void (*callback_bit_write)(uint8_t bit, void *param); 	///< callback function to notify about bit write
	uint32_t (*callback_transfer_finished)(uint32_t data, void *param); 	///< callback function to notify about a complete transfer
																			///		(read received data and write new output data)

	avr_iopin_t	p_clk;		///< clock pin (optional)
	avr_iopin_t	p_in;		///< data in pin
	avr_iopin_t	p_out;		///< data out pin

// private data
	uint32_t data;			///< data buffer
							///		- latest received bit the is lowest / most right one, bit number: 0
							///		- next bit to be written is the highest one, bit number: (buffer_size-1)
	int8_t		clk_count;	///< internal clock edge count
} avr_bitbang_t;

/**
 * reset bitbang sub-module
 *
 * @param avr	avr attached to
 * @param p		bitbang structure
 */
void avr_bitbang_reset(avr_t *avr, avr_bitbang_t * p);

/**
 * start bitbang transfer
 *
 * buffers should be written / cleared in advanced
 * timers and interrupts are connected
 *
 * @param p			bitbang structure
 */
void avr_bitbang_start(avr_bitbang_t * p);


/**
 * stop bitbang transfer
 *
 * timers and interrupts are disabled
 *
 * @param p			bitbang structure
 */
void avr_bitbang_stop(avr_bitbang_t * p);


#endif /* AVR_BITBANG_H_ */
/// @} end of avr_bitbang group
