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

	// interupt vectors, and their enable/clear registers
	struct avr_int_vector_t * vector[64];
	uint8_t		pending_wait;	// number of cycles to wait for pending
	uint32_t	pending[2];		// pending interupts

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

/*
 * this 'structure' is a packed representation of an IO register 'bit'
 * (or consecutive bits). This allows a way to set/get/clear them.
 * gcc is happy passing these as register value, so you don't need to
 * use a pointer when passing them along to functions.
 */
typedef struct avr_regbit_t {
	unsigned long reg : 8, bit : 3, mask : 8;
} avr_regbit_t;

// interupt vector for the IO modules
typedef struct avr_int_vector_t {
	uint8_t vector;		// vector number, zero (reset) is reserved

	avr_regbit_t enable;	// IO register index for the "interupt enable" flag for this vector
	avr_regbit_t raised;	// IO register index for the register where the "raised" flag is (optional)
} avr_int_vector_t;

/*
 * used by the ioports to implement their own features
 * see avr_eeprom.* for an example, and avr_ioctl().
 */
#define AVR_IOCTL_DEF(_a,_b,_c,_d) \
	(((_a) << 24)|((_b) << 16)|((_c) << 8)|((_d)))

/*
 * IO module base struct
 * Modules uses that as their first member in their own struct
 */
typedef struct avr_io_t {
	struct avr_io_t * next;
	const char * kind;
	// called at every instruction
	void (*run)(avr_t * avr, struct avr_io_t *io);
	// called at reset time
	void (*reset)(avr_t * avr, struct avr_io_t *io);
	// called externally. allow access to io modules and so on
	int (*ioctl)(avr_t * avr, struct avr_io_t *io, uint32_t ctl, void *io_param);
} avr_io_t;

// initializes a new AVR instance. Will call the IO registers init(), and then reset()
int avr_init(avr_t * avr);
// resets the AVR, and the IO modules
void avr_reset(avr_t * avr);

// load code in the "flash"
void avr_loadcode(avr_t * avr, uint8_t * code, uint32_t size, uint32_t address);

/*
 * IO modules helper functions
 */

// registers an IO module, so it's run(), reset() etc are called
// this is called by the AVR core init functions, you /could/ register an external
// one after instanciation, for whatever purpose...
void avr_register_io(avr_t *avr, avr_io_t * io);
// register a callback for when IO register "addr" is read
void avr_register_io_read(avr_t *avr, uint8_t addr, avr_io_read_t read, void * param);
// register a callback for when the IO register is written. callback has to set the memory itself
void avr_register_io_write(avr_t *avr, uint8_t addr, avr_io_write_t write, void * param);
// call every IO modules until one responds to this
int avr_ioctl(avr_t *avr, uint32_t ctl, void * io_param);

/*
 * Interupt Helper Functions
 */
// register an interupt vector. It's only needed if you want to use the "r_raised" flags
void avr_register_vector(avr_t *avr, avr_int_vector_t * vector);
// raise an interupt (if enabled). The interupt is latched and will be called later
void avr_raise_interupt(avr_t * avr, avr_int_vector_t * vector);
// return non-zero if the AVR core has any pending interupts
int avr_has_pending_interupts(avr_t * avr);
// return nonzero if a soecific interupt vector is pending
int avr_is_interupt_pending(avr_t * avr, avr_int_vector_t * vector);

/*
 * these are accessors for avr->data but allows watchpoints to be set for gdb
 * IO modules use that to set values to registers, and the AVR core decoder uses
 * that to register "public" read by instructions.
 */
void avr_core_watch_write(avr_t *avr, uint16_t addr, uint8_t v);
uint8_t avr_core_watch_read(avr_t *avr, uint16_t addr);


/*
 * These accessors are inlined and are used to perform the operations on
 * avr_regbit_t definitions. This is the "official" way to access bits into registers
 * The small footorint costs brings much better versatility for functions/bits that are
 * not always defined in the same place on real AVR cores
 */
/*
 * set/get/clear io register bits in one operation
 */
static inline uint8_t avr_regbit_set(avr_t * avr, avr_regbit_t rb)
{
	uint8_t a = rb.reg;
	if (!a)
		return 0;
	uint8_t m = rb.mask << rb.bit;
	avr_core_watch_write(avr, a, avr->data[a] | m);
	return (avr->data[a] >> rb.bit) & rb.mask;
}

static inline uint8_t avr_regbit_setto(avr_t * avr, avr_regbit_t rb, uint8_t v)
{
	uint8_t a = rb.reg;
	if (!a)
		return 0;
	uint8_t m = rb.mask << rb.bit;
	avr_core_watch_write(avr, a, (avr->data[a] & ~(m)) | ((v << rb.bit) & m));
	return (avr->data[a] >> rb.bit) & rb.mask;
}

static inline uint8_t avr_regbit_get(avr_t * avr, avr_regbit_t rb)
{
	uint8_t a = rb.reg;
	if (!a)
		return 0;
	//uint8_t m = rb.mask << rb.bit;
	return (avr->data[a] >> rb.bit) & rb.mask;
}

static inline uint8_t avr_regbit_clear(avr_t * avr, avr_regbit_t rb)
{
	uint8_t a = (rb.reg);
	uint8_t m = rb.mask << rb.bit;
	avr_core_watch_write(avr, a, avr->data[a] & ~m);
	return avr->data[a];
}

#define ARRAY_SIZE(_aa) (sizeof(_aa) / sizeof((_aa)[0]))

/*
 * This reads the bits for an array of avr_regbit_t, make up a "byte" with them.
 * This allows reading bits like CS0, CS1, CS2 etc even if they are not in the same
 * physical IO register.
 */
static inline uint8_t avr_regbit_get_array(avr_t * avr, avr_regbit_t *rb, int count)
{
	uint8_t res = 0;

	for (int i = 0; i < count; i++, rb++) if (rb->reg) {
		uint8_t a = (rb->reg);
		res |= ((avr->data[a] >> rb->bit) & rb->mask) << i;
	}
	return res;
}

#define AVR_IO_REGBIT(_io, _bit) { . reg = (_io), .bit = (_bit), .mask = 1 }
#define AVR_IO_REGBITS(_io, _bit, _mask) { . reg = (_io), .bit = (_bit), .mask = (_mask) }


#endif /*__SIMAVR_H__*/

