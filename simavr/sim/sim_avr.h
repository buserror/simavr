/*
	sim_avr.h

	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>

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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __has_attribute
	#define __has_attribute(x) 0
#endif

#if __has_attribute(fallthrough)
	#define FALLTHROUGH __attribute__((fallthrough));
#else
	#define FALLTHROUGH
#endif

#include <stdint.h>
#include "sim_irq.h"
#include "sim_interrupts.h"
#include "sim_cmds.h"
#include "sim_cycle_timers.h"

typedef uint32_t avr_flashaddr_t;

struct avr_t;
typedef uint8_t (*avr_io_read_t)(
		struct avr_t * avr,
		avr_io_addr_t addr,
		void * param);
typedef void (*avr_io_write_t)(
		struct avr_t * avr,
		avr_io_addr_t addr,
		uint8_t v,
		void * param);

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
	MAX_IOs	= 280,	// Bigger AVRs need more than 256-32 (mega1280)
};

#define AVR_DATA_TO_IO(v) ((v) - 32)
#define AVR_IO_TO_DATA(v) ((v) + 32)

/**
 * Logging macros and associated log levels.
 * The current log level is kept in avr->log.
 */
enum {
	LOG_NONE = 0,
	LOG_OUTPUT,
	LOG_ERROR,
	LOG_WARNING,
	LOG_TRACE,
	LOG_DEBUG,
};


#ifndef AVR_LOG
#define AVR_LOG(avr, level, ...) \
	do { \
		avr_global_logger(avr, level, __VA_ARGS__); \
	} while(0)
#endif
#define AVR_TRACE(avr, ... ) \
	AVR_LOG(avr, LOG_TRACE, __VA_ARGS__)

/*
 * Core states.
 */
enum {
	cpu_Limbo = 0,	// before initialization is finished
	cpu_Stopped,	// all is stopped, timers included

	cpu_Running,	// we're free running

	cpu_Sleeping,	// we're now sleeping until an interrupt

	cpu_Step,	// run ONE instruction, then...
	cpu_StepDone,	// tell gdb it's all OK, and give it registers
	cpu_Done,       // avr software stopped gracefully
	cpu_Crashed,    // avr software crashed (watchdog fired)
};

// this is only ever used if CONFIG_SIMAVR_TRACE is defined
struct avr_trace_data_t {
	const char **   codeline;       // Text for each Flash address
	uint32_t        codeline_size;  // Size of codeline table.
	uint32_t        data_names_size;// Size of data_names table.

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

typedef void (*avr_run_t)(
		struct avr_t * avr);

#define AVR_FUSE_LOW	0
#define AVR_FUSE_HIGH	1
#define AVR_FUSE_EXT	2

/*
 * Main AVR instance. Some of these fields are set by the AVR "Core" definition files
 * the rest is runtime data (as little as possible)
 */
typedef struct avr_t {
	const char *	 	mmcu;	// name of the AVR
	// these are filled by sim_core_declare from constants in /usr/lib/avr/include/avr/io*.h
	uint16_t			ioend;
	uint16_t 			ramend;
	uint32_t			flashend;
	uint32_t			e2end;
	uint8_t				vector_size;
	// accessible via LPM (BLBSET)
	uint8_t				fuse[6];
	uint8_t				lockbits;
	// accessible via LPM (if SIGRD is present)
	uint8_t				signature[3];
	uint8_t				serial[9];

	avr_io_addr_t		rampz;	// optional, only for ELPM/SPM on >64Kb cores
	avr_io_addr_t		eind;	// optional, only for EIJMP/EICALL on >64Kb cores
	uint8_t				address_size;	// 2, or 3 for cores >128KB in flash
	struct {
		avr_regbit_t		porf;
		avr_regbit_t		extrf;
		avr_regbit_t		borf;
		avr_regbit_t		wdrf;
	} reset_flags;

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

	// these next two allow the core to freely run between cycle timers and also allows
	// for a maximum run cycle limit... run_cycle_count is set during cycle timer processing.
	avr_cycle_count_t	run_cycle_count;	// cycles to run before next timer
	avr_cycle_count_t	run_cycle_limit;	// maximum run cycle interval limit

	/**
	 * Sleep requests are accumulated in sleep_usec until the minimum sleep value
	 * is reached, at which point sleep_usec is cleared and the sleep request
	 * is passed on to the operating system.
	 */
	uint32_t 			sleep_usec;
	uint64_t			time_base;	// for avr_get_time_stamp()

	// called at init time
	void (*init)(struct avr_t * avr);
	// called at reset time
	void (*reset)(struct avr_t * avr);

	struct {
		// called at init time (for special purposes like using a
		// memory mapped file as flash see: simduino)
		void (*init)(struct avr_t * avr, void * data);
		// called at termination time ( to clean special initializations)
		void (*deinit)(struct avr_t * avr, void * data);
		// value passed to init() and deinit()
		void *data;
	} custom;

	/*!
	 * Default AVR core run function.
	 * Two modes are available, a "raw" run that goes as fast as
	 * it can, and a "gdb" mode that also watchouts for gdb events
	 * and is a little bit slower.
	 */
	avr_run_t	run;

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
	// This array is re-synthesized back/forth when SREG changes
	uint8_t		sreg[8];

	/* Interrupt state:
		00: idle (no wait, no pending interrupts) or disabled
		<0: wait till zero
		>0: interrupt pending */
	int8_t			interrupt_state;	// interrupt state

	/*
	 * ** current PC **
	 * Note that the PC is representing /bytes/ while the AVR value is
	 * assumed to be "words". This is in line with what GDB does...
	 * this is why you will see >>1 and <<1 in the decoder to handle jumps.
	 * It CAN be a little confusing, so concentrate, young grasshopper.
	 */
	avr_flashaddr_t	pc;
	/*
	 * Reset PC, this is the value used to jump to at reset time, this
	 * allow support for bootloaders
	 */
	avr_flashaddr_t	reset_pc;

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
	int				io_shared_io_count;
	struct {
		int used;
		struct {
			void * param;
			void * c;
		} io[4];
	} io_shared_io[4];

    // SRAM tracepoint
    #define SRAM_TRACEPOINT_SIZE 16
	int				sram_tracepoint_count;
	struct {
		struct avr_irq_t * irq;
		int width;
		uint16_t addr;
	} sram_tracepoint[SRAM_TRACEPOINT_SIZE];

	// flash memory (initialized to 0xff, and code loaded into it)
	uint8_t *		flash;
	// this is the general purpose registers, IO registers, and SRAM
	uint8_t *		data;

	// queue of io modules
	struct avr_io_t * io_port;

	// Builtin and user-defined commands
	avr_cmd_table_t commands;
	// cycle timers tracking & delivery
	avr_cycle_timer_pool_t	cycle_timers;
	// interrupt vectors and delivery fifo
	avr_int_table_t	interrupts;

	// DEBUG ONLY -- value ignored if CONFIG_SIMAVR_TRACE = 0
	uint8_t	trace : 1,
			log : 4; // log level, default to 1

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

	// buffer for console debugging output from register
	struct {
		char *	 buf;
		uint32_t size;
		uint32_t len;
	} io_console_buffer;

	// Table of register names used by gdb and tracing.

	const char ** data_names;
} avr_t;


// this is a static constructor for each of the AVR devices
typedef struct avr_kind_t {
	const char * names[4];	// name aliases
	avr_t * (*make)(void);
} avr_kind_t;

// a symbol loaded from the .elf file
typedef struct avr_symbol_t {
	uint32_t	addr;
	uint32_t	size;
	const char  symbol[0];
} avr_symbol_t;

// locate the maker for mcu "name" and allocates a new avr instance
avr_t *
avr_make_mcu_by_name(
		const char *name);
// initializes a new AVR instance. Will call the IO registers init(), and then reset()
int
avr_init(
		avr_t * avr);
// Used by the cores, allocated a mutable avr_t from the const global
avr_t *
avr_core_allocate(
		const avr_t * core,
		uint32_t coreLen);

// resets the AVR, and the IO modules
void
avr_reset(
		avr_t * avr);
// run one cycle of the AVR, sleep if necessary
int
avr_run(
		avr_t * avr);
// finish any pending operations
void
avr_terminate(
		avr_t * avr);

// set an IO register to receive commands from the AVR firmware
// it's optional, and uses the ELF tags
void
avr_set_command_register(
		avr_t * avr,
		avr_io_addr_t addr);

// specify the "console register" -- output sent to this register
// is printed on the simulator console, without using a UART
void
avr_set_console_register(
		avr_t * avr,
		avr_io_addr_t addr);

// load code in the "flash"
void
avr_loadcode(
		avr_t * avr,
		uint8_t * code,
		uint32_t size,
		avr_flashaddr_t address);

/*
 * These are accessors for avr->data but allows watchpoints to be set for gdb
 * IO modules use that to set values to registers, and the AVR core decoder uses
 * that to register "public" read by instructions.
 */
void
avr_core_watch_write(
		avr_t *avr,
		uint16_t addr,
		uint8_t v);
uint8_t
avr_core_watch_read(
		avr_t *avr,
		uint16_t addr);

// called when the core has detected a crash somehow.
// this might activate gdb server
void
avr_sadly_crashed(
		avr_t *avr,
		uint8_t signal);

/*
 * Logs a message using the current logger
 */
void
avr_global_logger(
		struct avr_t* avr,
		const int level,
		const char * format,
		... );

#ifndef AVR_CORE
#include <stdarg.h>
/*
 * Type for custom logging functions
 */
typedef void (*avr_logger_p)(struct avr_t* avr, const int level, const char * format, va_list ap);

/* Sets a global logging function in place of the default */
void
avr_global_logger_set(
		avr_logger_p logger);
/* Gets the current global logger function */
avr_logger_p
avr_global_logger_get(void);
#endif

/*
 * These are callbacks for the two 'main' behaviour in simavr
 */
void avr_callback_sleep_gdb(avr_t * avr, avr_cycle_count_t howLong);
void avr_callback_run_gdb(avr_t * avr);
void avr_callback_sleep_raw(avr_t * avr, avr_cycle_count_t howLong);
void avr_callback_run_raw(avr_t * avr);

/**
 * Accumulates sleep requests (and returns a sleep time of 0) until
 * a minimum count of requested sleep microseconds are reached
 * (low amounts cannot be handled accurately).
 * This function is an utility function for the sleep callbacks
 */
uint32_t
avr_pending_sleep_usec(
		avr_t * avr,
		avr_cycle_count_t howLong);
/* Return the number of 'real time' spent since sim started, in uS */
uint64_t
avr_get_time_stamp(
		avr_t * avr );

#ifdef __cplusplus
};
#endif

#include "sim_io.h"
#include "sim_regbit.h"

#ifdef __GNUC__

# ifndef likely
#  define likely(x) __builtin_expect(!!(x), 1)
# endif

# ifndef unlikely
#  define unlikely(x) __builtin_expect(!!(x), 0)
# endif

#else /* ! __GNUC__ */

# ifndef likely
#  define likely(x) x
# endif

# ifndef unlikely
#  define unlikely(x) x
# endif

#endif /* __GNUC__ */

#endif /*__SIM_AVR_H__*/

