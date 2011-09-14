/*
	sim_avr.h

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

#ifndef __SIM_AVR_H__
#define __SIM_AVR_H__

#include <stdint.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_irq.h"

typedef uint64_t avr_cycle_count_t;
typedef uint16_t	avr_io_addr_t;

// printf() conversion specifier for avr_cycle_count_t
#define PRI_avr_cycle_count PRIu64

struct avr_t;
typedef uint8_t (*avr_io_read_t)(struct avr_t * avr, avr_io_addr_t addr, void * param);
typedef void (*avr_io_write_t)(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param);
typedef avr_cycle_count_t (*avr_cycle_timer_t)(struct avr_t * avr, avr_cycle_count_t when, void * param);

enum {
	// SREG bit indexes
	S_C = 0,S_Z,S_N,S_V,S_S,S_H,S_T,S_I,

	// 16 bits register pairs
	R_XL	= 0x1a, R_XH,R_YL,R_YH,R_ZL,R_ZH,
	// stack pointer
	R_SPL	= 32+0x3d, R_SPH,
	// real SREG
	R_SREG	= 32+0x3f,

	// maximum number of IO registers, on normal AVRs
	MAX_IOs	= 256 - 32,	// minus 32 GP registers
};

#define AVR_DATA_TO_IO(v) ((v) - 32)
#define AVR_IO_TO_DATA(v) ((v) + 32)

/*
 * Core states.
 */
enum {
	cpu_Limbo = 0,	// before initialization is finished
	cpu_Stopped,	// all is stopped, timers included

	cpu_Running,	// we're free running

	cpu_Sleeping,	// we're now sleeping until an interrupt

	cpu_Step,		// run ONE instruction, then...
	cpu_StepDone,	// tell gdb it's all OK, and give it registers
	cpu_Done,       // avr software stopped gracefully
	cpu_Crashed,    // avr software crashed (watchdog fired)
};

// this is only ever used if CONFIG_SIMAVR_TRACE is defined
struct avr_trace_data_t {
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
	// keeps track of which registers gets touched by instructions
	// reset before each new instructions. Allows meaningful traces
	uint32_t	touched[256 / 32];	// debug
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
	avr_io_addr_t	rampz;	// optional, only for ELPM/SPM on >64Kb cores
	avr_io_addr_t	eind;	// optional, only for EIJMP/EICALL on >64Kb cores

	// filled by the ELF data, this allow tracking of invalid jumps
	uint32_t			codeend;

	int					state;		// stopped, running, sleeping
	uint32_t			frequency;	// frequency we are running at
	// mostly used by the ADC for now
	uint32_t			vcc,avcc,aref; // (optional) voltages in millivolts

	// cycles gets incremented when sleeping and when running; it corresponds
	// not only to "cycles that runs" but also "cycles that might have run"
	// like, sleeping.
	avr_cycle_count_t	cycle;		// current cycle
	
	// called at init time
	void (*init)(struct avr_t * avr);
	// called at init time (for special purposes like using a memory mapped file as flash see: simduino)
	void (*special_init)(struct avr_t * avr);
	// called at termination time ( to clean special initalizations)
	void (*special_deinit)(struct avr_t * avr);
	// called at reset time
	void (*reset)(struct avr_t * avr);

	/*!
	 * Default AVR core run function.
	 * Two modes are available, a "raw" run that goes as fast as
	 * it can, and a "gdb" mode that also watchouts for gdb events
	 * and is a little bit slower.
	 */
	void (*run)(struct avr_t * avr);

	/*!
	 * Sleep default behaviour.
	 * In "raw" mode, it calls usleep, in gdb mode, it waits
	 * for howLong for gdb command on it's sockets.
	 */
	void (*sleep)(struct avr_t * avr, avr_cycle_count_t howLong);

	/*!
	 * Every IRQs will be stored in this pool. It is not
	 * mandatory (yet) but will allow listing IRQs and their connections
	 */
	avr_irq_pool_t	irq_pool;

	// Mirror of the SREG register, to facilitate the access to bits
	// in the opcode decoder.
	// This array is re-synthetized back/forth when SREG changes
	uint8_t		sreg[8];
	uint8_t		i_shadow;	// used to detect edges on I flag

	/* 
	 * ** current PC **
	 * Note that the PC is representing /bytes/ while the AVR value is
	 * assumed to be "words". This is in line with what GDB does...
	 * this is why you will see >>1 and <<1 in the decoder to handle jumps.
	 * It CAN be a little confusing, so concentrate, young grasshopper.
	 */
	uint32_t	pc;

	/*
	 * callback when specific IO registers are read/written.
	 * There is one drawback here, there is in way of knowing what is the
	 * "beginning of useful sram" on a core, so there is no way to deduce
	 * what is the maximum IO register for a core, and thus, we can't
	 * allocate this table dynamically.
	 * If you wanted to emulate the BIG AVRs, and XMegas, this would need
	 * work.
	 */
	struct {
		struct avr_irq_t * irq;	// optional, used only if asked for with avr_iomem_getirq()
		struct {
			void * param;
			avr_io_read_t c;
		} r;
		struct {
			void * param;
			avr_io_write_t c;
		} w;
	} io[MAX_IOs];

	/*
	 * This block allows sharing of the IO write/read on addresses between
	 * multiple callbacks. In 99% of case it's not needed, however on the tiny*
	 * (tiny85 at last) some registers have bits that are used by different
	 * IO modules.
	 * If this case is detected, a special "dispatch" callback is installed that
	 * will handle this particular case, without impacting the performance of the
	 * other, normal cases...
	 */
	int	io_shared_io_count;
	struct {
		int used;
		struct {
			void * param;
			void * c;
		} io[4];
	} io_shared_io[4];

	// flash memory (initialized to 0xff, and code loaded into it)
	uint8_t *	flash;
	// this is the general purpose registers, IO registers, and SRAM
	uint8_t *	data;

	// queue of io modules
	struct avr_io_t *io_port;

	// cycle timers are callbacks that will be called when "when" cycle is reached
	// the bitmap allows quick knowledge of whether there is anything to call
	// these timers are one shots, then get cleared if the timer function returns zero,
	// they get reset if the callback function returns a new cycle number
	uint32_t	cycle_timer_map;
	avr_cycle_count_t next_cycle_timer;
	struct {
		avr_cycle_count_t	when;
		avr_cycle_timer_t	timer;
		void * param;
	} cycle_timer[32];

	// interrupt vectors, and their enable/clear registers
	struct avr_int_vector_t * vector[64];
	uint8_t		pending_wait;	// number of cycles to wait for pending
	uint32_t	pending[2];		// pending interrupts

	// DEBUG ONLY -- value ignored if CONFIG_SIMAVR_TRACE = 0
	int		trace : 1,
			log : 2; // log level, default to 1

	// Only used if CONFIG_SIMAVR_TRACE is defined
	struct avr_trace_data_t *trace_data;

	// VALUE CHANGE DUMP file (waveforms)
	// this is the VCD file that gets allocated if the 
	// firmware that is loaded explicitly asks for a trace
	// to be generated, and allocates it's own symbols
	// using AVR_MMCU_TAG_VCD_TRACE (see avr_mcu_section.h)
	struct avr_vcd_t * vcd;
	
	// gdb hooking structure. Only present when gdb server is active
	struct avr_gdb_t * gdb;

	// if non-zero, the gdb server will be started when the core
	// crashed even if not activated at startup
	// if zero, the simulator will just exit() in case of a crash
	int		gdb_port;
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

// locate the maker for mcu "name" and allocates a new avr instance
avr_t * avr_make_mcu_by_name(const char *name);
// initializes a new AVR instance. Will call the IO registers init(), and then reset()
int avr_init(avr_t * avr);
// resets the AVR, and the IO modules
void avr_reset(avr_t * avr);
// run one cycle of the AVR, sleep if necessary
int avr_run(avr_t * avr);
// finish any pending operations 
void avr_terminate(avr_t * avr);

// set an IO register to receive commands from the AVR firmware
// it's optional, and uses the ELF tags
void avr_set_command_register(avr_t * avr, avr_io_addr_t addr);

// specify the "console register" -- output sent to this register
// is printed on the simulator console, without using a UART
void avr_set_console_register(avr_t * avr, avr_io_addr_t addr);

// load code in the "flash"
void avr_loadcode(avr_t * avr, uint8_t * code, uint32_t size, uint32_t address);


/*
 * these are accessors for avr->data but allows watchpoints to be set for gdb
 * IO modules use that to set values to registers, and the AVR core decoder uses
 * that to register "public" read by instructions.
 */
void avr_core_watch_write(avr_t *avr, uint16_t addr, uint8_t v);
uint8_t avr_core_watch_read(avr_t *avr, uint16_t addr);

// called when the core has detected a crash somehow.
// this might activate gdb server
void avr_sadly_crashed(avr_t *avr, uint8_t signal);


/*
 * These are callbacks for the two 'main' bahaviour in simavr
 */
void avr_callback_sleep_gdb(avr_t * avr, avr_cycle_count_t howLong);
void avr_callback_run_gdb(avr_t * avr);
void avr_callback_sleep_raw(avr_t * avr, avr_cycle_count_t howLong);
void avr_callback_run_raw(avr_t * avr);

#ifdef __cplusplus
};
#endif

#include "sim_io.h"
#include "sim_regbit.h"
#include "sim_interrupts.h"
#include "sim_cycle_timers.h"

#endif /*__SIM_AVR_H__*/

