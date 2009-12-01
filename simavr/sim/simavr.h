/*
	simavr.h

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

#ifndef __SIMAVR_H__
#define __SIMAVR_H__

#include <stdint.h>

struct avr_t;
typedef uint8_t (*avr_io_read_t)(struct avr_t * avr, uint8_t addr, void * param);
typedef void (*avr_io_write_t)(struct avr_t * avr, uint8_t addr, uint8_t v, void * param);

enum {
	// SREG bit indexes
	S_C = 0,S_Z,S_N,S_V,S_S,S_H,S_T,S_I,

	// 16 bits register pairs
	R_XL	= 0x1a, R_XH,R_YL,R_YH,R_ZL,R_ZH,
	// stack pointer
	R_SPL	= 32+0x3d, R_SPH,
	// real SREG
	R_SREG	= 32+0x3f,

	// maximum number of IO regisrer, on normal AVRs
	MAX_IOs	= 256 - 32,	// minus 32 GP registers
};

#define AVR_DATA_TO_IO(v) ((v) - 32)
#define AVR_IO_TO_DATA(v) ((v) + 32)

/*
 * Core states. This will need populating with debug states for gdb
 */
enum {
	cpu_Limbo = 0,	// before initialization is finished
	cpu_Stopped,
	cpu_Running,
	cpu_Sleeping,
};

/*
 * Main AVR instance. Some of these fields are set by the AVR "Core" definition files
 * the rest is runtime data (as little as possible)
 */
typedef struct avr_t {
	const char * mmcu;	// name of the AVR
	// these are filled by sim_core_declare from constants in /usr/lib/avr/include/avr/io*.h
	uint16_t 	ramend;		
	uint32_t	flashend;
	uint32_t	e2end;
	uint8_t		vector_size;
	uint8_t		signature[3];
	uint8_t		fuse[4];

	// filled by the ELF data, this allow tracking of invalid jumps
	uint32_t	codeend;

	int			state;		// stopped, running, sleeping
	uint32_t	frequency;	// frequency we are running at
	uint64_t	cycle;		// current cycle
	
	// called at init time
	void (*init)(struct avr_t * avr);
	// called at reset time
	void (*reset)(struct avr_t * avr);

	// Mirror of the SREG register, to facilitate the access to bits
	// in the opcode decoder.
	// This array is re-synthetized back/forth when SREG changes
	uint8_t		sreg[8];

	/* 
	 * ** current PC **
	 * Note that the PC is reoresenting /bytes/ while the AVR value is
	 * assumed to be "words". This is in line with what GDB does...
	 * this is why you will see >>1 ane <<1 in the decoder to handle jumps
	 */
	uint32_t	pc;

	/*
	 * callback when specific IO registers are read/written
	 */
	struct {
		void * param;
		avr_io_read_t r;
	} ior[MAX_IOs];
	struct {
		void * param;
		avr_io_write_t w;
	} iow[MAX_IOs];

	// flash memory (initialized to 0xff, and code loaded into it)
	uint8_t *	flash;
	// this is the general purpose registers, IO registers, and SRAM
	uint8_t *	data;

	// queue of io modules
	struct avr_io_t *io_port;

	// interrupt vectors, and their enable/clear registers
	struct avr_int_vector_t * vector[64];
	uint8_t		pending_wait;	// number of cycles to wait for pending
	uint32_t	pending[2];		// pending interrupts

	// DEBUG ONLY
	int		trace;
	struct avr_symbol_t ** codeline;

	/* DEBUG ONLY
	 * this keeps track of "jumps" ie, call,jmp,ret,reti and so on
	 * allows dumping of a meaningful data even if the stack is
	 * munched and so on
	 */
	#define OLD_PC_SIZE	32
	struct {
		uint32_t pc;
		uint16_t sp;
	} old[OLD_PC_SIZE]; // catches reset..
	int			old_pci;

#if AVR_STACK_WATCH
	#define STACK_FRAME_SIZE	32
	// this records the call/ret pairs, to try to catch
	// code that munches the stack -under- their own frame
	struct {
		uint32_t	pc;
		uint16_t 	sp;		
	} stack_frame[STACK_FRAME_SIZE];
	int			stack_frame_index;
#endif

	// DEBUG ONLY
	// keeps track of wich registers gets touched by instructions
	// reset before each new instructions. Allows meaningful traces
	uint32_t	touched[256 / 32];	// debug

	// placeholder
	struct avr_gdb_t * gdb;
} avr_t;


// this is a static constructor for each of the AVR devices
typedef struct avr_kind_t {
	const char * names[4];	// name aliases
	avr_t * (*make)();
} avr_kind_t;

// a symbol loaded from the .elf file
typedef struct avr_symbol_t {
	const char * symbol;
	uint32_t	addr;
} avr_symbol_t;


// initializes a new AVR instance. Will call the IO registers init(), and then reset()
int avr_init(avr_t * avr);
// resets the AVR, and the IO modules
void avr_reset(avr_t * avr);

// load code in the "flash"
void avr_loadcode(avr_t * avr, uint8_t * code, uint32_t size, uint32_t address);


/*
 * these are accessors for avr->data but allows watchpoints to be set for gdb
 * IO modules use that to set values to registers, and the AVR core decoder uses
 * that to register "public" read by instructions.
 */
void avr_core_watch_write(avr_t *avr, uint16_t addr, uint8_t v);
uint8_t avr_core_watch_read(avr_t *avr, uint16_t addr);


#include "sim_io.h"
#include "sim_regbit.h"
#include "sim_interrupts.h"
#include "sim_irq.h"

#endif /*__SIMAVR_H__*/

