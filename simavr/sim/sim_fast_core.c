/*
	sim_fast_core.c

	Copyright 2014 Michael Hughes <squirmyworms@embarqmail.com>
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

#ifndef __SIM_FAST_CORE_C
#define __SIM_FAST_CORE_C

#include <stdio.h>	// printf
#include <ctype.h>	// toupper
#include <stdlib.h>	// abort
#include <endian.h>	// byteorder macro stuff
#include <string.h>	// memset
#include <assert.h>	// assert

#include "sim_avr.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "avr_flash.h"
#include "sim_fast_core.h"
#include "avr_watchdog.h"

/* CONFIG_AVR_FAST_CORE_UINST_PROFILING
	tracks dtime and count of each instruction executed
	 enable/disable via 
	 	#define CONFIG_AVR_FAST_CORE_UINST_PROFILING
	 in sim_fast_core_profiling.h */
#include "sim_fast_core_profiler.h"

/* AVR_FAST_CORE_BRANCH_HINTS
	via likely() and unlikely() macros provide the compiler (and possibly passed 
	onto the processor) hints to help reduce pipeline stalls due to 
	misspredicted branches. USE WITH CARE! :) */
/* AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
	for processors with fast multiply, helps reduce branches in comparisons 
	some processors may have specialized instructions making this slower */
/* AVR_FAST_CORE_COMBINING
	common instruction sequences are combined as well as allowing 16 bit access tricks. */
/* AVR_FAST_CORE_COMPLEX
	some instuctions are translated to use a more simplified form. */
/* AVR_FAST_CORE_CPI_BRXX
	use individual cpi_brxx instruction sequences. */
/* AVR_FAST_CORE_CPI_BRXX_COMMON_BRANCH_CODE
	uses a central funciton to do the actual branch. */
/* AVR_FAST_CORE_IO_DISPTACH_TABLES
	initial io access will be trapped, later access will happen through dispatch tables. */
/* AVR_FAST_CORE_READ_MODIFY_WRITE
	reduces redundancy inherent in register access...  and cuts back on 
	some unecessary checks. */
/* AVR_FAST_CORE_SKIP_SHIFT
	use shifts vs comparisons and branching where shifting is less expensive
	then branching. Some processors have specialized instructions to handle
	such cases and may be faster disabled. */
/* AVR_FAST_CORE_32_SKIP_SHIFT
	long instruction skip is not as strait forward as a simple skip...
	overall results may vary. */
/* AVR_FAST_CORE_SLEEP_PREPROCESS_TIMERS_AND_INTERRUPTS
	fast forwards avr->cycle then calls timer and interrupt service routines in sleep functions */

#define AVR_FAST_CORE_BRANCH_HINTS
#define AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
static const int _AVR_FAST_CORE_COMBINING = 1;
static const int _AVR_FAST_CORE_COMPLEX = 1;
#define AVR_FAST_CORE_COMMON_DATA
static const int _AVR_FAST_CORE_CPI_BRXX = 1;
#define AVR_FAST_CORE_CPI_BRXX_COMMON_BRANCH_CODE
#define AVR_FAST_CORE_IO_DISPATCH_TABLES
#define AVR_FAST_CORE_READ_MODIFY_WRITE
#define AVR_FAST_CORE_SKIP_SHIFT
#define AVR_FAST_CORE_32_SKIP_SHIFT
#define AVR_FAST_CORE_SLEEP_PREPROCESS_TIMERS_AND_INTERRUPTS


/* CONFIG_SIMAVR_FAST_CORE_AGGRESSIVE_OPTIONS
	makefile option for those whom insist on every last possible cycle. */
#ifdef CONFIG_SIMAVR_FAST_CORE_AGGRESSIVE_OPTIONS
	/* AVR_FAST_CORE_GLOBAL_FLASH_ACCESS
		uses globals for uflash and io_table access, will not work for multiple avr cores */

	#define AVR_FAST_CORE_GLOBAL_FLASH_ACCESS

	#ifndef CONFIG_AVR_FAST_CORE_UINST_PROFILING

		/* AVR_FAST_CORE_TAIL_CALL
			build using tail calls, not all compilers may support the features
			necessary and/or may be compiler dependant. */

		#define AVR_FAST_CORE_TAIL_CALL

	#endif
#endif

/* AVR_FAST_CORE_DECODE_TRAP
	specific trap to catch missing instruction handlers */
/* AVR_FAST_CORE_AGGRESSIVE_CHECKS
	sanity and bounds checking at multiple points. */

static const int _AVR_FAST_CORE_DECODE_TRAP = 0;
//#define AVR_FAST_CORE_AGGRESSIVE_CHECKS

#ifndef CONFIG_SIMAVR_TRACE
/* AVR_CORE_FAST_CORE_BUGS
	Set to leave in known core bugs fixed in fast core and still existing
	in standard core */
/* AVR_CORE_FAST_CORE_DIFF_TRACE
	Some trace statements are slightly different than the original core...
	also, some operations have bug fixes included not in original core.
	defining AVR_CORE_FAST_CORE_DIFF_TRACE returns operation close to original 
	core to make diffing trace output from cores easier for debugging. */
/* AVR_FAST_CORE_LOCAL_TRACE
	set this to bypass mucking with the makefiles and possibly needing to 
	rebuild the entire project... allows tracing specific to the fast core. */
/* AVR_FAST_CORE_ITRACE
	helps to track flash -> uflash instruction opcode as instructions are 
	translated to uoperations...  after which quiets down as instructions 
	are picked up by the faster core loop. */
/* AVR_FAST_CORE_STACK_TRACE
	adds more verbose detail to stack operations */

//#define AVR_CORE_FAST_CORE_DIFF_TRACE
#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
#define AVR_CORE_FAST_CORE_BUGS
#define AVR_CORE_FAST_CORE_DIFF_TRACE
//#define AVR_FAST_CORE_ITRACE
#define AVR_FAST_CORE_LOCAL_TRACE
#define AVR_FAST_CORE_STACK_TRACE
#else
//#define AVR_FAST_CORE_LOCAL_TRACE
#ifdef AVR_FAST_CORE_LOCAL_TRACE
//#define AVR_CORE_FAST_CORE_BUGS
//#define AVR_CORE_FAST_CORE_DIFF_TRACE
#define AVR_FAST_CORE_ITRACE
//#define AVR_FAST_CORE_STACK_TRACE
#endif
#endif
#else
/* do not touch these here...  set above. */
#define AVR_CORE_FAST_CORE_BUGS
#define AVR_CORE_FAST_CORE_DIFF_TRACE
#define AVR_FAST_CORE_LOCAL_TRACE
#define AVR_FAST_CORE_STACK_TRACE
#endif

/* ****
	>>>>	END OF OPTION FLAGS SECTION
**** */

// /--SIM-CORE - Byte addressed pc
//   /--AVR - Word addressed pc
// 1,2,4,8,16,32,64
//     \--SIM-FAST-CORE - Long addressed pc

/* avr pc word addressed, stock core pc byte addressed */
//#define AVR_CORE_FLASH_UFLASH_SIZE_SHIFT(x) (x << 1)
//#define AVR_CORE_FLASH_ADDR_SHIFT(x) (x >> 1)

/* using below, will leave slot between opcode data...  for extra... stuff... .?. .?. ;) */
#define AVR_CORE_FLASH_UFLASH_SIZE_SHIFT(x) (x << 2)
#define AVR_CORE_FLASH_ADDR_SHIFT(x) x

typedef uint_fast8_t (*_avr_fast_core_io_read_fn_t)(avr_t *avr, int_fast32_t *count, uint_fast16_t addr);
typedef void (*_avr_fast_core_io_write_fn_t)(avr_t *avr, int_fast32_t *count, uint_fast16_t addr, uint_fast8_t v);

#define AVR_CORE_FLASH_FAST_CORE_UFLASH ((uint32_t*)&((uint8_t *)avr->flash)[avr->flashend + 1])

#ifdef AVR_FAST_CORE_GLOBAL_FLASH_ACCESS

	static uint32_t *_avr_fast_core_uflash;

	#define AVR_FAST_CORE_GLOBAL_UFLASH_ADDR_AT(addr) _avr_fast_core_uflash[AVR_CORE_FLASH_ADDR_SHIFT(addr)]

	#ifdef AVR_FAST_CORE_IO_DISPATCH_TABLES

		static _avr_fast_core_io_read_fn_t _avr_fast_core_io_read_fn[MAX_IOs];
		static _avr_fast_core_io_write_fn_t _avr_fast_core_io_write_fn[MAX_IOs];

		#define AVR_FAST_CORE_GLOBAL_IO_READ_FN(addr) _avr_fast_core_io_read_fn[addr]
		#define AVR_FAST_CORE_GLOBAL_IO_WRITE_FN(addr) _avr_fast_core_io_write_fn[addr]

	#endif

#elif defined(AVR_FAST_CORE_IO_DISPATCH_TABLES)

	typedef struct _avr_fast_core_data_t {
		_avr_fast_core_io_read_fn_t	io_read_fn[MAX_IOs];
		_avr_fast_core_io_write_fn_t	io_write_fn[MAX_IOs];
		uint32_t 			uflash[];
	}_avr_fast_core_data_t, *_avr_fast_core_data_p;

	#define AVR_FAST_CORE_DATA ((_avr_fast_core_data_p)AVR_CORE_FLASH_FAST_CORE_UFLASH)

	#define AVR_FAST_CORE_DATA_UFLASH_ADDR_AT(addr) AVR_FAST_CORE_DATA->uflash[AVR_CORE_FLASH_ADDR_SHIFT(addr)]

	#define AVR_FAST_CORE_DATA_IO_READ_FN(addr) AVR_FAST_CORE_DATA->io_read_fn[addr]
	#define AVR_FAST_CORE_DATA_IO_WRITE_FN(addr) AVR_FAST_CORE_DATA->io_write_fn[addr]

#else
	
	#define AVR_FAST_CORE_UFLASH_ADDR_AT(addr) AVR_CORE_FLASH_FAST_CORE_UFLASH[AVR_CORE_FLASH_ADDR_SHIFT(addr)]

#endif

#ifdef AVR_FAST_CORE_IO_DISPATCH_TABLES
	#ifdef AVR_FAST_CORE_GLOBAL_FLASH_ACCESS

		#define AVR_FAST_CORE_IO_READ_FN(addr) \
			AVR_FAST_CORE_GLOBAL_IO_READ_FN(addr);
		
		#define AVR_FAST_CORE_IO_WRITE_FN(addr) \
			AVR_FAST_CORE_GLOBAL_IO_WRITE_FN(addr);

		#define AVR_FAST_CORE_IO_READ_FN_DO(avr, count, addr) \
			AVR_FAST_CORE_GLOBAL_IO_READ_FN(addr)(avr, count, addr);

		#define AVR_FAST_CORE_IO_WRITE_FN_DO(avr, count, addr, v) \
			AVR_FAST_CORE_GLOBAL_IO_WRITE_FN(addr)(avr, count, addr, v);

		#define AVR_FAST_CORE_SET_IO_READ_FN_DO_RET(addr, name) \
			AVR_FAST_CORE_GLOBAL_IO_READ_FN(addr) = name; \
			return(name(avr, count, addr));

		#define AVR_FAST_CORE_SET_IO_WRITE_FN_DO_RET(addr, name) \
			AVR_FAST_CORE_GLOBAL_IO_WRITE_FN(addr) = name; \
			return(name(avr, count, addr, v));

	#else /* core data */

		#define AVR_FAST_CORE_IO_READ_FN(addr) \
			AVR_FAST_CORE_DATA_IO_READ_FN(addr);

		#define AVR_FAST_CORE_IO_WRITE_FN(addr) \
			AVR_FAST_CORE_DATA_IO_WRITE_FN(addr);

		#define AVR_FAST_CORE_IO_READ_FN_DO(avr, count, addr) \
			AVR_FAST_CORE_DATA_IO_READ_FN(addr)(avr, count, addr);

		#define AVR_FAST_CORE_IO_WRITE_FN_DO(avr, count, addr, v) \
			AVR_FAST_CORE_DATA_IO_WRITE_FN(addr)(avr, count, addr, v);

		#define AVR_FAST_CORE_SET_IO_READ_FN_DO_RET(addr, name) \
			AVR_FAST_CORE_DATA_IO_READ_FN(addr) = name; \
			return(name(avr, count, addr));

		#define AVR_FAST_CORE_SET_IO_WRITE_FN_DO_RET(addr, name) \
			AVR_FAST_CORE_DATA_IO_WRITE_FN(addr) = name; \
			return(name(avr, count, addr, v));
	#endif	
#else /* !AVR_FAST_CORE_IO_DISPATCH_TABLES */

	#define AVR_FAST_CORE_SET_IO_READ_FN_DO_RET(addr, name) \
		return(name(avr, count, addr));

	#define AVR_FAST_CORE_SET_IO_WRITE_FN_DO_RET(addr, name) \
		return(name(avr, count, addr, v));

#endif

#define CYCLES(x) { if(1 == (x)) { avr->cycle++; (*count)--; } else { avr->cycle += (x); (*count) -= (x); }}

#ifdef AVR_FAST_CORE_BRANCH_HINTS
#define likely(x) __builtin_expect(!!(x),1)
#define unlikely(x) __builtin_expect(!!(x),0)
#else
#define likely(x) x
#define unlikely(x) x
#endif


#ifdef CONFIG_AVR_FAST_CORE_UINST_PROFILING
static avr_cycle_count_t _avr_fast_core_cycle_timer_process(avr_t *avr, avr_cycle_count_t count)
{
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;
	uint_fast8_t pool_count = pool->count;
	avr_cycle_count_t new_count = count;

	if(pool_count) {
		avr_cycle_timer_slot_t  cycle_timer = pool->timer[pool_count-1];
		avr_cycle_count_t when = cycle_timer.when;
		if (when < avr->cycle) {
			AVR_FAST_CORE_PROFILER_PROFILE(timer, new_count = avr_cycle_timer_process(avr));
		}
	}

	return(new_count);
}
#else
#define _avr_fast_core_cycle_timer_process(avr, count) avr_cycle_timer_process(avr)
#endif

#ifdef CONFIG_AVR_FAST_CORE_UINST_PROFILING
static void _avr_fast_core_service_interrupts(avr_t *avr) {
	if(avr->sreg[S_I]) {
		if(avr_has_pending_interrupts(avr)) {
			AVR_FAST_CORE_PROFILER_PROFILE(isr, avr_service_interrupts(avr));
		}
	}
}
#else
#define _avr_fast_core_service_interrupts(avr) avr_service_interrupts(avr);
#endif

#define xSTATE(_f, args...) { \
	printf("%06x: " _f, avr->pc, ## args);\
	}
#define iSTATE(_f, args...) { \
	printf("%06x: " _f, inst_pc, ## args);\
	}

#define xSREG() {\
	printf("%06x: \t\t\t\t\t\t\t\t\tSREG = ", avr->pc); \
	for (int _sbi = 0; _sbi < 8; _sbi++)\
		printf("%c", avr->sreg[_sbi] ? toupper(_sreg_bit_name[_sbi]) : '.');\
	printf("\n");\
	}

#ifdef AVR_FAST_CORE_STACK_TRACE
#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
#define TSTACK(w) w
#define STACK(_f, args...) xSTATE(_f, ## args)
#define STACK_STATE(_f, args...) xSTATE(_f, ## args)
#else
#define TSTACK(w)
#define STACK(_f, args...)
#define STACK_STATE(_f, args...) STATE(_f, ## args)
#endif
#else
#define TSTACK(w)
#define STACK(_f, args...)
#define STACK_STATE(_f, args...) STATE(_f, ## args)
#endif

// SREG bit names
extern const char * _sreg_bit_name;

/*
 * This allows a "special case" to skip instruction tracing when in these
 * symbols since printf() is useful to have, but generates a lot of cycles.
 */
extern int dont_trace(const char * name);
extern int donttrace;

/*
 * "Pretty" register names
 */
extern const char * reg_names[255];
extern const char * avr_regname(uint8_t reg);

#if CONFIG_SIMAVR_TRACE
/*
 * Dump changed registers when tracing
 */
extern void avr_dump_state(avr_t *avr);
#endif

static inline uint_fast8_t _avr_fast_core_data_read(avr_t* avr, uint_fast16_t addr)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely(addr > avr->ramend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.\n", __FUNCTION__, addr);
			CRASH();
		}
	#endif

	return(avr->data[addr]);
}

static inline void _avr_fast_core_data_write(avr_t* avr, uint_fast16_t addr, uint_fast8_t data)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely(addr > avr->ramend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.\n", __FUNCTION__, addr);
			CRASH();
		}
	#endif

	avr->data[addr]=data;
}

static inline uint_fast16_t _avr_bswap16le(uint_fast16_t v)
{
	#if __BYTE_ORDER == __LITTLE_ENDIAN
		return(v);
	#else
		return(((v & 0xff00) >> 8) | ((v & 0x00ff) << 8));
	#endif
}

static inline uint_fast16_t _avr_bswap16be(uint_fast16_t v)
{
	#if __BYTE_ORDER == __LITTLE_ENDIAN
		return(((v & 0xff00) >> 8) | ((v & 0x00ff) << 8));
	#else
		return(v);
	#endif
}

static inline uint_fast16_t _avr_fast_core_fetch16(void* p, uint_fast16_t addr)
{
	return(*((uint16_t *)&((uint8_t *)p)[addr]));
}

static inline void _avr_fast_core_store16(void*p, uint_fast16_t addr, uint_fast16_t data)
{
	*((uint16_t *)&((uint8_t *)p)[addr])=data;
}

static inline void _avr_fast_core_data_mov(avr_t* avr, uint_fast16_t dst, uint_fast16_t src)
{
	avr->data[dst] = avr->data[src];
}

static inline void _avr_fast_core_data_mov16(avr_t* avr, uint_fast16_t dst, uint_fast16_t src)
{
	uint8_t* data = avr->data;

	uint16_t* ptr_src = (uint16_t *)&data[src];
	uint16_t* ptr_dst = (uint16_t *)&data[dst];

	*ptr_dst = *ptr_src;
}

static inline uint_fast16_t _avr_fast_core_data_read16(avr_t* avr, uint_fast16_t addr)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely((addr + 1) > avr->ramend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
			CRASH();
		}
	#endif

	return(_avr_fast_core_fetch16(avr->data, addr));
}

static inline void _avr_fast_core_data_write16(avr_t* avr, uint_fast16_t addr, uint_fast16_t data)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely((addr + 1) > avr->ramend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
			CRASH();
		}
	#endif

	_avr_fast_core_store16(avr->data, addr, data);
}

static inline uint_fast16_t _avr_fast_core_data_read16be(avr_t* avr, uint_fast16_t addr)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely((addr + 1) > avr->ramend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
			CRASH();
		}
	#endif
	
	return(_avr_bswap16be(_avr_fast_core_fetch16(avr->data, addr)));
}

static inline uint_fast16_t _avr_fast_core_data_read16le(avr_t* avr, uint_fast16_t addr)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely((addr + 1) > avr->ramend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
			CRASH();
		}
	#endif

	return(_avr_bswap16le(_avr_fast_core_fetch16(avr->data, addr)));
}

static inline void _avr_fast_core_data_write16be(avr_t* avr, uint_fast16_t addr, uint_fast16_t data)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely((addr + 1) > avr->ramend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
			CRASH();
		}
	#endif

	_avr_fast_core_store16(avr->data, addr, _avr_bswap16be(data));
}

static inline void _avr_fast_core_data_write16le(avr_t* avr, uint_fast16_t addr, uint_fast16_t data)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely((addr + 1) > avr->ramend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
			CRASH();
		}
	#endif

	_avr_fast_core_store16(avr->data, addr, _avr_bswap16le(data));
}

/*
 * Stack pointer access
 */
static inline uint_fast16_t _avr_fast_core_sp_get(avr_t *avr)
{
	#ifdef AVR_FAST_CORE_COMBINING
		return(_avr_fast_core_data_read16le(avr, R_SPL));
	#else
		uint_fast16_t sp = _avr_fast_core_data_read(avr, R_SPL);
		return(sp | (_avr_fast_core_data_read(avr, R_SPH) << 8));
	#endif
}

static inline void _avr_fast_core_sp_set(avr_t *avr, uint_fast16_t sp)
{
	#ifdef AVR_FAST_CORE_COMBINING
		_avr_fast_core_data_write16le(avr, R_SPL, sp);
	#else
		_avr_fast_core_data_write(avr, R_SPH, sp >> 8);
		_avr_fast_core_data_write(avr, R_SPL, sp & 0xff);
	#endif
}

/*
 * Register access funcitons
 */
static inline uint_fast8_t _avr_fast_core_fetch_r(avr_t* avr, uint_fast8_t reg)
{
	return(_avr_fast_core_data_read(avr, reg));
}

static inline void _avr_fast_core_mov_r(avr_t* avr, uint_fast8_t dst, uint_fast8_t src)
{
	_avr_fast_core_data_mov(avr, dst, src);
}
static inline void _avr_fast_core_store_r(avr_t* avr, uint_fast8_t reg, uint_fast8_t v)
{
	_avr_fast_core_data_write(avr, reg, v);
}

static inline uint_fast16_t _avr_fast_core_fetch_r16(avr_t* avr, uint_fast8_t addr)
{
	return(_avr_fast_core_data_read16(avr, addr));
}

static inline void _avr_fast_core_mov_r16(avr_t* avr, uint_fast8_t dst, uint_fast8_t src)
{
	_avr_fast_core_data_mov16(avr, dst, src);
}

static inline void _avr_fast_core_store_r16(avr_t* avr, uint_fast8_t addr, uint_fast16_t data)
{
	_avr_fast_core_data_write16(avr, addr, data);
}

static inline uint_fast16_t _avr_fast_core_fetch_r16le(avr_t* avr, uint_fast8_t addr)
{
	return(_avr_fast_core_data_read16le(avr, addr));
}

static inline void _avr_fast_core_store_r16le(avr_t* avr, uint_fast8_t addr, uint_fast16_t data)
{
	_avr_fast_core_data_write16le(avr, addr, data);
}


/*
 * Flash accessors
 */
static inline uint_fast16_t _avr_fast_core_flash_read16le(avr_t* avr, uint_fast16_t addr)
{
	return(_avr_bswap16le(_avr_fast_core_fetch16(avr->flash, addr)));
}

static inline uint_fast16_t _avr_fast_core_flash_read16be(avr_t* avr, uint_fast16_t addr)
{
	return(_avr_bswap16be(_avr_fast_core_fetch16(avr->flash, addr)));
}


/*
 * CONFIG_SIMAVR_TRACE
 *	functionality has not been checked since the early stages of
 *	making fast core... you have been warned.
 */

#if CONFIG_SIMAVR_TRACE
	/*
	 * Add a "jump" address to the jump trace buffer
	 */
	#define TRACE_JUMP()\
		avr->trace_data->old[avr->trace_data->old_pci].pc = avr->pc;\
		avr->trace_data->old[avr->trace_data->old_pci].sp = _avr_fast_core_sp_get(avr);\
		avr->trace_data->old_pci = (avr->trace_data->old_pci + 1) & (OLD_PC_SIZE-1);\

	#if AVR_STACK_WATCH

		#define STACK_FRAME_PUSH()\
			avr->trace_data->stack_frame[avr->trace_data->stack_frame_index].pc = avr->pc;\
			avr->trace_data->stack_frame[avr->trace_data->stack_frame_index].sp = _avr_fast_core_sp_get(avr);\
			avr->trace_data->stack_frame_index++; 

		#define STACK_FRAME_POP()\
			if (avr->trace_data->stack_frame_index > 0) \
				avr->trace_data->stack_frame_index--;

		#define STACK_FRAME_PUSH()

		#define STACK_FRAME_POP()
	#endif

	/*
	 * Handle "touching" registers, marking them changed.
	 * This is used only for debugging purposes to be able to
	 * print the effects of each instructions on registers
	 */

	#define T(w) w
	#define NO_T(w)

	#define REG_TOUCH(a, r) (a)->trace_data->touched[(r) >> 5] |= (1 << ((r) & 0x1f))
	#define REG_ISTOUCHED(a, r) ((a)->trace_data->touched[(r) >> 5] & (1 << ((r) & 0x1f)))

	#define STATE(_f, args...) { \
		if (avr->trace) {\
			avr_symbol_t *symbol = avr_symbol_for_address(avr, avr->pc >> 1); \
			if (symbol) {\
				const char * symn = symbol->symbol; \
				int dont = 0 && dont_trace(symn);\
				if (dont!=donttrace) { \
					donttrace = dont;\
					DUMP_REG();\
				}\
				if (donttrace==0)\
					printf("%04x: %-25s " _f, avr->pc, symn, ## args);\
			} else \
				printf("%s: %04x: " _f, __FUNCTION__, avr->pc, ## args);\
			}\
		}
	#define SREG() if (avr->trace && donttrace == 0) {\
		printf("%04x: \t\t\t\t\t\t\t\t\tSREG = ", avr->pc); \
		for (int _sbi = 0; _sbi < 8; _sbi++)\
			printf("%c", avr->sreg[_sbi] ? toupper(_sreg_bit_name[_sbi]) : '.');\
		printf("\n");\
		}

#else

	#define REG_TOUCH(a, r)

	#define STACK_FRAME_POP()

	#define STACK_FRAME_PUSH()

	#define TRACE_JUMP()

	#ifdef AVR_FAST_CORE_LOCAL_TRACE
	
		#define T(w) w
	
		#define NO_T(w)

		#define STATE(_f, args...) \
			xSTATE(_f, ## args)
		
		#define SREG() xSREG();

	#else
		#define SREG()

		#ifdef AVR_FAST_CORE_ITRACE
	
			#define T(w) w
	
			#define NO_T(w)
	
			static uint32_t _avr_fast_core_flash_uflash_read_0(avr_t* avr, avr_flashaddr_t addr);
			#define STATE(_f, args...) \
				do { \
					if(0 == _avr_fast_core_flash_uflash_read_0(avr, avr->pc)) \
						xSTATE(_f, ## args); \
				} while(0);

		#else
	
			#define T(w)
	
			#define NO_T(w) w
	
			#define STATE(_f, args...)
		
		#endif
	#endif
#endif

static void _avr_fast_core_reg_io_write_sreg(avr_t *avr, int_fast32_t *count, uint_fast16_t addr, uint_fast8_t v)
{
	AVR_FAST_CORE_PROFILER_PROFILE_START(iow_sreg);
	REG_TOUCH(avr, addr);

	_avr_fast_core_data_write(avr, addr, v);
	SET_SREG_FROM(avr, v);

	if(avr->sreg[S_I] != avr->i_shadow)
		*count = 0;

	SREG();
	AVR_FAST_CORE_PROFILER_PROFILE_STOP(iow_sreg);
}

static void _avr_fast_core_reg_io_write_data(avr_t *avr, int_fast32_t *count, uint_fast16_t addr, uint_fast8_t v)
{
	AVR_FAST_CORE_PROFILER_PROFILE_START(iow_data);
	REG_TOUCH(avr, addr);

	_avr_fast_core_data_write(avr, addr, v);
	AVR_FAST_CORE_PROFILER_PROFILE_STOP(iow_data);
}

static void _avr_fast_core_reg_io_write_wc(avr_t *avr, int_fast32_t *count, uint_fast16_t addr, uint_fast8_t v)
{
	AVR_FAST_CORE_PROFILER_PROFILE_START(iow_wc);
	REG_TOUCH(avr, addr);

	uint8_t io = AVR_DATA_TO_IO(addr);

	avr->io[io].w.c(avr, addr, v, avr->io[io].w.param);

	*count = 0;
	AVR_FAST_CORE_PROFILER_PROFILE_STOP(iow_wc);
}

static void _avr_fast_core_reg_io_write_irq(avr_t *avr, int_fast32_t *count, uint_fast16_t addr, uint_fast8_t v)
{
	AVR_FAST_CORE_PROFILER_PROFILE_START(iow_irq);
	REG_TOUCH(avr, addr);

	uint8_t io = AVR_DATA_TO_IO(addr);

	_avr_fast_core_data_write(avr, addr, v);

	avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
	for (int i = 0; i < 8; i++)
		avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);

	*count = 0;
	AVR_FAST_CORE_PROFILER_PROFILE_STOP(iow_irq);
}

static void _avr_fast_core_reg_io_write_wc_irq(avr_t *avr, int_fast32_t *count, uint_fast16_t addr, uint_fast8_t v)
{
	AVR_FAST_CORE_PROFILER_PROFILE_START(iow_wc_irq);
	REG_TOUCH(avr, addr);

	uint8_t io = AVR_DATA_TO_IO(addr);

	avr->io[io].w.c(avr, addr, v, avr->io[io].w.param);
	*count = 0;

	avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
	for (int i = 0; i < 8; i++)
		avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);

	AVR_FAST_CORE_PROFILER_PROFILE_STOP(iow_wc_irq);
}

static void _avr_fast_core_reg_io_write_trap(avr_t *avr, int_fast32_t *count, uint_fast16_t addr, uint_fast8_t v)
{
	if (addr == R_SREG) {
		AVR_FAST_CORE_SET_IO_WRITE_FN_DO_RET(addr, _avr_fast_core_reg_io_write_sreg);
#ifdef AVR_FAST_CORE_IO_DISPATCH_TABLES
	} else if ((addr == R_SPL) || (addr == R_SPH)) {
		AVR_FAST_CORE_SET_IO_WRITE_FN_DO_RET(addr, _avr_fast_core_reg_io_write_data);
#endif
	} else if (addr > 31) {
		uint8_t io = AVR_DATA_TO_IO(addr);

		if((avr->io[io].w.c) && (avr->io[io].irq)) {
			AVR_FAST_CORE_SET_IO_WRITE_FN_DO_RET(addr, _avr_fast_core_reg_io_write_wc_irq);
		} else if(avr->io[io].w.c) {
			AVR_FAST_CORE_SET_IO_WRITE_FN_DO_RET(addr, _avr_fast_core_reg_io_write_wc);
		} else if(avr->io[io].irq) {
			AVR_FAST_CORE_SET_IO_WRITE_FN_DO_RET(addr, _avr_fast_core_reg_io_write_irq);
		} else {
			AVR_FAST_CORE_SET_IO_WRITE_FN_DO_RET(addr, _avr_fast_core_reg_io_write_data);
		}
	} else {
		AVR_FAST_CORE_SET_IO_WRITE_FN_DO_RET(addr, _avr_fast_core_reg_io_write_data);
	}
}

static inline void _avr_fast_core_reg_io_write(avr_t *avr, int_fast32_t *count, uint_fast16_t addr, uint_fast8_t v)
{
	AVR_FAST_CORE_PROFILER_PROFILE_START(iow);

	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely(addr > avr->ramend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
			CRASH();
		}
	#endif

#ifdef AVR_FAST_CORE_IO_DISPATCH_TABLES
	_avr_fast_core_io_write_fn_t iow_fn = AVR_FAST_CORE_IO_WRITE_FN(addr);
	if(likely(iow_fn)) {
		iow_fn(avr, count, addr, v);
	} else
#endif
		_avr_fast_core_reg_io_write_trap(avr, count, addr, v);

	AVR_FAST_CORE_PROFILER_PROFILE_STOP(iow);
}

/*
 * Set any address to a value; split between registers and SRAM
 */
static inline void _avr_fast_core_store_ram(avr_t *avr, int_fast32_t *count, uint_fast16_t addr, uint_fast8_t v)
{
	if (likely(addr >= 256 && addr <= avr->ramend))
		return(_avr_fast_core_data_write(avr, addr, v));
	else
		return(_avr_fast_core_reg_io_write(avr, count, addr, v));
}

static uint_fast8_t _avr_fast_core_reg_io_read_sreg(avr_t *avr, int_fast32_t *count, uint_fast16_t addr)
{
	AVR_FAST_CORE_PROFILER_PROFILE_START(ior_sreg);

	uint_fast8_t sreg = 0;
	
	READ_SREG_INTO(avr, sreg);
	_avr_fast_core_data_write(avr, R_SREG, sreg);

	AVR_FAST_CORE_PROFILER_PROFILE_STOP(ior_sreg);
	return(sreg);
}

static uint_fast8_t _avr_fast_core_reg_io_read_data(avr_t *avr, int_fast32_t *count, uint_fast16_t addr)
{
	AVR_FAST_CORE_PROFILER_PROFILE_START(ior_data);
	
	uint_fast8_t  data = _avr_fast_core_data_read(avr, addr);
	
	AVR_FAST_CORE_PROFILER_PROFILE_STOP(ior_data);
	return(data);
}

static uint_fast8_t _avr_fast_core_reg_io_read_rc(avr_t *avr, int_fast32_t *count, uint_fast16_t addr)
{
	AVR_FAST_CORE_PROFILER_PROFILE_START(ior_rc);
	uint8_t io = AVR_DATA_TO_IO(addr);

	register uint_fast8_t data = avr->io[io].r.c(avr, addr, avr->io[io].r.param);
	_avr_fast_core_data_write(avr, addr, data);
	
	*count = 0;
	
	AVR_FAST_CORE_PROFILER_PROFILE_STOP(ior_rc);
	return(data);
}

static uint_fast8_t _avr_fast_core_reg_io_read_irq(avr_t *avr, int_fast32_t *count, uint_fast16_t addr)
{
	AVR_FAST_CORE_PROFILER_PROFILE_START(ior_irq);
	uint8_t io = AVR_DATA_TO_IO(addr);

	uint8_t v = _avr_fast_core_data_read(avr, addr);
	avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
	for (int i = 0; i < 8; i++)
		avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);
	*count = 0;
	
	uint_fast8_t data = _avr_fast_core_data_read(avr, addr);
	
	AVR_FAST_CORE_PROFILER_PROFILE_STOP(ior_irq);
	return(data);
}

static uint_fast8_t _avr_fast_core_reg_io_read_rc_irq(avr_t *avr, int_fast32_t *count, uint_fast16_t addr)
{
	AVR_FAST_CORE_PROFILER_PROFILE_START(ior_rc_irq);
	uint8_t io = AVR_DATA_TO_IO(addr);

	uint_fast8_t data = avr->io[io].r.c(avr, addr, avr->io[io].r.param);
	_avr_fast_core_data_write(avr, addr, data);

	avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, data);
	for (int i = 0; i < 8; i++)
		avr_raise_irq(avr->io[io].irq + i, (data >> i) & 1);

	*count = 0;
	
	data = _avr_fast_core_data_read(avr, addr);
	
	AVR_FAST_CORE_PROFILER_PROFILE_STOP(ior_rc_irq);
	return(data);
}

static uint_fast8_t _avr_fast_core_reg_io_read_trap(avr_t *avr, int_fast32_t *count, uint_fast16_t addr)
{
	if (addr == R_SREG) {
		AVR_FAST_CORE_SET_IO_READ_FN_DO_RET(addr, _avr_fast_core_reg_io_read_sreg);
#ifdef AVR_FAST_CORE_IO_DISPATCH_TABLES
	} else if ((addr == R_SPL) || (addr == R_SPH)) {
		AVR_FAST_CORE_SET_IO_READ_FN_DO_RET(addr, _avr_fast_core_reg_io_read_data);
#endif
	} else if (addr > 31) {
		uint8_t io = AVR_DATA_TO_IO(addr);
	
		if((avr->io[io].r.c) && (avr->io[io].irq)) {
			AVR_FAST_CORE_SET_IO_READ_FN_DO_RET(addr, _avr_fast_core_reg_io_read_rc_irq);
		} else if(avr->io[io].r.c) {
			AVR_FAST_CORE_SET_IO_READ_FN_DO_RET(addr, _avr_fast_core_reg_io_read_rc);
		} else if(avr->io[io].irq) {
			AVR_FAST_CORE_SET_IO_READ_FN_DO_RET(addr, _avr_fast_core_reg_io_read_irq);
		} else {
			AVR_FAST_CORE_SET_IO_READ_FN_DO_RET(addr, _avr_fast_core_reg_io_read_data);
		}
	} else {
		AVR_FAST_CORE_SET_IO_READ_FN_DO_RET(addr, _avr_fast_core_reg_io_read_data);
	}
}

static inline uint_fast8_t _avr_fast_core_reg_io_read(avr_t *avr, int_fast32_t *count, uint_fast16_t addr)
{
	uint_fast8_t data;

	AVR_FAST_CORE_PROFILER_PROFILE_START(ior);
	
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely(addr > avr->ramend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
			CRASH();
		}
	#endif

#ifdef AVR_FAST_CORE_IO_DISPATCH_TABLES
	_avr_fast_core_io_read_fn_t ior_fn = AVR_FAST_CORE_IO_READ_FN(addr);
	if(likely(ior_fn)) {
		data = ior_fn(avr, count, addr);
	} else
#endif
		data = _avr_fast_core_reg_io_read_trap(avr, count, addr);
		
	AVR_FAST_CORE_PROFILER_PROFILE_STOP(ior);
	return(data);
}

/*
 * Get a value from SRAM.
 */
static inline uint_fast8_t _avr_fast_core_fetch_ram(avr_t *avr, int_fast32_t *count, uint_fast16_t addr)
{
	uint_fast8_t data;
	if(likely(addr >= 256 && addr <= avr->ramend))
		return(_avr_fast_core_data_read(avr, addr));
	else {
		data = _avr_fast_core_reg_io_read(avr, count, addr);
		return(data);
	}
}

#ifdef AVR_FAST_CORE_READ_MODIFY_WRITE
typedef void *avr_rmw_t;
typedef avr_rmw_t avr_rmw_p;
#else
typedef struct avr_rmw_t {
	union	{
		uint8_t		*b;
		uint16_t	*w;
	}data;

	avr_t			*avr;
	uint16_t		addr;

#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
	int			flags;
#endif
}avr_rmw_t, *avr_rmw_p;
#endif

/*
	generic rmw functions */

static inline uint8_t _avr_fast_core_rmw8_ptr_set_fetch(avr_rmw_p rmw, uint8_t *data)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(!rmw) {
			printf("FAST-CORE (%s): null pointer\n", __FUNCTION__);
			abort();
		}
		
		rmw->flags = 8;
	#endif
	
	#ifdef AVR_FAST_CORE_READ_MODIFY_WRITE
		*(uint8_t **)rmw = data;
	#else
		rmw->data.b = data;
	#endif
	
	return(*data);
}

static inline void _avr_fast_core_rmw8_store(avr_rmw_p rmw, uint8_t data)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(!rmw) {
			printf("FAST-CORE (%s): null pointer\n", __FUNCTION__);
			abort();
		}
		
		if(rmw->flags != 8) {
			printf("FAST-CORE (%s): get from invalid set pointer type, %p[.data: %p .flags: 0x%04x]\n",
				__FUNCTION__, rmw, rmw->data.b, rmw->flags);
		}
	#endif
	
	#ifdef AVR_FAST_CORE_READ_MODIFY_WRITE
		**(uint8_t **)rmw = data;
	#else
		*rmw->data.b = data;
	#endif
}

static inline uint16_t _avr_fast_core_rmw16_ptr_set_fetch(avr_rmw_p rmw, uint16_t *data)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(!rmw) {
			printf("FAST-CORE (%s): null pointer\n", __FUNCTION__);
			abort();
		}
		
		rmw->flags = 16;
	#endif
	
	#ifdef AVR_FAST_CORE_READ_MODIFY_WRITE
		*(uint16_t **)rmw = data;
	#else
		rmw->data.w = data;
	#endif

	return(*data);
}

static inline void _avr_fast_core_rmw16le_store(avr_rmw_p rmw, uint16_t data)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(!rmw) {
			printf("FAST-CORE (%s): null pointer\n", __FUNCTION__);
			abort();
		}
		
		if(rmw->flags != 16) {
			printf("FAST-CORE (%s): get from invalid set pointer type, %p[.data: %p .flags: 0x%04x]\n",
				__FUNCTION__, rmw, rmw->data.w, rmw->flags);
		}
	#endif
	
	#ifdef AVR_FAST_CORE_READ_MODIFY_WRITE
		**(uint16_t **)rmw = data;
	#else
		*rmw->data.w = data;
	#endif
}

static inline uint_fast16_t _avr_fast_core_rmw_fetch16(void * p, uint_fast16_t addr, avr_rmw_p ptr_data)
{
	return(_avr_fast_core_rmw16_ptr_set_fetch(ptr_data, ((uint16_t*)&((uint8_t *)p)[addr])));
}

/*
	avr data function */

static inline uint_fast8_t _avr_fast_core_data_rmw(avr_t* avr, uint_fast16_t addr, avr_rmw_p ptr_reg)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely(addr > avr->ramend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
			CRASH();
		}
	#endif
	
	#ifndef AVR_FAST_CORE_READ_MODIFY_WRITE
		ptr_reg->avr = avr;
		ptr_reg->addr = addr;
	#endif

	return(_avr_fast_core_rmw8_ptr_set_fetch(ptr_reg, &avr->data[addr]));
}

static inline uint_fast16_t _avr_fast_core_data_rmw16le(avr_t* avr, uint_fast16_t addr, avr_rmw_p ptr_reg)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely((addr + 1) > avr->ramend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
			CRASH();
		}
	#endif

	#ifndef AVR_FAST_CORE_READ_MODIFY_WRITE
		ptr_reg->avr = avr;
		ptr_reg->addr = addr;
	#endif
	
	return(_avr_bswap16le(_avr_fast_core_rmw_fetch16(avr->data, addr, ptr_reg)));
}

static inline void _avr_fast_core_rmw_store(avr_rmw_p ptr_reg, uint_fast8_t data)
{
	_avr_fast_core_rmw8_store(ptr_reg, data);
}

static inline void _avr_rmw_write16le(avr_rmw_p ptr_data, uint_fast16_t data)
{
	#ifdef AVR_FAST_CORE_READ_MODIFY_WRITE
		_avr_fast_core_rmw16le_store(ptr_data, _avr_bswap16le(data));
	#else
		ptr_data->avr->data[ptr_data->addr] = data;
	#endif
}

/*
	stack accessors */

static inline uint_fast16_t _avr_fast_core_rmw_sp(avr_t *avr, avr_rmw_p ptr_sp)
{
	return(_avr_fast_core_data_rmw16le(avr, R_SPL, ptr_sp));
}

/*
	avr register functions */

static inline uint_fast8_t _avr_fast_core_rmw_fetch_r(avr_t* avr, uint_fast8_t reg, avr_rmw_p reg_ptr)
{
	return(_avr_fast_core_data_rmw(avr, reg, reg_ptr));
}

static inline uint_fast16_t _avr_fast_core_rmw_fetch_r16le(avr_t* avr, uint_fast8_t addr, avr_rmw_p reg_ptr)
{
	return(_avr_fast_core_data_rmw16le(avr, addr, reg_ptr));
}

/*
 * Stack push accessors. Push/pop 8 and 16 bits
 */
static inline void _avr_fast_core_push8(avr_t *avr, int_fast32_t *count, uint_fast8_t v)
{
	avr_rmw_t ptr_sp;
	uint_fast16_t sp = _avr_fast_core_rmw_sp(avr, &ptr_sp);

	TSTACK(printf("%s @0x%04x[0x%04x]\n", __FUNCTION__, sp, v));
	_avr_fast_core_store_ram(avr, count, sp, v);

	_avr_rmw_write16le(&ptr_sp, sp - 1);
}

static inline uint_fast8_t _avr_fast_core_pop8(avr_t *avr, int_fast32_t *count)
{
	avr_rmw_t ptr_sp;
	uint_fast16_t sp = _avr_fast_core_rmw_sp(avr, &ptr_sp) + 1;

	uint_fast8_t res = _avr_fast_core_fetch_ram(avr, count, sp);
	TSTACK(printf("%s @0x%04x[0x%04x]\n", __FUNCTION__, sp, res));

	_avr_rmw_write16le(&ptr_sp, sp);
	
	return res;
}

static inline void _avr_fast_core_push16xx(avr_t *avr, uint_fast16_t v)
{
	avr_rmw_t ptr_sp;
	uint_fast16_t sp = _avr_fast_core_rmw_sp(avr, &ptr_sp);

	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely(256 > sp)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): stack pointer at 0x%04x, below end of io space, aborting.",
				__FUNCTION__, sp);
			CRASH();
		}
	#endif

	if(likely(sp <= avr->ramend)) {
		_avr_fast_core_data_write16(avr, sp - 1, v);
		_avr_rmw_write16le(&ptr_sp, sp - 2);
	} else {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): stack pointer at 0x%04x, above ramend... aborting.",
			__FUNCTION__, sp);
		CRASH();
	}
}

static inline void _avr_fast_core_push16be(avr_t *avr, int_fast32_t *count, uint_fast16_t v)
{
	#ifdef AVR_FAST_CORE_COMBINING
		TSTACK(uint_fast16_t sp = _avr_fast_core_sp_get(avr));
		STACK("push.w ([%02x]@%04x):([%02x]@%04x)\n", 
			v >> 8, sp - 1, v & 0xff, sp);
		_avr_fast_core_push16xx(avr, _avr_bswap16be(v));
	#else
		_avr_fast_core_push8(avr, count, v);
		_avr_fast_core_push8(avr, count, v >> 8);
	#endif
}

static inline void _avr_fast_core_push16le(avr_t *avr, int_fast32_t *count, uint_fast16_t v)
{
	#ifdef AVR_FAST_CORE_COMBINING
		TSTACK(uint_fast16_t sp = _avr_fast_core_sp_get(avr));
		STACK("push.w ([%02x]@%04x):([%02x]@%04x)\n", 
			v & 0xff, sp - 1, v >> 8, sp);
		_avr_fast_core_push16xx(avr, _avr_bswap16le(v));
	#else
		_avr_fast_core_push8(avr, count, v >> 8);
		_avr_fast_core_push8(avr, count, v);
	#endif
}

static inline uint_fast16_t _avr_fast_core_pop16xx(avr_t *avr)
{
	avr_rmw_t ptr_sp;
	uint_fast16_t sp = _avr_fast_core_rmw_sp(avr, &ptr_sp) + 2;

	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely(256 > sp)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): stack pointer at 0x%04x, below end of io space, aborting.",
				__FUNCTION__, sp);
			CRASH();
		}
	#endif

	if(likely(sp <= avr->ramend)) {
		uint_fast16_t data = _avr_fast_core_data_read16(avr, sp - 1);
		_avr_rmw_write16le(&ptr_sp, sp);
		return(data);
	} else {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): stack pointer at 0x%04x, above ramend... aborting.",
			__FUNCTION__, sp);
		CRASH();
	}

	return(0);
}

static inline uint_fast16_t _avr_fast_core_pop16be(avr_t *avr, int_fast32_t *count)
{
	uint_fast16_t data;
	
	#ifdef AVR_FAST_CORE_COMBINING
		TSTACK(uint_fast16_t sp = _avr_fast_core_sp_get(avr));
		data = _avr_bswap16be(_avr_fast_core_pop16xx(avr));
		STACK("pop.w ([%02x]@%04x):([%02x]@%04x)\n", 
			data >> 8, sp + 1, data & 0xff, sp + 2);
	#else
		data = _avr_fast_core_pop8(avr, count) << 8;
		data |= _avr_fast_core_pop8(avr, count);
	#endif

	return(data);
}

static inline uint_fast16_t _avr_fast_core_pop16le(avr_t *avr, int_fast32_t *count)
{
	uint_fast16_t data;

	#ifdef AVR_FAST_CORE_COMBINING
		TSTACK(uint_fast16_t sp = _avr_fast_core_sp_get(avr));
		data = _avr_bswap16le(_avr_fast_core_pop16xx(avr));
		STACK("pop.w ([%02x]@%04x):([%02x]@%04x)\n",
			data & 0xff, sp + 1, data >> 8, sp + 2);
	#else
		data = _avr_fast_core_pop8(avr, count);
		data |= (_avr_fast_core_pop8(avr, count) << 8);
	#endif

	return(data);
}

static inline void _avr_fast_core_push24be(avr_t *avr, int_fast32_t *count, uint_fast32_t v)
{
	_avr_fast_core_push16be(avr, count, v & 0xffff);
	_avr_fast_core_push8(avr, count, v >> 16);
}

static inline uint_fast32_t _avr_fast_core_pop24be(avr_t *avr, int_fast32_t *count)
{
	uint_fast32_t res = _avr_fast_core_pop8(avr, count) << 16;
	return(res |= _avr_fast_core_pop16be(avr, count));
}

static inline int _avr_is_instruction_32_bits(avr_t *avr, avr_flashaddr_t pc)
{
	int o = (_avr_fast_core_flash_read16le(avr, pc)) & 0xfc0f;
	
	return	o == 0x9200 || // STS ! Store Direct to Data Space
			o == 0x9000 || // LDS Load Direct from Data Space
			o == 0x940c || // JMP Long Jump
			o == 0x940d || // JMP Long Jump
			o == 0x940e ||  // CALL Long Call to sub
			o == 0x940f; // CALL Long Call to sub
}

static inline void _avr_fast_core_flags_zc16(avr_t* avr, const uint_fast16_t res)
{
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = (res >> 15) & 1;
}


static inline void _avr_fast_core_flags_zcnvs(avr_t* avr, const uint_fast8_t res, const uint_fast8_t vr)
{
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vr & 1;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_fast_core_flags_zcn0vs(avr_t* avr, const uint_fast8_t res, const uint_fast8_t vr)
{
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vr & 1;
	avr->sreg[S_N] = 0;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_fast_core_flags_zcnvs16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t vr)
{
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vr & 1;
	avr->sreg[S_N] = (res >> 15) & 1;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_fast_core_flags_zcn0vs16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t vr)
{
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vr & 1;
	avr->sreg[S_N] = 0;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_fast_core_flags_zns(avr_t* avr, const uint_fast8_t res)
{
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_fast_core_flags_znvs(avr_t* avr, const uint_fast8_t res)
{
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_fast_core_flags_Rzns(avr_t* avr, const uint_fast8_t res)
{
	if (res)
		avr->sreg[S_Z] = 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_fast_core_flags_zns16(avr_t* avr, const uint_fast16_t res)
{
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 15) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_fast_core_flags_Rzns16(avr_t* avr, const uint_fast16_t res)
{
	if (res)
		avr->sreg[S_Z] = 0;
	avr->sreg[S_N] = (res >> 15) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_fast_core_flags_znv0s(avr_t* avr, const uint_fast8_t res)
{
	avr->sreg[S_V] = 0;
	_avr_fast_core_flags_zns(avr, res);
}

static inline void _avr_fast_core_flags_znv0s16(avr_t* avr, const uint_fast16_t res)
{
	avr->sreg[S_V] = 0;
	_avr_fast_core_flags_zns16(avr, res);
}

/* solutions pulled from NO EXECUTE website, bochs, qemu */
static inline void _avr_fast_core_flags_add(avr_t* avr, const uint_fast8_t res, const uint_fast8_t rd, const uint_fast8_t rr)
{
	uint_fast8_t xvec = (rd ^ rr);
	uint_fast8_t ovec = (rr ^ res) & ~xvec;
	uint_fast8_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 3) & 1;
	avr->sreg[S_C] = (rxor >> 7) & 1;

	avr->sreg[S_V] = (ovec >> 7) & 1;
}

static inline void _avr_fast_core_flags_add_zns(avr_t* avr, const uint_fast8_t res, const uint_fast8_t rd, const uint_fast8_t rr)
{
	uint_fast8_t xvec = (rd ^ rr);
	uint_fast8_t ovec = (rr ^ res) & ~xvec;
	uint_fast8_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 3) & 1;
	avr->sreg[S_C] = (rxor >> 7) & 1;

	avr->sreg[S_V] = (ovec >> 7) & 1;

	/* avr_flags_zns */
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_fast_core_flags_add16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t rd, const uint_fast16_t rr)
{
	uint_fast16_t xvec = (rd ^ rr);
	uint_fast16_t ovec = (rr ^ res) & ~xvec;
	uint_fast16_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 11) & 1;
	avr->sreg[S_C] = (rxor >> 15) & 1;

	avr->sreg[S_V] = (ovec >> 15) & 1;
}

static inline void _avr_fast_core_flags_add16_zns16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t rd, const uint_fast16_t rr)
{
	uint_fast16_t xvec = (rd ^ rr);
	uint_fast16_t ovec = (rr ^ res) & ~xvec;
	uint_fast16_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 11) & 1;
	avr->sreg[S_C] = (rxor >> 15) & 1;

	avr->sreg[S_V] = (ovec >> 15) & 1;

	/* avr_flags_zns16 */
	
/*	NOTE: about flag behavior...
	certain flags, despite logic where concerned with the combination of
		add + adc and other combinations only pertain to the data relevant
		to the adc instruction, if handled as and entire quantity produces
		incorrect results...  this is taken into accunt in the 
		_avr_fast_core_flags_add16_zns16 function...
		
		prior implimentation was:
			_avr_fast_core_flags_add16(avr, res, vd, vr);
			_avr_fast_core_flags_zns16(avr, res & 0xff00);
		
		in particular it was found that zero flag was incorrectly set. */

	avr->sreg[S_Z] = (res & 0xff00) == 0;
	avr->sreg[S_N] = (res >> 15) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_fast_core_flags_sub(avr_t* avr, const uint_fast8_t res, const uint_fast8_t rd, const uint_fast8_t rr)
{
	uint_fast8_t xvec = (rd ^ rr);
	uint_fast8_t ovec = (rd ^ res) & xvec;
	uint_fast8_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 3) & 1;
	avr->sreg[S_C] = (rxor >> 7) & 1;

	avr->sreg[S_V] = (ovec >> 7) & 1;
}

static inline void _avr_fast_core_flags_sub_zns(avr_t* avr, const uint_fast8_t res, const uint_fast8_t rd, const uint_fast8_t rr)
{
	uint_fast8_t xvec = (rd ^ rr);
	uint_fast8_t ovec = (rd ^ res) & xvec;
	uint_fast8_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 3) & 1;
	avr->sreg[S_C] = (rxor >> 7) & 1;

	avr->sreg[S_V] = (ovec >> 7) & 1;

	/* avr_flags_zns */
	
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_fast_core_flags_sub_Rzns(avr_t* avr, const uint_fast8_t res, const uint_fast8_t rd, const uint_fast8_t rr)
{
	uint_fast8_t xvec = (rd ^ rr);
	uint_fast8_t ovec = (rd ^ res) & xvec;
	uint_fast8_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 3) & 1;
	avr->sreg[S_C] = (rxor >> 7) & 1;

	avr->sreg[S_V] = (ovec >> 7) & 1;

	/* avr_flags_zns */
	
	if (res)
		avr->sreg[S_Z] = 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_fast_core_flags_sub16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t rd, const uint_fast16_t rr)
{
	uint_fast16_t xvec = (rd ^ rr);
	uint_fast16_t ovec = (rd ^ res) & xvec;
	uint_fast16_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 11) & 1;
	avr->sreg[S_C] = (rxor >> 15) & 1;

	avr->sreg[S_V] = (ovec >> 15) & 1;
}

static inline void _avr_fast_core_flags_sub16_zns16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t rd, const uint_fast16_t rr)
{
	uint_fast16_t xvec = (rd ^ rr);
	uint_fast16_t ovec = (rd ^ res) & xvec;
	uint_fast16_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 11) & 1;
	avr->sreg[S_C] = (rxor >> 15) & 1;

	avr->sreg[S_V] = (ovec >> 15) & 1;

	/* avr_flags_zns16 */

	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 15) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

#define AVR_FAST_CORE_CPI_BRXX_GROUP \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k8o7_cpi_brcc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k8o7_cpi_brcs) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k8o7_cpi_breq) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k8o7_cpi_brne)

#define AVR_FAST_CORE_UINST_ESAC_TABLE_DEFN \
		AVR_FAST_CORE_UINST_ESAC_DEFN(avr_decode_one) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_adc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_add) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_add_adc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(p2k6_adiw) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_and) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k8_andi) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k16_andi_andi) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4r5k8_andi_or) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k8k8_andi_ori) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_asr) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(b3_bclr) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5m8_bld) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(o7_brcc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(o7_brcs) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(o7_breq) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(o7_brne) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(o7_brpl) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(b3o7_brxc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(b3o7_brxs) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(b3_bset) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5b3_bst) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(x22_call) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(a5m8_cbi) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(cli) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_clr) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_com) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_cp) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_cp_cpc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5o7_cp_cpc_brne) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_cpc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k8_cpi) \
			AVR_FAST_CORE_CPI_BRXX_GROUP \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4r5k8_cpi_cpc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_16_cpse) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_32_cpse) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_dec) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(x22_eind_call) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(x_eind_eicall) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(x_eind_eijmp) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(o12_eind_rcall) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(x_eind_ret) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(x_eind_reti) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_eor) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(x_icall) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(x_ijmp) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5a6_in) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5a6k8_in_andi_out) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5a6k8_in_ori_out) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5a6_in_push) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5a6m8_in_sbrs) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_inc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(x22_jmp) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5rXYZ_ld_no_op) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5rXYZ_ld_pre_dec) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5rXYZ_ld_post_inc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5rYZq6_ldd) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5rYZq6_ldd_ldd) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k8_ldi) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k16_ldi_ldi) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k8a6_ldi_out) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5x16_lds) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5x16_lds_no_io) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5x16_lds_lds) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5x16_lds_lds_no_io) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5x16_lds_no_io_tst) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5x16_lds_tst) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_lpm_z) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5rXYZ_lpm_z_post_inc_st_post_inc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_lpm_z_post_inc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_lpm16_z_post_inc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_lsl) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_lsl_lsl) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_lsl_rol) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_lsr) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_lsr_lsr) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_lsr_ror) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_mov) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d4r4_movw) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_mul) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d16r16_muls) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_neg) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(x_nop) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_or) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k8_ori) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5a6_out) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_out_sph_sreg_spl) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_pop) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5a6_pop_out) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_pop_pop16be) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_pop_pop16le) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_push) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_push_push16be) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_push_push16le) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(o12_rcall) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(x_ret) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(x_reti) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(o12_rjmp) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_rol) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_ror) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_sbc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k8_sbci) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(a5m8_sbi) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(a5m8_16_sbic) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(a5m8_32_sbic) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(a5m8_16_sbis) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(a5m8_32_sbis) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(p2k6_sbiw) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5m8_16_sbrc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5m8_32_sbrc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5m8_16_sbrs) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5m8_32_sbrs) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(sei) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(sei_sleep) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(x_sleep) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5rXYZ_st_no_op) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5rXYZ_st_pre_dec) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5rXYZ_st_post_inc) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5rYZq6_std) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5rYZq6_std_std_hhll) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5rYZq6_std_std_hllh) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5x16_sts) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5x16_sts_no_io) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5x16_sts_sts) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5x16_sts_sts_no_io) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5r5_sub) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k8_subi) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(h4k16_subi_sbci) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_swap) \
		AVR_FAST_CORE_UINST_ESAC_DEFN(d5_tst)

#undef AVR_FAST_CORE_UINST_ESAC_DEFN
#define AVR_FAST_CORE_UINST_ESAC_DEFN(name) _avr_fast_core_uinst_##name##_k,

enum {
	AVR_FAST_CORE_UINST_ESAC_TABLE_DEFN
};

static inline void _avr_fast_core_flash_uflash_write_0(avr_t* avr, avr_flashaddr_t addr, uint_fast32_t data)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely(addr > avr->flashend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of flash, aborting.", __FUNCTION__, addr);
			CRASH();
		}
	#endif

	#ifdef AVR_FAST_CORE_GLOBAL_FLASH_ACCESS
		AVR_FAST_CORE_GLOBAL_UFLASH_ADDR_AT(addr) = data;
	#elif defined(AVR_FAST_CORE_IO_DISPATCH_TABLES)
		AVR_FAST_CORE_DATA_UFLASH_ADDR_AT(addr) = data;
	#else
		AVR_FAST_CORE_UFLASH_ADDR_AT(addr) = data;
	#endif
		
}

static inline uint_fast32_t _avr_fast_core_flash_uflash_read_0(avr_t* avr, avr_flashaddr_t addr)
{
	#ifdef AVR_FAST_CORE_AGRESSIVE_CHECKS
		if(unlikely(addr > avr->flashend)) {
			AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of flash, aborting.", __FUNCTION__, addr);
			CRASH();
		}
	#endif

	#ifdef AVR_FAST_CORE_GLOBAL_FLASH_ACCESS
		return(AVR_FAST_CORE_GLOBAL_UFLASH_ADDR_AT(addr));
	#elif defined(AVR_FAST_CORE_IO_DISPATCH_TABLES)
		return(AVR_FAST_CORE_DATA_UFLASH_ADDR_AT(addr));
	#else
		return(AVR_FAST_CORE_UFLASH_ADDR_AT(addr));
	#endif
}

#define AVR_FAST_CORE_UINST_DEFN_vIO(io) \
	uint_fast8_t v##io = _avr_fast_core_reg_io_read(avr, count, io)

#define AVR_FAST_CORE_UINST_DEFN_Rv(rx) \
	uint_fast8_t v##rx = _avr_fast_core_fetch_r(avr, rx)

#ifdef AVR_FAST_CORE_COMMON_DATA
#define AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv(rx) \
	uint_fast8_t v##rx = data[rx]
#else
#define AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv(rx) \
	AVR_FAST_CORE_UINST_DEFN_Rv(rx)
#endif

#define AVR_FAST_CORE_UINST_DEFN_rmwRv(rx) \
	avr_rmw_t rmw_##rx; \
	uint_fast8_t v##rx = _avr_fast_core_rmw_fetch_r(avr, rx, &rmw_##rx);

#ifdef AVR_FAST_CORE_COMMON_DATA
#define AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_rmwRv(rx) \
	avr_rmw_t rmw_##rx; \
	uint_fast8_t v##rx = _avr_fast_core_rmw8_ptr_set_fetch(&rmw_##rx, &data[rx]);
#else
#define AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_rmwRv(rx) \
	AVR_FAST_CORE_UINST_DEFN_rmwRv(rx)
#endif

#define AVR_FAST_CORE_UINST_DEFN_Rv16(rx) \
	uint_fast16_t v##rx = _avr_fast_core_fetch_r16(avr, rx)

#define AVR_FAST_CORE_UINST_DEFN_Rv16le(rx) \
	uint_fast16_t v##rx = _avr_fast_core_fetch_r16le(avr, rx)

#ifdef AVR_FAST_CORE_COMMON_DATA
#define AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv16le(rx) \
	uint_fast16_t v##rx = _avr_bswap16le(*((uint16_t *)&data[rx]))
#else
#define AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv16le(rx) \
	AVR_FAST_CORE_UINST_DEFN_Rv16le(rx)
#endif

#define AVR_FAST_CORE_UINST_DEFN_rmwRv16le(rx) \
	avr_rmw_t rmw_##rx; \
	uint_fast16_t v##rx = _avr_fast_core_rmw_fetch_r16le(avr, rx, &rmw_##rx);

#ifdef AVR_FAST_CORE_COMMON_DATA
#define AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_rmwRv16le(rx) \
	avr_rmw_t rmw_##rx = (avr_rmw_p)&data[rx]; \
	uint_fast16_t v##rx = _avr_bswap16le(_avr_fast_core_rmw16_ptr_set_fetch(&rmw_##rx, (uint16_t *)&data[rx]));
#else
#define AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_rmwRv16le(rx) \
	AVR_FAST_CORE_UINST_DEFN_rmwRv16le(rx)
#endif

#define AVR_FAST_CORE_RMW_STORE_R(r0, data) \
	_avr_fast_core_rmw_store(&rmw_##r0, data)

#define AVR_FAST_CORE_RMW_STORE_R16LE(r0, data) \
	_avr_fast_core_rmw16le_store(&rmw_##r0, data)

#define AVR_FAST_CORE_UINST_R0(xu_opcode) (xu_opcode & 0xff)
#define AVR_FAST_CORE_UINST_R1(xu_opcode) ((xu_opcode >> 8) & 0xff)
#define AVR_FAST_CORE_UINST_24R1(xu_opcode) (xu_opcode >> 8)
#define AVR_FAST_CORE_UINST_R2(xu_opcode) ((xu_opcode >> 16) & 0xff)
#define AVR_FAST_CORE_UINST_16R2(xu_opcode) ((xu_opcode >> 16) & 0xffff)
#define AVR_FAST_CORE_UINST_R3(xu_opcode) ((xu_opcode >> 24) & 0xff)

#define AVR_FAST_CORE_UINST_DEFN_R0(xu_opcode, r0) uint_fast8_t r0 = AVR_FAST_CORE_UINST_R0(xu_opcode)
#define AVR_FAST_CORE_UINST_DEFN_R1(xu_opcode, r1) uint_fast8_t r1 = AVR_FAST_CORE_UINST_R1(xu_opcode)
#define AVR_FAST_CORE_UINST_DEFN_R2(xu_opcode, r2) uint_fast8_t r2 = AVR_FAST_CORE_UINST_R2(xu_opcode)
#define AVR_FAST_CORE_UINST_DEFN_R3(xu_opcode, r3) uint_fast8_t r3 = AVR_FAST_CORE_UINST_R3(xu_opcode)

#define AVR_FAST_CORE_UINST_DEFN_iR1(xu_opcode, r1) int_fast8_t r1 = AVR_FAST_CORE_UINST_R1(xu_opcode)
#define AVR_FAST_CORE_UINST_DEFN_iR2(xu_opcode, r2) int_fast8_t r2 = AVR_FAST_CORE_UINST_R2(xu_opcode)
#define AVR_FAST_CORE_UINST_DEFN_iR3(xu_opcode, r3) int_fast8_t r3 = AVR_FAST_CORE_UINST_R3(xu_opcode)

#define AVR_FAST_CORE_UINST_DEFN_24R1(xu_opcode, x24) uint_fast32_t x24 = AVR_FAST_CORE_UINST_24R1(xu_opcode)
#define AVR_FAST_CORE_UINST_DEFN_16R2(xu_opcode, x16) uint_fast16_t x16 = AVR_FAST_CORE_UINST_16R2(xu_opcode)

#define AVR_FAST_CORE_UINST_DEFN_R1_iR2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1(xu_opcode, r1); \
	AVR_FAST_CORE_UINST_DEFN_iR2(xu_opcode, r2)

#define AVR_FAST_CORE_UINST_DEFN_R1_16R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1(xu_opcode, r1); \
	AVR_FAST_CORE_UINST_DEFN_16R2(xu_opcode, r2)


#define AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1(xu_opcode, r1); \
	AVR_FAST_CORE_UINST_DEFN_R2(xu_opcode, r2)

#define AVR_FAST_CORE_UINST_DEFN_R1_R2_R3(xu_opcode, r1, r2, r3) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_R3(xu_opcode, r3)


#define AVR_FAST_CORE_UINST_DEFN_R1v(xu_opcode, r1) \
	AVR_FAST_CORE_UINST_DEFN_R1(xu_opcode, r1); \
	AVR_FAST_CORE_UINST_DEFN_Rv(r1)

#define AVR_FAST_CORE_UINST_DEFN_R2v(xu_opcode, r2) \
	AVR_FAST_CORE_UINST_DEFN_R2(xu_opcode, r2); \
	AVR_FAST_CORE_UINST_DEFN_Rv(r2)


#define AVR_FAST_CORE_UINST_DEFN_R1v_16R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1v(xu_opcode, r1); \
	AVR_FAST_CORE_UINST_DEFN_16R2(xu_opcode, r2)

#define AVR_FAST_CORE_UINST_DEFN_R1v_R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_Rv(r1)

#ifdef AVR_FAST_CORE_COMMON_DATA
#define AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_R1v_R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	uint8_t *data = avr->data; \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv(r1)
#else
#define AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_R1v_R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv(r1)
#endif

#define AVR_FAST_CORE_UINST_DEFN_R1v_R2v(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_R1v_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv(r2)

#define AVR_FAST_CORE_UINST_DEFN_R1v_R2v_R3(xu_opcode, r1, r2, r3) \
	AVR_FAST_CORE_UINST_DEFN_R1v_R2v(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_R3(xu_opcode, r3)

#define AVR_FAST_CORE_UINST_DEFN_R1v_R2v_R3v(xu_opcode, r1, r2, r3) \
	AVR_FAST_CORE_UINST_DEFN_R1v_R2v_R3(xu_opcode, r1, r2, r3); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv(r3)

#define AVR_FAST_CORE_UINST_DEFN_R1v_R2v16le(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_R1v_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv16le(r2)


#define AVR_FAST_CORE_UINST_DEFN_R1v_R2v16le_R3(xu_opcode, r1, r2, r3) \
	AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_R1v_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv16le(r2); \
	AVR_FAST_CORE_UINST_DEFN_R3(xu_opcode, r3)

#define AVR_FAST_CORE_UINST_DEFN_R1v_rmwR2v16le(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_R1v_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_rmwRv16le(r2)

#define AVR_FAST_CORE_UINST_DEFN_rmwR1v(xu_opcode, r1) \
	AVR_FAST_CORE_UINST_DEFN_R1(xu_opcode, r1); \
	AVR_FAST_CORE_UINST_DEFN_rmwRv(r1)

#ifdef AVR_FAST_CORE_COMMON_DATA
#define AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_rmwR1v_R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	uint8_t *data = avr->data; \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_rmwRv(r1)
#else
#define AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_rmwR1v_R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_rmwRv(r1)
#endif

#define AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_rmwRv(r1)

#define AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2_R3(xu_opcode, r1, r2, r3) \
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_R3(xu_opcode, r3)


#define AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2v(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_rmwR1v_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv(r2)

#define AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2v_R3(xu_opcode, r1, r2, r3) \
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2v(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_R3(xu_opcode, r3)


#define AVR_FAST_CORE_UINST_DEFN_v16R1_16R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1(xu_opcode, r1); \
	AVR_FAST_CORE_UINST_DEFN_Rv16(r1); \
	AVR_FAST_CORE_UINST_DEFN_16R2(xu_opcode, r2)

#define AVR_FAST_CORE_UINST_DEFN_R1v16le(xu_opcode, r1) \
	AVR_FAST_CORE_UINST_DEFN_R1(xu_opcode, r1); \
	AVR_FAST_CORE_UINST_DEFN_Rv16le(r1)

#define AVR_FAST_CORE_UINST_DEFN_R2v16le(xu_opcode, r2) \
	AVR_FAST_CORE_UINST_DEFN_R2(xu_opcode, r2); \
	AVR_FAST_CORE_UINST_DEFN_Rv16le(r2)


#define AVR_FAST_CORE_UINST_DEFN_rmwR1v16le(xu_opcode, r1) \
	AVR_FAST_CORE_UINST_DEFN_R1(xu_opcode, r1); \
	AVR_FAST_CORE_UINST_DEFN_rmwRv16le(r1)

#define AVR_FAST_CORE_UINST_DEFN_R1v16le_16R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1v16le(xu_opcode, r1); \
	AVR_FAST_CORE_UINST_DEFN_16R2(xu_opcode, r2)

#define AVR_FAST_CORE_UINST_DEFN_rmwR1v16le_R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_rmwRv16le(r1)

#ifdef AVR_FAST_CORE_COMMON_DATA
#define AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_rmwR1v16le_R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	uint8_t *data = avr->data; \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_rmwRv16le(r1)
#else
#define AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_rmwR1v16le_R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_rmwRv16le(r1)
#endif

#define AVR_FAST_CORE_UINST_DEFN_R1v16le_R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_Rv16le(r1)

#ifdef AVR_FAST_CORE_COMMON_DATA
#define AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_R1v16le_R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	uint8_t *data = avr->data; \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv16le(r1)
#else
#define AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_R1v16le_R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv16le(r1)
#endif

#define AVR_FAST_CORE_UINST_DEFN_R1v16le_R2v(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_R1v16le_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv(r2)

#define AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_R1v16le_R2v(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_R1v16le_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv(r2)

#define AVR_FAST_CORE_UINST_DEFN_R1v16le_R2v_R3(xu_opcode, r1, r2, r3) \
	AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_R1v16le_R2v(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_R3(xu_opcode, r3)

#define AVR_FAST_CORE_UINST_DEFN_R1v16le_R2v16le(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_R1v16le_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv16le(r2)

#define AVR_FAST_CORE_UINST_DEFN_R1v16le_R2v16le_R3(xu_opcode, r1, r2, r3) \
	AVR_FAST_CORE_UINST_DEFN_R1v16le_R2v16le(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_R3(xu_opcode, r3)

#define AVR_FAST_CORE_UINST_DEFN_rmwR1v16le_R2v16le(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_COMMON_DATA_DEFN_rmwR1v16le_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_COMMON_DATA_Rv16le(r2)

#define AVR_FAST_CORE_UINST_DEFN_R1_R2v16le(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_Rv16le(r2)

#define AVR_FAST_CORE_UINST_DEFN_R1_R2v16le_R3(xu_opcode, r1, r2, r3) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2_R3(xu_opcode, r1, r2, r3); \
	AVR_FAST_CORE_UINST_DEFN_Rv16le(r2)

#define AVR_FAST_CORE_UINST_DEFN_R1_rmwR2v16le(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1_R2(xu_opcode, r1, r2); \
	AVR_FAST_CORE_UINST_DEFN_rmwRv16le(r2)


#define AVR_FAST_CORE_UINST_DEFN_rmwR1v16le_16R2(xu_opcode, r1, r2) \
	AVR_FAST_CORE_UINST_DEFN_R1(xu_opcode, r1); \
	AVR_FAST_CORE_UINST_DEFN_16R2(xu_opcode, r2); \
	AVR_FAST_CORE_UINST_DEFN_rmwRv16le(r1)

#define AVR_FAST_CORE_UFLASH_OPCODE_FETCH(xxu_opcode, addr) xxu_opcode = _avr_fast_core_flash_uflash_read_0(avr, addr)

#define AVR_FAST_CORE_UFLASH_OPCODE_FETCH_DEFN(xau_opcode, addr) \
	uint_fast32_t AVR_FAST_CORE_UFLASH_OPCODE_FETCH(xau_opcode, addr)
#define AVR_FAST_CORE_UFLASH_OPCODE_FETCH_DEFN2(xbu_opcode, addr) \
	uint_fast32_t xbu_opcode = _avr_fast_core_flash_uflash_read_1(avr, addr)

#define AVR_FAST_CORE_MAKE_UINST_OPCODE(xu_opcode, r1, r2, r3) ((r3<<24)|(r2<<16)|(r1<<8)|(_avr_fast_core_uinst_##xu_opcode##_k))
#define AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, r1, r2, r3) ((r3<<24)|(r2<<16)|(r1<<8)|(r0))

static inline uint_fast8_t _avr_fast_core_inst_decode_b3(const uint_fast16_t o) {
	return(o & 0x0007);
}

static inline uint_fast8_t _avr_fast_core_inst_decode_d4(const uint_fast16_t o) {
	return((o & 0x00f0) >> 4);
}

static inline uint_fast8_t _avr_fast_core_inst_decode_d5(const uint_fast16_t o) {
	return((o & 0x01f0) >> 4);
}

static inline uint_fast8_t _avr_fast_core_inst_decode_r4(const uint_fast16_t o) {
	return(o & 0x000f);
}

#define AVR_FAST_CORE_FLASH_OPCODE_FETCH(xxi_opcode, addr) xxi_opcode = _avr_fast_core_flash_read16le(avr, addr)
#define AVR_FAST_CORE_FLASH_OPCODE_DEFN(xxi_opcode, addr) uint_fast16_t xxi_opcode = _avr_fast_core_flash_read16le(avr, addr)

#define AVR_FAST_CORE_INST_DEFN_A5(a5, xi_opcode) uint_fast8_t a5 = ( 32 + ((xi_opcode & 0x00f8) >> 3) )
#define AVR_FAST_CORE_INST_DEFN_A6(a6, xi_opcode) uint_fast8_t a6 = ( 32 + ( ((xi_opcode & 0x0600) >> 5) | _avr_fast_core_inst_decode_r4(xi_opcode) ) )
#define AVR_FAST_CORE_INST_DEFN_B3a(b3, xi_opcode) uint_fast8_t b3 = _avr_fast_core_inst_decode_b3(xi_opcode)
#define AVR_FAST_CORE_INST_DEFN_B3b(b3, xi_opcode) uint_fast8_t b3 = ((xi_opcode & 0x0070) >> 4)
#define AVR_FAST_CORE_INST_DEFN_D4(d4, xi_opcode) uint_fast8_t d4 = _avr_fast_core_inst_decode_d4(xi_opcode)
#define	AVR_FAST_CORE_INST_DEFN_D5(d5, xi_opcode) uint_fast8_t d5 = _avr_fast_core_inst_decode_d5(xi_opcode)
#define AVR_FAST_CORE_INST_DEFN_D16(d16, xi_opcode) uint_fast8_t d16 = (16 + _avr_fast_core_inst_decode_d4(xi_opcode))
#define AVR_FAST_CORE_INST_DEFN_H4(h4, xi_opcode) uint_fast8_t h4 = (16 + _avr_fast_core_inst_decode_d4(xi_opcode))
#define AVR_FAST_CORE_INST_DEFN_K6(k6, xi_opcode) uint_fast8_t k6 = (((xi_opcode & 0x00c0) >> 2) | _avr_fast_core_inst_decode_r4(xi_opcode))
#define AVR_FAST_CORE_INST_DEFN_K8(k8, xi_opcode) uint_fast8_t k8 = (((xi_opcode & 0x0f00) >> 4) | _avr_fast_core_inst_decode_r4(xi_opcode))
#define AVR_FAST_CORE_INST_DEFN_O7(o7, xi_opcode) int_fast8_t o7 = ((int16_t)((xi_opcode & 0x03f8) << 6) >> 8)
#define AVR_FAST_CORE_INST_DEFN_O12(o12, xi_opcode) int_fast16_t o12 = ((int16_t)((xi_opcode & 0x0fff) << 4) >> 3)
#define AVR_FAST_CORE_INST_DEFN_P2(p2, xi_opcode) uint_fast8_t p2 = (24 + ((xi_opcode & 0x0030) >> 3))
#define AVR_FAST_CORE_INST_DEFN_Q6(q6, xi_opcode) uint_fast8_t q6 = ( ((xi_opcode & 0x2000) >> 8) | ((xi_opcode & 0x0c00) >> 7) | _avr_fast_core_inst_decode_b3(xi_opcode) )
#define AVR_FAST_CORE_INST_DEFN_R4(r4, xi_opcode) uint_fast8_t r4 = _avr_fast_core_inst_decode_r4(xi_opcode)
#define AVR_FAST_CORE_INST_DEFN_R5(r5, xi_opcode) uint_fast8_t r5 = ( ((xi_opcode & 0x0200) >> 5) | _avr_fast_core_inst_decode_r4(xi_opcode) )
#define AVR_FAST_CORE_INST_DEFN_R16(r16, xi_opcode) uint_fast8_t r16 = (16 + _avr_fast_core_inst_decode_r4(xi_opcode))

typedef void (pfnInst_t)(avr_t* avr, int_fast32_t *count, uint_fast32_t u_opcode);

#define AVR_FAST_CORE_INST_DECL(name) \
	void _avr_fast_core_inst_##name(avr_t *avr, int_fast32_t *count, uint16_t i_opcode)
#define AVR_FAST_CORE_INST_CALL(type, name) \
	_avr_fast_core_inst_##type##_##name(avr, count, i_opcode)
	
#define AVR_FAST_CORE_UINST_DECL(name) \
	void _avr_fast_core_uinst_##name(avr_t *avr, int_fast32_t *count, uint_fast32_t u_opcode)
#define AVR_FAST_CORE_UINST_CALL(name) \
	_avr_fast_core_uinst_##name(avr, count, u_opcode)

#define AVR_FAST_CORE_PFN_UINST_CALL(pfn_opcode) \
	pfn(avr, count, pfn_opcode)

typedef pfnInst_t * pfnInst_p;


#ifdef AVR_FAST_CORE_ITRACE
#define AVR_FAST_CORE_ITRACE_CALL(combining) \
	_avr_fast_core_itrace_call(avr, combining, i_opcode, inst_pc, u_opcode, __FUNCTION__)
	
static void _avr_fast_core_itrace_call(avr_t *avr, int combining, uint16_t i_opcode,
		avr_flashaddr_t inst_pc, uint32_t u_opcode, const char *function) {
	iSTATE("\t\t\t\t\t\t\t\t%s  (0x%04x [0x%08x]) %s\n",
		(combining ? "combining" : "         "),
			i_opcode, u_opcode, function);
}
#else
#define AVR_FAST_CORE_ITRACE_CALL(combining)
#endif

#define AVR_FAST_CORE_INST_XLAT(type, name) \
	avr_flashaddr_t __attribute__((__unused__)) inst_pc = avr->pc; \
	avr_flashaddr_t __attribute__((__unused__)) new_pc = 2 + inst_pc; \
	pfnInst_p __attribute__((__unused__)) pfn = _avr_fast_core_uinst_##type##_##name; \
	uint_fast32_t pfn_opcode = _avr_fast_core_inst_xlat_##type(avr, _avr_fast_core_uinst_##type##_##name##_k, i_opcode); \
	uint_fast32_t u_opcode = pfn_opcode;

#define AVR_FAST_CORE_INST_XLAT_DO_WRITE(type, name) \
	AVR_FAST_CORE_INST_XLAT(type, name); \
	AVR_FAST_CORE_PROFILER_PROFILE(uinst[AVR_FAST_CORE_UINST_R0(u_opcode)], AVR_FAST_CORE_UINST_CALL(type##_##name)); \
	_avr_fast_core_flash_uflash_write_0(avr, inst_pc, u_opcode);

#define AVR_FAST_CORE_INST_DEFN(type, name) \
	AVR_FAST_CORE_INST_DECL(type##_##name) { \
		AVR_FAST_CORE_INST_XLAT_DO_WRITE(type, name) \
		AVR_FAST_CORE_ITRACE_CALL(0); \
	}

#define AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(type, name, args...) \
	AVR_FAST_CORE_INST_DECL(type##_##name) { \
		AVR_FAST_CORE_INST_XLAT(type, name); \
		int combining = _AVR_FAST_CORE_COMBINING; \
		if(combining) { \
			AVR_FAST_CORE_INST_DEFN_##type(i_opcode, ## args); \
			AVR_FAST_CORE_FLASH_OPCODE_DEFN(next_opcode, new_pc);

#define AVR_FAST_CORE_END_COMBINING_INST_DEFN \
		} \
		AVR_FAST_CORE_PROFILER_PROFILE(uinst[AVR_FAST_CORE_UINST_R0(pfn_opcode)], \
			AVR_FAST_CORE_PFN_UINST_CALL(pfn_opcode)); \
		_avr_fast_core_flash_uflash_write_0(avr, inst_pc, u_opcode); \
		AVR_FAST_CORE_ITRACE_CALL(combining); \
	}

#define AVR_FAST_CORE_BEGIN_COMPLEX_INST_DEFN(type, name, args...) \
	AVR_FAST_CORE_INST_DECL(type##_##name) { \
		AVR_FAST_CORE_INST_XLAT(type, name); \
		if(0 != _AVR_FAST_CORE_COMPLEX) { \
			AVR_FAST_CORE_INST_DEFN_##type(i_opcode, ## args);
		
#define AVR_FAST_CORE_END_COMPLEX_INST_DEFN \
		} \
		AVR_FAST_CORE_PROFILER_PROFILE(uinst[AVR_FAST_CORE_UINST_R0(pfn_opcode)], \
			AVR_FAST_CORE_PFN_UINST_CALL(pfn_opcode)); \
		_avr_fast_core_flash_uflash_write_0(avr, inst_pc, u_opcode); \
		AVR_FAST_CORE_ITRACE_CALL(0); \
	}

static uint32_t _avr_fast_core_inst_xlat_x(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, 0, 0, 0));
}

#define AVR_FAST_CORE_INST_DEFN_a5m8(xi_opcode, a5, b3) \
	AVR_FAST_CORE_INST_DEFN_A5(a5, xi_opcode); \
	AVR_FAST_CORE_INST_DEFN_B3a(b3, xi_opcode)

static uint32_t _avr_fast_core_inst_xlat_a5m8(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_a5m8(i_opcode, a5, b3);
	uint8_t mask = (1 << b3);
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, a5, mask, 0));
}

#define AVR_FAST_CORE_INST_DEFN_b3(xi_opcode, b3) \
	AVR_FAST_CORE_INST_DEFN_B3b(b3, xi_opcode);

static uint32_t _avr_fast_core_inst_xlat_b3(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_b3(i_opcode, b3);
	uint8_t mask = (1 << b3);
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, b3, mask, 0));
}

#define AVR_FAST_CORE_INST_DEFN_b3o7(xi_opcode, b3, o7) \
	AVR_FAST_CORE_INST_DEFN_B3a(b3, xi_opcode); \
	AVR_FAST_CORE_INST_DEFN_O7(o7, xi_opcode)

static uint32_t _avr_fast_core_inst_xlat_b3o7(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_b3o7(i_opcode, b3, o7);
	uint8_t mask = (1 << b3);
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, b3, o7, mask));
}

static uint32_t _avr_fast_core_inst_xlat_d4r4(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_D4(d4, i_opcode) << 1;
	AVR_FAST_CORE_INST_DEFN_R4(r4, i_opcode) << 1;
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, d4, r4, 0));
}

#define AVR_FAST_CORE_INST_DEFN_d5(xi_opcode, d5) \
	AVR_FAST_CORE_INST_DEFN_D5(d5, xi_opcode);

static uint32_t _avr_fast_core_inst_xlat_d5(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_d5(i_opcode, d5);
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, d5, 0, 0));
}


#define	AVR_FAST_CORE_INST_DEFN_d5a6(xi_opcode, d5, a6) \
	AVR_FAST_CORE_INST_DEFN_D5(d5, xi_opcode); \
	AVR_FAST_CORE_INST_DEFN_A6(a6, xi_opcode)

static uint32_t _avr_fast_core_inst_xlat_d5a6(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_d5a6(i_opcode, d5, a6);
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, d5, a6, 0));
}

static uint32_t _avr_fast_core_inst_xlat_d5b3(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_D5(d5, i_opcode);
	AVR_FAST_CORE_INST_DEFN_B3a(b3, i_opcode);
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, d5, b3, 0));
}

static uint32_t _avr_fast_core_inst_xlat_d5m8(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_D5(d5, i_opcode);
	AVR_FAST_CORE_INST_DEFN_B3a(b3, i_opcode);
	uint8_t mask = (1 << b3);
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, d5, mask, 0));
}

#define	AVR_FAST_CORE_INST_DEFN_d5r5(xi_opcode, d5, r5) \
	AVR_FAST_CORE_INST_DEFN_D5(d5, xi_opcode); \
	AVR_FAST_CORE_INST_DEFN_R5(r5, xi_opcode)

static uint32_t _avr_fast_core_inst_xlat_d5r5(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_d5r5(i_opcode, d5, r5);
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, d5, r5, 0));
}

static uint32_t _avr_fast_core_inst_xlat_d5rXYZ(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_D5(d5, i_opcode);
	uint8_t  rXYZ = ((uint8_t []){R_ZL, 0x00, R_YL, R_XL})[(i_opcode & 0x000c)>>2];
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, d5, rXYZ, 0));
}

#define AVR_FAST_CORE_INST_DEFN_d5rYZq6(xi_opcode, d5, rYZ, q6) \
	AVR_FAST_CORE_INST_DEFN_D5(d5, xi_opcode); \
	uint8_t  rYZ = ((i_opcode & 0x0008) ? R_YL : R_ZL); \
	AVR_FAST_CORE_INST_DEFN_Q6(q6, xi_opcode)

static uint32_t _avr_fast_core_inst_xlat_d5rYZq6(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_d5rYZq6(i_opcode, d5, rYZ, q6);
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, d5, rYZ, q6));
}


#define AVR_FAST_CORE_INST_DEFN_d5x16(xi_opcode, d5, x16) \
	AVR_FAST_CORE_INST_DEFN_D5(d5, xi_opcode); \
	AVR_FAST_CORE_FLASH_OPCODE_DEFN(x16, new_pc); \
	new_pc += 2;


static uint32_t _avr_fast_core_inst_xlat_d5x16(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_D5(d5, i_opcode);
	AVR_FAST_CORE_FLASH_OPCODE_DEFN(x16, (2 + avr->pc));
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, d5, x16, 0));
}

static uint32_t _avr_fast_core_inst_xlat_d16r16(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_D16(d16, i_opcode);
	AVR_FAST_CORE_INST_DEFN_R16(r16, i_opcode);
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, d16, r16, 0));
}

#define	AVR_FAST_CORE_INST_DEFN_h4k8(xi_opcode, h4, k8) \
	AVR_FAST_CORE_INST_DEFN_H4(h4, xi_opcode); \
	AVR_FAST_CORE_INST_DEFN_K8(k8, xi_opcode)

static uint32_t _avr_fast_core_inst_xlat_h4k8(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_h4k8(i_opcode, h4, k8);
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, h4, k8, 0));
}

static uint32_t _avr_fast_core_inst_xlat_o12(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_O12(o12, i_opcode);
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, 0, o12, 0));
}

static uint32_t _avr_fast_core_inst_xlat_p2k6(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	AVR_FAST_CORE_INST_DEFN_P2(p2, i_opcode);
	AVR_FAST_CORE_INST_DEFN_K6(k6, i_opcode);
	return(AVR_FAST_CORE_MAKE_UFLASH_OPCODE(r0, p2, k6, 0));
}

static uint32_t _avr_fast_core_inst_xlat_x22(avr_t *avr, uint8_t r0, uint16_t i_opcode) {
	uint_fast8_t x6 = ((_avr_fast_core_inst_decode_d5(i_opcode) << 1) | (i_opcode & 0x0001));
	AVR_FAST_CORE_FLASH_OPCODE_DEFN(x16, (2 + avr->pc));
	uint_fast32_t x22 = ((x6 << 16) | x16) << 1;
	return((x22 << 8) | r0);
}

#if 1
#define AVR_FAST_CORE_UINST_STEP_PC_CYCLES(x, y) CYCLES(y); avr->pc += (x);
#define AVR_FAST_CORE_UINST_JMP_PC_CYCLES(x, y) CYCLES(y); avr->pc = (x);
#else
#define AVR_FAST_CORE_UINST_STEP_PC_CYCLES(x, y) avr->pc += (x); CYCLES(y);
#define AVR_FAST_CORE_UINST_JMP_PC_CYCLES(x, y) avr->pc = (x); CYCLES(y);
#endif

static pfnInst_p _avr_fast_core_uinst_op_table[256];
#ifdef AVR_FAST_CORE_TAIL_CALL
#define AVR_FAST_CORE_UINST_SPLT_NEXT() \
	if(likely(0 < *count)) { \
		AVR_FAST_CORE_UFLASH_OPCODE_FETCH(u_opcode, avr->pc); \
		AVR_FAST_CORE_UINST_DEFN_R0(u_opcode, u_opcode_op); \
		pfnInst_p pfn = _avr_fast_core_uinst_op_table[u_opcode_op]; \
		return(AVR_FAST_CORE_PFN_UINST_CALL(u_opcode)); \
	} else \
		return;
#else
#define AVR_FAST_CORE_UINST_SPLT_NEXT() \
	return
#endif

#define AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(next_pc, next_cycles) \
	AVR_FAST_CORE_UINST_STEP_PC_CYCLES(next_pc, next_cycles); \
	AVR_FAST_CORE_UINST_SPLT_NEXT();
	
#define AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(next_pc, next_cycles) \
	AVR_FAST_CORE_UINST_JMP_PC_CYCLES(next_pc, next_cycles); \
	AVR_FAST_CORE_UINST_SPLT_NEXT();

AVR_FAST_CORE_UINST_DECL(d5r5_adc)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2v(u_opcode, d, r);
	uint_fast8_t res = vd + vr + avr->sreg[S_C];

	if (r == d) {
		STATE("rol %s[%02x] = %02x\n", avr_regname(d), vd, res);
	} else {
		STATE("addc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
	}

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	_avr_fast_core_flags_add_zns(avr, res, vd, vr);

	SREG();
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_BEGIN_COMPLEX_INST_DEFN(d5r5, adc, d5, r5)
	if(d5 == r5)
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5_rol, d5, 0, 0);
AVR_FAST_CORE_END_COMPLEX_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5r5_add)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2v(u_opcode, d, r);
	uint_fast8_t res = vd + vr;

	if (r == d) {
		STATE("lsl %s[%02x] = %02x\n", avr_regname(d), vd, res & 0xff);
	} else {
		STATE("add %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
	}

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	_avr_fast_core_flags_add_zns(avr, res, vd, vr);

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(d5r5, add, d5, r5)
	AVR_FAST_CORE_INST_DEFN_d5r5(next_opcode, d5b, r5b);

	if( (0x0c00 /* ADD */ == (i_opcode & 0xfc00)) && (0x1c00 /* ADDC */ == (next_opcode & 0xfc00))
			&& ((d5 + 1) == d5b) && ((r5 + 1) == r5b) ) {
		if(d5 == r5) {
			u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5_lsl_rol, d5, 0, 0);
		} else {
			u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5r5_add_adc, d5, r5, 0);
		}
	} else if (d5 == r5) {
		int count = 1;
		while((7 > count) && (i_opcode == next_opcode)) {
			new_pc += 2;
			count++;
			AVR_FAST_CORE_FLASH_OPCODE_FETCH(next_opcode, new_pc);
		}
		
		if(1 < count) {
			u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5r5_lsl_lsl, d5, count, 0);
		} else {
			u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5_lsl, d5, 0, 0);
		}
	} else
		combining = 0;
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5r5_add_adc)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v16le_R2v16le(u_opcode, d, r);
	uint_fast16_t res = vd + vr;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint8_t vdl = vd & 0xff; uint8_t vrl = vr & 0xff);
		T(uint8_t res0 = vdl + vrl);
		STATE("add %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdl, avr_regname(r), vrl, res0);
		T(_avr_fast_core_flags_add_zns(avr, res0, vdl, vrl));
		SREG();
	#else
	//	STATE("/ add %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdl, avr_regname(r), vrl, res0);
		STATE("add.adc %s:%s[%04x], %s:%s[%04x] = %04x\n", avr_regname(d), avr_regname(d + 1), vd, 
			avr_regname(r), avr_regname(r + 1), vr, res);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	
	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint8_t vdh = vd >> 8; uint8_t vrh = vr >> 8);
		T(uint8_t res1 = vdh + vrh + avr->sreg[S_C]);
		STATE("addc %s[%02x], %s[%02x] = %02x\n", avr_regname(d + 1), vdh, avr_regname(r + 1), vrh, res1);
	#else
	//	STATE("\\ addc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdh, avr_regname(r), vrh, res1);
	#endif

	AVR_FAST_CORE_RMW_STORE_R16LE(d, res);

/*	NOTE: about flag behavior...
	certain flags, despite logic where concerned with the combination of
		add + adc and other combinations only pertain to the data relevant
		to the adc instruction, if handled as and entire quantity produces
		incorrect results...  this is taken into accunt in the 
		_avr_fast_core_flags_add16_zns16 function...
		
		prior implimentation was:
			_avr_fast_core_flags_add16(avr, res, vd, vr);
			_avr_fast_core_flags_zns16(avr, res & 0xff00);
		
		in particular it was found that zero flag was incorrectly set. */
		
	_avr_fast_core_flags_add16_zns16(avr, res, vd, vr);

	SREG();

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

AVR_FAST_CORE_UINST_DECL(p2k6_adiw)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v16le_R2(u_opcode, p, k);
	uint_fast16_t res = vp + k;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("adiw %s:%s[%04x], 0x%02x = 0x%04x\n", avr_regname(p), avr_regname(p+1), vp, k, res);
	#else
		STATE("adiw %s:%s[%04x], 0x%02x\n", avr_regname(p), avr_regname(p+1), vp, k);
	#endif

	AVR_FAST_CORE_RMW_STORE_R16LE(p, res);

	avr->sreg[S_V] = (((~vp) & res) >> 15) & 1;
	avr->sreg[S_C] = (((~res) & vp) >> 15) & 1;

	_avr_fast_core_flags_zns16(avr, res);

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2);
}
AVR_FAST_CORE_INST_DEFN(p2k6, adiw)

AVR_FAST_CORE_UINST_DECL(d5r5_and)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2v(u_opcode, d, r);
	uint_fast8_t res = vd & vr;

	if (r == d) {
		STATE("tst %s[%02x]\n", avr_regname(d), vd);
	} else {
		STATE("and %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
	}

	AVR_FAST_CORE_RMW_STORE_R(d, res);
	
	_avr_fast_core_flags_znv0s(avr, res);

	SREG();
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_BEGIN_COMPLEX_INST_DEFN(d5r5, and, d5, r5)
	if(d5 == r5)
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5_tst, d5, 0, 0);
AVR_FAST_CORE_END_COMPLEX_INST_DEFN

AVR_FAST_CORE_UINST_DECL(h4k8_andi)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2(u_opcode, h, k);
	uint_fast8_t res = vh & k;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("andi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
	#else
		STATE("andi %s[%02x], 0x%02x = 0x%02x\n", avr_regname(h), vh, k, res);
	#endif

	AVR_FAST_CORE_RMW_STORE_R(h, res);

	_avr_fast_core_flags_znv0s(avr, res);

	SREG();
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(h4k8, andi, h4, k8)
	AVR_FAST_CORE_INST_DEFN_H4(h4b, next_opcode);
	AVR_FAST_CORE_INST_DEFN_D5(d5, next_opcode);
	
	if( (0x7000 == (i_opcode & 0xf000)) && ( 0x7000 == (next_opcode & 0xf000))
			&& ((h4 + 1) == h4b) ) {
		AVR_FAST_CORE_INST_DEFN_K8(k8b, next_opcode);
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(h4k16_andi_andi, h4, k8, k8b);
	} else if( (0x7000 /* ANDI */ == (i_opcode & 0xf000)) && ( 0x2800 == (next_opcode /* OR */ & 0xfc00))
			&& (h4 == d5) ) {
		AVR_FAST_CORE_INST_DEFN_R5(r5, next_opcode);
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(h4r5k8_andi_or, h4, r5, k8);
	} else if( (0x7000 /* ANDI */ == (i_opcode & 0xf000)) && ( 0x6000 == (next_opcode /* ORI */ & 0xf000))
			&& (h4 == h4b) ) {
		AVR_FAST_CORE_INST_DEFN_K8(k8b, next_opcode);
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(h4k8k8_andi_ori, h4, k8, k8b);
	} else

		combining = 0;
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(h4k16_andi_andi)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v16le_16R2(u_opcode, h, k);
	uint_fast16_t res = vh & k;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("andi %s[%02x], 0x%02x\n", avr_regname(h), vh & 0xff, k & 0xff);
	#else
		STATE("andi %s:%s[%04x], 0x%04x\n", avr_regname(h), avr_regname(h + 1), vh, k);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("andi %s[%02x], 0x%02x\n", avr_regname(h + 1), vh >> 8, k >> 8);
	#endif

	AVR_FAST_CORE_RMW_STORE_R16LE(h, res);
	
	_avr_fast_core_flags_znv0s16(avr, res);

	SREG();
	
	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

AVR_FAST_CORE_UINST_DECL(h4r5k8_andi_or)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2v_R3(u_opcode, h, r, andi_k);
	uint_fast8_t andi_res = vh & andi_k;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("andi %s[%02x], 0x%02x\n", avr_regname(h), vh, andi_k);
	#else
		STATE("/ andi %s[%02x], 0x%02x = 0x%02x\n", avr_regname(h), vh, andi_k, andi_res);
	#endif

	T(_avr_fast_core_flags_znv0s(avr, andi_res));
	SREG();

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	uint_fast8_t res = andi_res | vr;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("or %s[%02x], %s[%02x] = %02x\n", avr_regname(h), andi_res, avr_regname(r), vr, res);
	#else
		STATE("\\ or %s[%02x], %s[%02x] = %02x\n", avr_regname(h), andi_res, avr_regname(r), vr, res);
	#endif

	AVR_FAST_CORE_RMW_STORE_R(h, res);

	_avr_fast_core_flags_znv0s(avr, res);

	SREG();

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

AVR_FAST_CORE_UINST_DECL(h4k8k8_andi_ori)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2_R3(u_opcode, h, andi_k, ori_k);
	uint_fast8_t andi_res = vh & andi_k;
	
	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("andi %s[%02x], 0x%02x\n", avr_regname(h), vh, andi_k);
	#else
		STATE("/ andi %s[%02x], 0x%02x = 0x%02x\n", avr_regname(h), vh, andi_k, andi_res);
	#endif

	T(_avr_fast_core_flags_znv0s(avr, andi_res));
	SREG();

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));
	uint_fast8_t res = andi_res | ori_k;
	
	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("ori %s[%02x], 0x%02x\n", avr_regname(h), andi_res, ori_k);
	#else
		STATE("\\ ori %s[%02x], 0x%02x = 0x%02x\n", avr_regname(h), andi_res, ori_k, res);
	#endif

	AVR_FAST_CORE_RMW_STORE_R(h, res);

	_avr_fast_core_flags_znv0s(avr, res);

	SREG();

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

AVR_FAST_CORE_UINST_DECL(d5_asr)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v(u_opcode, d);
	
	uint_fast8_t neg = vd & 0x80;
	uint_fast8_t res = (vd >> 1) | neg;
	
	STATE("asr %s[%02x]\n", avr_regname(d), vd);

	AVR_FAST_CORE_RMW_STORE_R(d, res);
	
	neg >>= 7;
	
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vd & 1;
	avr->sreg[S_N] = neg;
	avr->sreg[S_V] = neg ^ avr->sreg[S_C];
	avr->sreg[S_S] = neg ^ avr->sreg[S_V];

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5, asr)

AVR_FAST_CORE_UINST_DECL(b3_bclr)
{
	AVR_FAST_CORE_UINST_DEFN_R1(u_opcode, b);
	avr->sreg[b]=0;

	if(S_I == b)
		*count = 0;
	
	STATE("cl%c\n", _sreg_bit_name[b]);
	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(b3, bclr)

AVR_FAST_CORE_UINST_DECL(d5m8_bld)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2(u_opcode, d, mask);
	
	uint_fast8_t res;
	#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
		res = (vd & ~(mask)) | (avr->sreg[S_T] * (mask));
	#else
		res = (vd & ~(mask)) | (avr->sreg[S_T] ? (mask) : 0);
	#endif

	STATE("bld %s[%02x], 0x%02x = %02x\n", avr_regname(d), vd, mask, res);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5m8, bld)

AVR_FAST_CORE_UINST_DECL(o7_brcc)
{
	AVR_FAST_CORE_UINST_DEFN_iR2(u_opcode, o);

	int branch = (0 == avr->sreg[S_C]);
	avr_flashaddr_t new_pc = 2 + avr->pc;
	
	avr_flashaddr_t branch_pc;
	#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
		branch_pc = new_pc + (o * branch);
	#else
		branch_pc = new_pc + (branch ? o : 0);
	#endif

	STATE("brcc .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}

AVR_FAST_CORE_UINST_DECL(o7_brcs)
{
	AVR_FAST_CORE_UINST_DEFN_iR2(u_opcode, o);

	int branch = (0 != avr->sreg[S_C]);
	avr_flashaddr_t new_pc = 2 + avr->pc;
	
	avr_flashaddr_t branch_pc;
	#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
		branch_pc = new_pc + (o * branch);
	#else
		branch_pc = new_pc + (branch ? o : 0);
	#endif

	STATE("brcs .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}

AVR_FAST_CORE_UINST_DECL(o7_breq)
{
	AVR_FAST_CORE_UINST_DEFN_iR2(u_opcode, o);

	int branch = (0 != avr->sreg[S_Z]);
	avr_flashaddr_t new_pc = 2 + avr->pc;

	avr_flashaddr_t branch_pc;
	#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
		branch_pc = new_pc + (o * branch);
	#else
		branch_pc = new_pc + (branch ? o : 0);
	#endif

	STATE("breq .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}

AVR_FAST_CORE_UINST_DECL(o7_brne)
{
	AVR_FAST_CORE_UINST_DEFN_iR2(u_opcode, o);

	int branch = (0 == avr->sreg[S_Z]);
	avr_flashaddr_t new_pc = 2 + avr->pc;

	avr_flashaddr_t branch_pc;
	#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
		branch_pc = new_pc + (o * branch);
	#else
		branch_pc = new_pc + (branch ? o : 0);
	#endif

	STATE("brne .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}

AVR_FAST_CORE_UINST_DECL(o7_brpl)
{
	AVR_FAST_CORE_UINST_DEFN_iR2(u_opcode, o);

	int branch = (0 == avr->sreg[S_N]);
	avr_flashaddr_t new_pc = 2 + avr->pc;

	avr_flashaddr_t branch_pc;
	#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
		branch_pc = new_pc + (o * branch);
	#else
		branch_pc = new_pc + (branch ? o : 0);
	#endif

	STATE("brpl .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}

AVR_FAST_CORE_UINST_DECL(b3o7_brxc)
{
	AVR_FAST_CORE_UINST_DEFN_R1_iR2(u_opcode, b, o);

	int branch = (0 == avr->sreg[b]);
	avr_flashaddr_t new_pc = 2 + avr->pc;

	avr_flashaddr_t branch_pc;
	#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
		branch_pc = new_pc + (o * branch);
	#else
		branch_pc = new_pc + (branch ? o : 0);
	#endif

	const char *names[8] = {
		"brcc", "brne", "brpl", "brvc", NULL, "brhc", "brtc", "brid"
	};

	if (names[b]) {
		STATE("%s .%d [%04x]\t; Will%s branch\n", names[b], o >> 1, new_pc + o, branch ? "":" not");
	} else {
		STATE("brbc%c .%d [%04x]\t; Will%s branch\n", _sreg_bit_name[b], o >> 1, new_pc + o, branch ? "":" not");
	}

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}
AVR_FAST_CORE_BEGIN_COMPLEX_INST_DEFN(b3o7, brxc, b3, o7)
	if(S_C == b3)
		u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(o7_brcc, 0, o7, 0);
	else if(S_N == b3)
		u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(o7_brpl, 0, o7, 0);
	else if(S_Z == b3)
		u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(o7_brne, 0, o7, 0);
AVR_FAST_CORE_END_COMPLEX_INST_DEFN

AVR_FAST_CORE_UINST_DECL(b3o7_brxs)
{
	AVR_FAST_CORE_UINST_DEFN_R1_iR2(u_opcode, b, o);

	int branch = (0 != avr->sreg[b]);
	avr_flashaddr_t new_pc = 2 + avr->pc;

	avr_flashaddr_t branch_pc;
	#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
		branch_pc = new_pc + (o * branch);
	#else
		branch_pc = new_pc + (branch ? o : 0);
	#endif

	const char *names[8] = {
		"brcs", "breq", "brmi", "brvs", NULL, "brhs", "brts", "brie"
	};
	
	if (names[b]) {
		STATE("%s .%d [%04x]\t; Will%s branch\n", names[b], o >> 1, new_pc + o, branch ? "":" not");
	} else {
		STATE("brbs%c .%d [%04x]\t; Will%s branch\n", _sreg_bit_name[b], o >> 1, new_pc + o, branch ? "":" not");
	}

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}
AVR_FAST_CORE_BEGIN_COMPLEX_INST_DEFN(b3o7, brxs, b3, o7)
	if(S_C == b3)
		u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(o7_brcs, 0, o7, 0);
	else if(S_Z == b3)
		u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(o7_breq, 0, o7, 0);
AVR_FAST_CORE_END_COMPLEX_INST_DEFN

AVR_FAST_CORE_UINST_DECL(b3_bset)
{
	AVR_FAST_CORE_UINST_DEFN_R1(u_opcode, b);
	avr->sreg[b]=1;

	/* 
		avr instruction manual states there shall be one cycle latency
		after issuing an sei instruction. */

	if(S_I == b) {
		*count = 1;
	}

	STATE("se%c\n", _sreg_bit_name[b]);
	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(b3, bset, b3)
	if(0x9588 == next_opcode) {
		u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(sei_sleep, 0, 0, 0);
	} else if(S_I == b3) {
		u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(sei, 0, 0, 0);
		combining = 0;
	} else
		combining = 0;
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5b3_bst)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2(u_opcode, d, b);
	uint_fast8_t res = (vd >> b) & 1;

	STATE("bst %s[%02x], 0x%02x = %02x\n", avr_regname(d), vd, (1 << b), res);
	
	avr->sreg[S_T] = res;

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5b3, bst)

AVR_FAST_CORE_UINST_DECL(x22_call)
{
	AVR_FAST_CORE_UINST_DEFN_24R1(u_opcode, x22);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("call 0x%06x\n", x22 >> 1);
	#else
		STATE("call 0x%06x\n", x22);
	#endif

	_avr_fast_core_push16be(avr, count, 2 + (avr->pc >> 1));

	TRACE_JUMP();
	STACK_FRAME_PUSH();

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(x22, 4);
}
AVR_FAST_CORE_INST_DEFN(x22, call)

AVR_FAST_CORE_UINST_DECL(a5m8_cbi)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2(u_opcode, io, mask);
	AVR_FAST_CORE_UINST_DEFN_vIO(io);
	uint_fast8_t res = vio & ~(mask);

	STATE("cbi %s[%04x], 0x%02x = %02x\n", avr_regname(io), vio, mask, res);

	_avr_fast_core_reg_io_write(avr, count, io, res);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2);
}
AVR_FAST_CORE_BEGIN_COMPLEX_INST_DEFN(a5m8, cbi, a5, mask)
	if((R_SREG == a5) && (S_I == mask)) {
		u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(cli, 0, 0, 0);
	}
AVR_FAST_CORE_END_COMPLEX_INST_DEFN

AVR_FAST_CORE_UINST_DECL(cli)
{
	T(uint_fast8_t io = R_SREG);
	uint_fast8_t mask = S_I;

	T(AVR_FAST_CORE_UINST_DEFN_vIO(io));
	T(uint_fast8_t res = vio & ~(mask));
	
	STATE("cbi %s[%04x], 0x%02x = %02x\n", avr_regname(io), vio, mask, res);

	avr->sreg[mask]=0;
	
	*count = 0;
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2);
}

AVR_FAST_CORE_UINST_DECL(d5_clr)
{
	T(AVR_FAST_CORE_UINST_DEFN_R1v(u_opcode, d));
	NO_T(AVR_FAST_CORE_UINST_DEFN_R1(u_opcode, d));

	STATE("clr %s[%02x]\n", avr_regname(d), vd);

	_avr_fast_core_store_r(avr, d, 0);

	avr->sreg[S_N] = 0;
	avr->sreg[S_S] = 0;
	avr->sreg[S_V] = 0;
	avr->sreg[S_Z] = 1;
	
	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}

AVR_FAST_CORE_UINST_DECL(d5_com)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v(u_opcode, d);
	uint_fast8_t res = 0xff - vd;

	STATE("com %s[%02x] = %02x\n", avr_regname(d), vd, res);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	avr->sreg[S_C] = 1;

	_avr_fast_core_flags_znv0s(avr, res);

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5, com)

AVR_FAST_CORE_UINST_DECL(d5r5_cp)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2v(u_opcode, d, r);
	uint_fast8_t res = vd - vr;

	STATE("cp %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_fast_core_flags_sub_zns(avr, res, vd, vr);

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(d5r5, cp, d5, r5)
	AVR_FAST_CORE_INST_DEFN_d5r5(next_opcode, d5b, r5b);

	if( (0x1400 /* CP.lh */ == (i_opcode & 0xfc00)) && (0x0400 /* CPC.lh */ == (next_opcode & 0xfc00))
			&& ((d5 + 1) == d5b) && ((r5 + 1) == r5b) ) {
		AVR_FAST_CORE_FLASH_OPCODE_DEFN(next_next_opcode, 2 + new_pc);
		if(0xf401 == (next_next_opcode & 0xfc07)) {
			AVR_FAST_CORE_INST_DEFN_O7(o7, next_next_opcode);
			u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(d5r5o7_cp_cpc_brne, d5, r5, o7);
		} else {
			u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(d5r5_cp_cpc, d5, r5, 0);
		}
	} else
		combining = 0;
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5r5_cp_cpc)
{
	AVR_FAST_CORE_UINST_DEFN_R1v16le_R2v16le(u_opcode, d, r);
	uint_fast16_t res = vd - vr;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint8_t vdl = vd & 0xff; uint8_t vrl = vr & 0xff);
		T(uint8_t res0 = vdl  - vrl);
		STATE("cp %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdl, avr_regname(r), vrl, res0);
		T(_avr_fast_core_flags_sub_zns(avr, res0, vdl, vrl));
		T(SREG());
	#else
		STATE("cp.cpc %s:%s[%04x], %s:%s[%04x] = %04x\n", avr_regname(d), avr_regname(d + 1), vd, 
			avr_regname(r), avr_regname(r + 1), vr, res);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint8_t vdh = (vd >> 8) & 0xff; uint8_t vrh = (vr >> 8) & 0xff);
		T(uint8_t res1 = vdh  - vrh);
		STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(d + 1), vdh, avr_regname(r + 1), vrh, res1);
	#endif

	_avr_fast_core_flags_sub16_zns16(avr, res, vd, vr);

	SREG();

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

AVR_FAST_CORE_UINST_DECL(d5r5o7_cp_cpc_brne)
{
	AVR_FAST_CORE_UINST_DEFN_R1v16le_R2v16le(u_opcode, d, r);
	uint_fast16_t res = vd - vr;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint8_t vdl = vd & 0xff; uint8_t vrl = vr & 0xff);
		T(uint8_t res0 = vdl  - vrl);
		STATE("cp %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdl, avr_regname(r), vrl, res0);
		T(_avr_fast_core_flags_sub_zns(avr, res0, vdl, vrl));
		T(SREG());
	#else
		STATE("cp.cpc %s:%s[%04x], %s:%s[%04x] = %04x\n", avr_regname(d), avr_regname(d + 1), vd, 
			avr_regname(r), avr_regname(r + 1), vr, res);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint8_t vdh = (vd >> 8) & 0xff; uint8_t vrh = (vr >> 8) & 0xff);
		T(uint8_t res1 = vdh  - vrh);
		STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(d + 1), vdh, avr_regname(r + 1), vrh, res1);
	#endif

	_avr_fast_core_flags_sub16_zns16(avr, res, vd, vr);

	SREG();

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));
	AVR_FAST_CORE_UINST_DEFN_iR3(u_opcode, o);

	int branch = (0 != res);
		
	T(avr_flashaddr_t new_pc = 2 + avr->pc);
	NO_T(avr_flashaddr_t new_pc = 2 + 2 + 2 + avr->pc);

	avr_flashaddr_t branch_pc;
	#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
		branch_pc = new_pc + (o * branch);
	#else
		branch_pc = new_pc + (branch ? o : 0);
	#endif

	STATE("brne .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	T(AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch));
	NO_T(AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + 1 + 1 + branch));
}

AVR_FAST_CORE_UINST_DECL(d5r5_cpc)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2v(u_opcode, d, r);
	uint_fast8_t res = vd - vr - avr->sreg[S_C];

	STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_fast_core_flags_sub_Rzns(avr, res, vd, vr);

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5r5, cpc)

AVR_FAST_CORE_UINST_DECL(h4k8_cpi)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2(u_opcode, h, k);
	uint_fast8_t res = vh - k;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("cpi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);
	#else
		STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
	#endif

	_avr_fast_core_flags_sub_zns(avr, res, vh, k);

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(h4k8, cpi, h4, k8)
	AVR_FAST_CORE_INST_DEFN_D5(d5, next_opcode);

	if(_AVR_FAST_CORE_CPI_BRXX &&
		(0x3000 /* CPI */ == (i_opcode & 0xf000)) && (0xf000 /* BRX(S/C) */ == (next_opcode & 0xf800)) ) {
			AVR_FAST_CORE_INST_DEFN_O7(o7, next_opcode);
			switch(next_opcode & 0x0407) {
				case	S_C:
					u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(h4k8o7_cpi_brcs, h4, k8, o7);
					break;
				case	S_Z:
					u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(h4k8o7_cpi_breq, h4, k8, o7);
					break;
				case	(0x400 | S_C):
					u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(h4k8o7_cpi_brcc, h4, k8, o7);
					break;
				case	(0x400 | S_Z):
					u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(h4k8o7_cpi_brne, h4, k8, o7);
					break;
				default:
					combining = 0;
					break;
			}
	} else if( (0x3000 /* CPI.l */ == (i_opcode & 0xf000)) && (0x0400 /* CPC.h */ == (next_opcode & 0xfc00))
			&& ((h4 + 1) == d5) ) {
		AVR_FAST_CORE_INST_DEFN_R5(r5, next_opcode);
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(h4r5k8_cpi_cpc, h4, r5, k8);
	} else
		combining = 0;
AVR_FAST_CORE_END_COMBINING_INST_DEFN

#define AVR_FAST_CORE_DO_CPI_BRXX_BRANCH(branch_op_string) \
	_avr_uinst_do_cpi_brxx_branch(avr, count, u_opcode, branch, branch_op_string)
	
static void _avr_uinst_do_cpi_brxx_branch(avr_t *avr, int_fast32_t *count, uint_fast32_t u_opcode,
	int branch, const char *branch_op_string);


AVR_FAST_CORE_UINST_DECL(h4k8o7_cpi_brcc)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2(u_opcode, h, k);
	uint_fast8_t res = vh - k;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("cpi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);
	#else
		STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
	#endif

	_avr_fast_core_flags_sub_zns(avr, res, vh, k);

	int branch = (0 == avr->sreg[S_C]);

	SREG();

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	#ifdef AVR_FAST_CORE_CPI_BRXX_COMMON_BRANCH_CODE
		AVR_FAST_CORE_DO_CPI_BRXX_BRANCH("brcc");
	#else
		AVR_FAST_CORE_UINST_DEFN_iR3(u_opcode, o);

		T(avr_flashaddr_t new_pc = 2 + avr->pc);
		NO_T(avr_flashaddr_t new_pc = 2 + 2 + avr->pc);

		avr_flashaddr_t branch_pc;
		#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
			branch_pc = new_pc + (o * branch);
		#else
			branch_pc = new_pc + (branch ? o : 0);
		#endif

		STATE("brcc .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

		T(AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch));
		NO_T(AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + 1 + branch));
	#endif
}

AVR_FAST_CORE_UINST_DECL(h4k8o7_cpi_brcs)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2(u_opcode, h, k);
	uint_fast8_t res = vh - k;
	
	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("cpi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);
	#else
		STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
	#endif

	_avr_fast_core_flags_sub_zns(avr, res, vh, k);

	int branch = (vh < k);

	SREG();

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	#ifdef AVR_FAST_CORE_CPI_BRXX_COMMON_BRANCH_CODE
		AVR_FAST_CORE_DO_CPI_BRXX_BRANCH("brcs");
	#else
		AVR_FAST_CORE_UINST_DEFN_iR3(u_opcode, o);

		T(avr_flashaddr_t new_pc = 2 + avr->pc);
		NO_T(avr_flashaddr_t new_pc = 2 + 2 + avr->pc);

		avr_flashaddr_t branch_pc;
		#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
			branch_pc = new_pc + (o * branch);
		#else
			branch_pc = new_pc + (branch ? o : 0);
		#endif

		STATE("brcs .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

		T(AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch));
		NO_T(AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + 1 + branch));
	#endif
}

static void _avr_uinst_do_cpi_brxx_branch(avr_t *avr, int_fast32_t *count,
		uint_fast32_t u_opcode, int branch, 
			const char *branch_op_string)
{

	AVR_FAST_CORE_UINST_DEFN_iR3(u_opcode, o);

	T(avr_flashaddr_t new_pc = 2 + avr->pc);
	NO_T(avr_flashaddr_t new_pc = 2 + 2 + avr->pc);

	avr_flashaddr_t branch_pc;
	#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
		branch_pc = new_pc + (o * branch);
	#else
		branch_pc = new_pc + (branch ? o : 0);
	#endif

	STATE("%s .%d [%04x]\t; Will%s branch\n", branch_op_string, o >> 1,
		new_pc + o, branch ? "":" not");

	T(AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch));
	NO_T(AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + 1 + branch));
}

AVR_FAST_CORE_UINST_DECL(h4k8o7_cpi_breq)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2(u_opcode, h, k);
	uint_fast8_t res = vh - k;
	
	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("cpi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);
	#else
		STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
	#endif

	_avr_fast_core_flags_sub_zns(avr, res, vh, k);

	int branch = (vh == k);

	SREG();

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	#ifdef AVR_FAST_CORE_CPI_BRXX_COMMON_BRANCH_CODE
		AVR_FAST_CORE_DO_CPI_BRXX_BRANCH("breq");
	#else
		AVR_FAST_CORE_UINST_DEFN_iR3(u_opcode, o);

		T(avr_flashaddr_t new_pc = 2 + avr->pc);
		NO_T(avr_flashaddr_t new_pc = 2 + 2 + avr->pc);

		avr_flashaddr_t branch_pc;
		#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
			branch_pc = new_pc + (o * branch);
		#else
			branch_pc = new_pc + (branch ? o : 0);
		#endif

		STATE("breq .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

		T(AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch));
		NO_T(AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + 1 + branch));
	#endif
}

AVR_FAST_CORE_UINST_DECL(h4k8o7_cpi_brne)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2(u_opcode, h, k);
	uint_fast8_t res = vh - k;
	int branch = (vh != k);
	
	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("cpi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);
	#else
		STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
	#endif

	_avr_fast_core_flags_sub_zns(avr, res, vh, k);

	SREG();

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	#ifdef AVR_FAST_CORE_CPI_BRXX_COMMON_BRANCH_CODE
		AVR_FAST_CORE_DO_CPI_BRXX_BRANCH("brne");
	#else
		AVR_FAST_CORE_UINST_DEFN_iR3(u_opcode, o);

		T(avr_flashaddr_t new_pc = 2 + avr->pc);
		NO_T(avr_flashaddr_t new_pc = 2 + 2 + avr->pc);

		avr_flashaddr_t branch_pc;
		#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
			branch_pc = new_pc + (o * branch);
		#else
			branch_pc = new_pc + (branch ? o : 0);
		#endif

		STATE("brne .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

		T(AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch));
		NO_T(AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + 1 + branch));
	#endif
}

AVR_FAST_CORE_UINST_DECL(h4r5k8_cpi_cpc)
{
	AVR_FAST_CORE_UINST_DEFN_R1v16le_R2v_R3(u_opcode, h, r, k);
	uint_fast16_t vrk = (vr << 8) | k;
	uint_fast16_t res = vh - vrk;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint8_t vhl = vh & 0xff);
		T(uint8_t res0 = vhl  - k);
		STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vhl, k);
		T(_avr_fast_core_flags_sub_zns(avr, res0, vhl, k));
		T(SREG());
	#else
		STATE("cpi.cpc %s:%s[%04x], 0x%04x = %04x\n", avr_regname(h), avr_regname(h + 1), vh, 
			vrk, res);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint8_t vhh = (vh >> 8) & 0xff);
		T(uint8_t res1 = vhh  - vr - avr->sreg[S_C]);
		STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(h + 1), vhh, avr_regname(r), vr, res1);
	#endif

	_avr_fast_core_flags_sub16_zns16(avr, res, vh, vrk);

	SREG();

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

AVR_FAST_CORE_UINST_DECL(d5r5_16_cpse)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2v(u_opcode, d, r);
	uint_fast8_t skip = vd == vr;

	STATE("cpse %s[%02x], %s[%02x]\t; Will%s skip\n", avr_regname(d), vd, avr_regname(r), vr, skip ? "":" not");

	#ifdef AVR_FAST_CORE_SKIP_SHIFT
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 << skip, 1 + skip);
	#else
		if (skip) {
			AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4, 2);
		}
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
	#endif
}
AVR_FAST_CORE_INST_DEFN(d5r5, 16_cpse)

AVR_FAST_CORE_UINST_DECL(d5r5_32_cpse)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2v(u_opcode, d, r);
	uint_fast8_t skip = (vd == vr);
	
	STATE("cpse %s[%02x], %s[%02x]\t; Will%s skip\n", avr_regname(d), vd, avr_regname(r), vr, skip ? "":" not");

	#ifdef AVR_FAST_CORE_32_SKIP_SHIFT
		int skip_count = skip ? 3 : 1;
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(skip_count << 1, skip_count);
	#else
		if (skip) {
			AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(6, 3);
		}
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
	#endif
}
AVR_FAST_CORE_INST_DEFN(d5r5, 32_cpse)

AVR_FAST_CORE_UINST_DECL(d5_dec)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v(u_opcode, d);
	uint_fast8_t res = vd - 1;

	STATE("dec %s[%02x] = %02x\n", avr_regname(d), vd, res);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	avr->sreg[S_V] = res == 0x7f;

	_avr_fast_core_flags_zns(avr, res);

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5, dec)

AVR_FAST_CORE_UINST_DECL(x22_eind_call)
{
	AVR_FAST_CORE_UINST_DEFN_24R1(u_opcode, x22);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("call 0x%06x\n", x22 >> 1);
	#else
		STATE("call 0x%06x\n", x22);
	#endif

	_avr_fast_core_push24be(avr, count, 2 + (avr->pc >> 1));

	TRACE_JUMP();
	STACK_FRAME_PUSH();

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(x22, 5);
}
AVR_FAST_CORE_INST_DEFN(x22, eind_call)

AVR_FAST_CORE_UINST_DECL(x_eind_eicall)
{
	uint_fast32_t z = (_avr_fast_core_fetch_r16le(avr, R_ZL) | avr->data[avr->eind] << 16) << 1;

	STATE("eicall Z[%04x]\n", z);

	_avr_fast_core_push24be(avr, count, 1 + (avr->pc >> 1));

	TRACE_JUMP();

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(z, 4);  // 4 cycles except 3 cycles on XMEGAs
}
AVR_FAST_CORE_INST_DEFN(x, eind_eicall)

AVR_FAST_CORE_UINST_DECL(x_eind_eijmp)
{
	uint_fast32_t z = (_avr_fast_core_fetch_r16le(avr, R_ZL) | avr->data[avr->eind] << 16) << 1;

	STATE("eijmp Z[%04x]\n", z);

	TRACE_JUMP();
	
	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(z, 2);
}
AVR_FAST_CORE_INST_DEFN(x, eind_eijmp)

AVR_FAST_CORE_UINST_DECL(o12_eind_rcall)
{
	AVR_FAST_CORE_UINST_DEFN_16R2(u_opcode, o);
	avr_flashaddr_t new_pc = 2 + avr->pc;
	avr_flashaddr_t branch_pc = new_pc + (int16_t)o;

	STATE("rcall .%d [%04x]\n", (int16_t)o >> 1, branch_pc);

	_avr_fast_core_push24be(avr, count, new_pc >> 1);

	// 'rcall .1' is used as a cheap "push 16 bits of room on the stack"
	if (o != 0) {
		TRACE_JUMP();
		STACK_FRAME_PUSH();
	}

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 4);
}
AVR_FAST_CORE_INST_DEFN(o12, eind_rcall)

AVR_FAST_CORE_UINST_DECL(x_eind_ret)
{
	STATE("ret\n");

	TRACE_JUMP();
	STACK_FRAME_POP();
	
	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(_avr_fast_core_pop24be(avr, count) << 1, 5);
}
AVR_FAST_CORE_INST_DEFN(x, eind_ret)

AVR_FAST_CORE_UINST_DECL(x_eind_reti)
{
	*count = 0;
	avr->sreg[S_I] = 1;

	STATE("reti\n");

	TRACE_JUMP();
	STACK_FRAME_POP();

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(_avr_fast_core_pop24be(avr, count) << 1, 5);
}
AVR_FAST_CORE_INST_DEFN(x, eind_reti)

AVR_FAST_CORE_UINST_DECL(d5r5_eor)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2v(u_opcode, d, r);
	uint_fast8_t res = vd ^ vr;

	if (r==d) {
		STATE("clr %s[%02x]\n", avr_regname(d), vd);
	} else {
		STATE("eor %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
	}

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	_avr_fast_core_flags_znv0s(avr, res);

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_BEGIN_COMPLEX_INST_DEFN(d5r5, eor, d5, r5)
	if(d5 == r5) {
		u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(d5_clr, d5, 0, 0);
	}
AVR_FAST_CORE_END_COMPLEX_INST_DEFN

AVR_FAST_CORE_UINST_DECL(x_icall)
{
	uint_fast16_t z = _avr_fast_core_fetch_r16le(avr, R_ZL) << 1;

	STATE("icall Z[%04x]\n", z);

	_avr_fast_core_push16be(avr, count, 1 + (avr->pc >> 1));

	TRACE_JUMP();

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(z, 3);
}
AVR_FAST_CORE_INST_DEFN(x, icall)

AVR_FAST_CORE_UINST_DECL(x_ijmp)
{
	uint_fast16_t z = _avr_fast_core_fetch_r16le(avr, R_ZL) << 1;

	STATE("ijmp Z[%04x]\n", z);

	TRACE_JUMP();

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(z, 2);
}
AVR_FAST_CORE_INST_DEFN(x, ijmp)

AVR_FAST_CORE_UINST_DECL(d5a6_in)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2(u_opcode, d, a);
	AVR_FAST_CORE_UINST_DEFN_vIO(a);

	#ifdef AVR_CORE_FAST_CORE_BUGS
		/* CORE TRACE BUG ??? */
		STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), avr->data[a]);
	#else
		STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);
	#endif

	_avr_fast_core_store_r(avr, d, va);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(d5a6, in, d5, a6)
	AVR_FAST_CORE_INST_DEFN_D5(d5b, next_opcode);
	AVR_FAST_CORE_INST_DEFN_H4(h4, next_opcode);

	if( (0xb000 /* IN */ == (i_opcode & 0xf800)) && (0xfe00 /* SBRS */ == (next_opcode & 0xfe00))
			&& (d5 == d5b) ) {
		AVR_FAST_CORE_INST_DEFN_B3a(b3, next_opcode);
		u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(d5a6m8_in_sbrs, d5, a6, (1 << b3));
	} else if( (0xb000 /* IN */ == (i_opcode & 0xf800)) && (0x920f /* PUSH */ == (0xfe0f & next_opcode))
			&& (d5 == d5b) ) {
		u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(d5a6_in_push, d5, a6, 0);
	} else if( (0xb000 /* IN */ == (i_opcode & 0xf800)) && 
		( (0x7000 /* ANDI */ == (next_opcode & 0xf000)) || (0x6000 /* ORI */ == (next_opcode & 0xf000)))
			&& (d5 == h4)) {
		AVR_FAST_CORE_FLASH_OPCODE_DEFN(next_next_opcode, 2 + new_pc);
		AVR_FAST_CORE_INST_DEFN_D5(d5c, next_next_opcode);
		AVR_FAST_CORE_INST_DEFN_A6(a6b, next_next_opcode);

		if( (0xb800 /* OUT */ == (next_next_opcode & 0xf800)) && (d5 == d5c) && (a6 == a6b)) {
			AVR_FAST_CORE_INST_DEFN_K8(k8, next_opcode);
			if( (0x7000 /* ANDI */ == (next_opcode & 0xf000)) ) {
				u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(d5a6k8_in_andi_out, d5, a6, k8);
			} else if( (0x6000 /* ORI */ == (next_opcode & 0xf000)) ) {
				u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(d5a6k8_in_ori_out, d5, a6, k8);
			}
		} else
			combining = 0;
	} else
		combining = 0;
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5a6k8_in_andi_out)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2_R3(u_opcode, d, a, k);
	AVR_FAST_CORE_UINST_DEFN_vIO(a);
	
	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		/* CORE TRACE BUG ??? */
		STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), avr->data[a]);
	#else
		STATE("/ in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	/* andi d5 == h4, k8 */

	uint_fast8_t res = va & k;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("andi %s[%02x], 0x%02x\n", avr_regname(d), va, k);
	#else
		STATE("| andi %s[%02x], 0x%02x = 0x%02x\n", avr_regname(d), va, k, res);
	#endif

	_avr_fast_core_store_r(avr, d, res);
	_avr_fast_core_flags_znv0s(avr, res);

	SREG();

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	/* out d5, a6 */

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), res);
	#else
		STATE("\\ out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), res);
	#endif

	_avr_fast_core_reg_io_write(avr, count, a, res);

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2 + 2, 1 + 1 + 1));
}

AVR_FAST_CORE_UINST_DECL(d5a6k8_in_ori_out)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2_R3(u_opcode, d, a, k);
	AVR_FAST_CORE_UINST_DEFN_vIO(a);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		/* CORE TRACE BUG ??? */
		STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), avr->data[a]);
	#else
		STATE("/ in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));
	
	/* ori h4, k8 */

	uint_fast8_t res = va | k;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("ori %s[%02x], 0x%02x\n", avr_regname(d), va, k);
	#else
		STATE("| ori %s[%02x], 0x%02x\n", avr_regname(d), va, k);
	#endif

	_avr_fast_core_store_r(avr, d, res);
	_avr_fast_core_flags_znv0s(avr, res);

	SREG();

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	/* out d5, a6 */

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), res);
	#else
		STATE("\\ out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), res);
	#endif

	_avr_fast_core_reg_io_write(avr, count, a, res);

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2 + 2, 1 + 1 + 1));
}

AVR_FAST_CORE_UINST_DECL(d5a6_in_push)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2(u_opcode, d, a);
	AVR_FAST_CORE_UINST_DEFN_vIO(a);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		/* CORE TRACE BUG ??? */
		STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), avr->data[a]);
	#else
		STATE("/ in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);
	#endif

	_avr_fast_core_store_r(avr, d, va);
	
	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	_avr_fast_core_push8(avr, count, va);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d), va, _avr_fast_core_sp_get(avr));
	#else
		STACK_STATE("\\ push %s[%02x] (@%04x)\n", avr_regname(d), va, _avr_fast_core_sp_get(avr));
	#endif

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 1 + 2));
}

AVR_FAST_CORE_UINST_DECL(d5a6m8_in_sbrs)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2_R3(u_opcode, d, a, mask);
	AVR_FAST_CORE_UINST_DEFN_vIO(a);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		/* CORE TRACE BUG ??? */
		STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), avr->data[a]);
	#else
		STATE("/ in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);
	#endif

	_avr_fast_core_store_r(avr, d, va);

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	int	branch = (0 != (va & (mask)));
	T(avr_flashaddr_t branch_pc = 2 + avr->pc);
	NO_T(avr_flashaddr_t branch_pc = 2 + 2 + avr->pc);

	T(int cycles = 1);
	NO_T(int cycles = 1 + 1);	

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("sbrs %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), va, mask, branch ? "":" not");
	#else
		STATE("\\ sbrs %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), va, mask, branch ? "":" not");
	#endif

	if (branch) {
		T(int shift = _avr_is_instruction_32_bits(avr, 2 + branch_pc));
		NO_T(int shift = _avr_is_instruction_32_bits(avr, 2 + branch_pc));

		#ifdef AVR_FAST_CORE_SKIP_SHIFT
			branch_pc += (2 << shift);
			cycles += (1 << shift);
		#else		
			if (shift) {
				branch_pc += 4;
				cycles += 2;
			} else {
				branch_pc += 2;
				cycles += 1;
			}
		#endif
	}
	
	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, cycles);
}

AVR_FAST_CORE_UINST_DECL(d5_inc)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v(u_opcode, d);
	uint_fast8_t res = vd + 1;

	STATE("inc %s[%02x] = %02x\n", avr_regname(d), vd, res);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	avr->sreg[S_V] = res == 0x80;

	_avr_fast_core_flags_zns(avr, res);

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5, inc)

AVR_FAST_CORE_UINST_DECL(x22_jmp)
{
	AVR_FAST_CORE_UINST_DEFN_24R1(u_opcode, x22);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("jmp 0x%06x\n", x22 >> 1);
	#else
		STATE("jmp 0x%06x\n", x22);
	#endif

	TRACE_JUMP();
	
	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(x22, 3);
}
AVR_FAST_CORE_INST_DEFN(x22, jmp)

AVR_FAST_CORE_UINST_DECL(d5rXYZ_ld_no_op)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2v16le(u_opcode, d, r);

	uint_fast8_t ivr = _avr_fast_core_fetch_ram(avr, count, vr);
	_avr_fast_core_store_r(avr, d, ivr);
	
	STATE("ld %s, %c[%04x]\n", avr_regname(d), *avr_regname(r), vr);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2);
}
AVR_FAST_CORE_INST_DEFN(d5rXYZ, ld_no_op)

AVR_FAST_CORE_UINST_DECL(d5rXYZ_ld_pre_dec)
{
	AVR_FAST_CORE_UINST_DEFN_R1_rmwR2v16le(u_opcode, d, r);

	vr--;

	uint_fast8_t ivr = _avr_fast_core_fetch_ram(avr, count, vr);
	_avr_fast_core_store_r(avr, d, ivr);
	
	STATE("ld %s, --%c[%04x]\n", avr_regname(d), *avr_regname(r), vr);

	AVR_FAST_CORE_RMW_STORE_R16LE(r, vr);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles (1 for tinyavr, except with inc/dec 2)
}
AVR_FAST_CORE_INST_DEFN(d5rXYZ, ld_pre_dec)

AVR_FAST_CORE_UINST_DECL(d5rXYZ_ld_post_inc)
{
	AVR_FAST_CORE_UINST_DEFN_R1_rmwR2v16le(u_opcode, d, r);

	uint_fast8_t ivr = _avr_fast_core_fetch_ram(avr, count, vr);
	_avr_fast_core_store_r(avr, d, ivr);
	
	vr++;
	
	STATE("ld %s, %c[%04x]++\n", avr_regname(d), *avr_regname(r), vr);

	AVR_FAST_CORE_RMW_STORE_R16LE(r, vr);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles (1 for tinyavr, except with inc/dec 2)
}
AVR_FAST_CORE_INST_DEFN(d5rXYZ, ld_post_inc)

AVR_FAST_CORE_UINST_DECL(d5rYZq6_ldd)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2v16le_R3(u_opcode, d, r, q);
	vr += q;
	
	uint_fast8_t ivr = _avr_fast_core_fetch_ram(avr, count, vr);
	_avr_fast_core_store_r(avr, d, ivr);

	STATE("ld %s, (%c+%d[%04x])=[%02x]\n", avr_regname(d), *avr_regname(r), q, vr, ivr);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles, 3 for tinyavr
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(d5rYZq6, ldd, d5, rYZ, q6)
	AVR_FAST_CORE_INST_DEFN_D5(d5b, next_opcode);
	AVR_FAST_CORE_INST_DEFN_Q6(q6b, next_opcode);

	if( ((i_opcode /* LDD.l.h */ & 0xd208) == (next_opcode /* LDD.h.l */ & 0xd208))
			&& (d5 == (d5b + 1)) && (q6 == (q6b - 1)) ) {
		u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(d5rYZq6_ldd_ldd, d5, rYZ, q6);
	} else
		combining=0;
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5rYZq6_ldd_ldd)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2v16le_R3(u_opcode, d, r, q);
	vr += q;

	uint16_t ivr;
	
	if(likely(0xff < vr)) {
		ivr = _avr_fast_core_data_read16be(avr, vr);
	} else {
		ivr = _avr_fast_core_fetch_ram(avr, count, vr) << 8;
		ivr |= _avr_fast_core_fetch_ram(avr, count, vr + 1);
	}
	
	_avr_fast_core_store_r16le(avr, d - 1, ivr);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("ld %s, (%c+%d[%04x])=[%02x]\n", avr_regname(d), *avr_regname(r), q, vr, ivr >> 8);
		T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 2));
		STATE("ld %s, (%c+%d[%04x])=[%02x]\n", avr_regname(d), *avr_regname(r), q + 1, vr + 1, ivr & 0xff);
		T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2)); // 2 cycles, 3 for tinyavr
		NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2)); // 2 cycles, 3 for tinyavr
	#else
		STATE("ld.w %s:%s, (%s+%d:%d[%04x:%04x])=[%04x]\n", 
			avr_regname(d), avr_regname(d - 1), 
			avr_regname(r), q, q + 1, vr, vr + 1, 
			ivr);
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2); // 2 cycles, 3 for tinyavr
	#endif
}

AVR_FAST_CORE_UINST_DECL(h4k8_ldi)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2(u_opcode, h, k);
	STATE("ldi %s, 0x%02x\n", avr_regname(h), k);

	_avr_fast_core_store_r(avr, h, k);
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(h4k8, ldi, h4, k8)
	AVR_FAST_CORE_INST_DEFN_H4(h4b, next_opcode);
	AVR_FAST_CORE_INST_DEFN_D5(d5, next_opcode);
	
	if( (0xe000 /* LDI.l */ == (i_opcode & 0xf000)) && (0xe000 /* LDI.h */ == (next_opcode & 0xf000))
			&& ((h4 + 1) == h4b) ) {
		AVR_FAST_CORE_INST_DEFN_K8(k8b, next_opcode);
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(h4k16_ldi_ldi, h4, k8, k8b);
	} else if( (0xe000 /* LDI.h */ == (i_opcode & 0xf000)) && (0xe000 /* LDI.l */ == (next_opcode & 0xf000))
			&& (h4 == (h4b + 1)) ) {
		AVR_FAST_CORE_INST_DEFN_K8(k8b, next_opcode);
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(h4k16_ldi_ldi, h4b, k8b, k8);
	} else if( (0xe000 /* LDI */ == (i_opcode & 0xf000)) && (0xb800 /* OUT */ == (next_opcode & 0xf800))
			&& (h4 == d5) ) {
		AVR_FAST_CORE_INST_DEFN_A6(a6, next_opcode);
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(h4k8a6_ldi_out, h4, k8, a6);
	} else

		combining = 0;
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(h4k16_ldi_ldi)
{
	AVR_FAST_CORE_UINST_DEFN_R1_16R2(u_opcode, h, k);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		//  For tracing purposes, there is no easier way to get around this...
		T(AVR_FAST_CORE_FLASH_OPCODE_DEFN(i_opcode_a, avr->pc));
		T(AVR_FAST_CORE_INST_DEFN_H4(h4a, i_opcode_a));
		T(AVR_FAST_CORE_INST_DEFN_K8(k8a, i_opcode_a));
		STATE("ldi %s, 0x%02x\n", avr_regname(h4a), k8a);
	#else
		STATE("ldi.w %s:%s, 0x%04x\n", avr_regname(h), avr_regname(h+1), k);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(AVR_FAST_CORE_FLASH_OPCODE_DEFN(i_opcode_b, avr->pc));
		T(AVR_FAST_CORE_INST_DEFN_H4(h4b, i_opcode_b));
		T(AVR_FAST_CORE_INST_DEFN_K8(k8b, i_opcode_b));
		STATE("ldi %s, 0x%02x\n", avr_regname(h4b), k8b);
	#endif

	_avr_fast_core_store_r16le(avr, h, k);

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

AVR_FAST_CORE_UINST_DECL(h4k8a6_ldi_out)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2_R3(u_opcode, h, k, a);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("ldi %s, 0x%02x\n", avr_regname(h), k);
	#else
		STATE("/ ldi %s, 0x%02x\n", avr_regname(h), k);
	#endif

	_avr_fast_core_store_r(avr, h, k);

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(h), k);
	#else
		STATE("\\ out %s, %s[%02x]\n", avr_regname(a), avr_regname(h), k);
	#endif

	_avr_fast_core_reg_io_write(avr, count, a, k);

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));	
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));	
}

AVR_FAST_CORE_UINST_DECL(d5x16_lds)
{
	AVR_FAST_CORE_UINST_DEFN_R1_16R2(u_opcode, d, x);
	
	uint_fast8_t vd = _avr_fast_core_fetch_ram(avr, count, x);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_fast_core_fetch_r(avr, d), x);
	#else
		STATE("lds %s, 0x%04x[%02x]\n", avr_regname(d), x, vd);
	#endif

	_avr_fast_core_store_r(avr, d, vd);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4, 2);	
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(d5x16, lds, d5, x16)
	AVR_FAST_CORE_INST_DEFN_D5(d5b, next_opcode);
	AVR_FAST_CORE_INST_DEFN_R5(r5, next_opcode);
	AVR_FAST_CORE_FLASH_OPCODE_DEFN(x16b, 2 + new_pc);

	if( (0x9000 /* LDS.l */ == (0xfe0f & i_opcode)) && (0x9000 /* LDS.h */ == (0xfe0f & next_opcode))
			&& ((d5 + 1) == d5b) && ((x16 + 1) == x16b) ) {
		if(0xff < x16) {
			u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5x16_lds_lds_no_io, d5, x16, 0);
		} else {
			u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5x16_lds_lds, d5, x16, 0);
		}
	} else if( (0x9000 /* LDS */ == (0xfe0f & i_opcode)) && (0x2000 /* TST */ == (0xfc00 & next_opcode))
			&& (d5 == d5b) && (d5 == r5) ) {
		if(0xff < x16) {
			u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5x16_lds_no_io_tst, d5, x16, 0);
		} else {
			u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5x16_lds_tst, d5, x16, 0);
		}
	} else {
		if(0xff < x16) {
			u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5x16_lds_no_io, d5, x16, 0);
		}
		combining = 0;
	}
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5x16_lds_no_io)
{
	AVR_FAST_CORE_UINST_DEFN_R1_16R2(u_opcode, d, x);

	uint_fast8_t vd = _avr_fast_core_data_read(avr, x);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		/* CORE BUG -- TRACE */
		STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_fast_core_fetch_r(avr, d), x);
	#else
		STATE("lds %s, 0x%04x[%02x]\n", avr_regname(d), x, vd);
	#endif

	_avr_fast_core_store_r(avr, d, vd);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4, 2);	
}

AVR_FAST_CORE_UINST_DECL(d5x16_lds_lds)
{
	AVR_FAST_CORE_UINST_DEFN_R1_16R2(u_opcode, d, x);

	/* lds low:high, sts high:low ... replicate order incase in the instance io is accessed. */

	uint_fast8_t vxl = _avr_fast_core_fetch_ram(avr, count, x);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_fast_core_fetch_r(avr, d), x);
	#else
		STATE("/ lds %s, 0x%04x[%02x]\n", avr_regname(d), x, vxl);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(4, 2));

	uint_fast8_t vxh = _avr_fast_core_fetch_ram(avr, count, x + 1);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("lds %s[%02x], 0x%04x\n", avr_regname(d + 1), _avr_fast_core_fetch_r(avr, d + 1), x + 1);
	#else
		STATE("\\ lds %s, 0x%04x[%02x]\n", avr_regname(d + 1), x + 1, vxh);
	#endif

	_avr_fast_core_store_r16le(avr, d, (vxh << 8) | vxl);

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4, 2));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4 + 4, 2 + 2));
}

AVR_FAST_CORE_UINST_DECL(d5x16_lds_lds_no_io)
{
	AVR_FAST_CORE_UINST_DEFN_R1_16R2(u_opcode, d, x);

	/* lds low:high, sts high:low ... replicate order incase in the instance io is accessed. */

	T(uint_fast16_t vx = _avr_fast_core_data_read16le(avr, x));
	NO_T(uint_fast16_t vx = _avr_fast_core_data_read16(avr, x));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_fast_core_fetch_r(avr, d), x);
	#else
		STATE("/ lds %s, 0x%04x[%02x]\n", avr_regname(d), x, vx & 0xff);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(4, 2));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("lds %s[%02x], 0x%04x\n", avr_regname(d + 1), _avr_fast_core_fetch_r(avr, d), x + 1);
	#else
		STATE("\\ lds %s, 0x%04x[%02x]\n", avr_regname(d + 1), x + 1, vx > 8);
	#endif

	T(_avr_fast_core_store_r16le(avr, d, vx));
	NO_T(_avr_fast_core_store_r16(avr, d, vx));

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4, 2));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4 + 4, 2 + 2));
}

AVR_FAST_CORE_UINST_DECL(d5x16_lds_no_io_tst)
{
	AVR_FAST_CORE_UINST_DEFN_R1_16R2(u_opcode, d, x);

	uint_fast8_t vd = _avr_fast_core_data_read(avr, x);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_fast_core_fetch_r(avr, d), x);
	#else
		STATE("/ lds %s, 0x%04x[%02x]\n", avr_regname(d), x, vd);
	#endif

	_avr_fast_core_store_r(avr, d, vd);

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(4, 2)); // 2 cycles

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("tst %s[%02x]\n", avr_regname(d), vd);
	#else
		STATE("\\ tst %s[%02x]\n", avr_regname(d), vd);
	#endif

	_avr_fast_core_flags_znv0s(avr, vd);

	SREG();
	
	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4 + 2, 2 + 1));
}

AVR_FAST_CORE_UINST_DECL(d5x16_lds_tst)
{
	AVR_FAST_CORE_UINST_DEFN_R1_16R2(u_opcode, d, x);
	
	uint_fast8_t vd = _avr_fast_core_fetch_ram(avr, count, x);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_fast_core_fetch_r(avr, d), x);
	#else
		STATE("/ lds %s, 0x%04x[%02x]\n", avr_regname(d), x, vd);
	#endif

	_avr_fast_core_store_r(avr, d, vd);

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(4, 2)); // 2 cycles

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("tst %s[%02x]\n", avr_regname(d), vd);
	#else
		STATE("\\ tst %s[%02x]\n", avr_regname(d), vd);
	#endif

	_avr_fast_core_flags_znv0s(avr, vd);

	SREG();
	
	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4 + 2, 2 + 1));
}

AVR_FAST_CORE_UINST_DECL(d5_lpm_z)
{
	AVR_FAST_CORE_UINST_DEFN_R1(u_opcode, d);
	uint_fast16_t z = _avr_fast_core_fetch_r16le(avr, R_ZL);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		/* CORE TRACE BUG -- LPM will always indicate as Z+ */
		STATE("lpm %s, (Z[%04x]+)\n", avr_regname(d), z);
	#else
		STATE("lpm %s, (Z[%04x])\n", avr_regname(d), z);
	#endif

	_avr_fast_core_store_r(avr, d, avr->flash[z]);
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 3); // 3 cycles
}
AVR_FAST_CORE_INST_DEFN(d5, lpm_z)

AVR_FAST_CORE_UINST_DECL(d5_lpm_z_post_inc)
{
	AVR_FAST_CORE_UINST_DEFN_R1(u_opcode, d);
	uint_fast8_t z = R_ZL;
	AVR_FAST_CORE_UINST_DEFN_rmwRv16le(z);

	STATE("lpm %s, (Z[%04x]+)\n", avr_regname(d), vz);

	_avr_fast_core_store_r(avr, d, avr->flash[vz]);

	vz++;

	AVR_FAST_CORE_RMW_STORE_R16LE(z, vz);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 3); // 3 cycles
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(d5, lpm_z_post_inc, d5)
	AVR_FAST_CORE_INST_DEFN_D5(d5b, next_opcode);

	if( (0x9005 /* LPM_Z+ */ == (0xfe0e & i_opcode)) && (0x920c /* ST+ */ == (0xfe0e & next_opcode))
			&& (d5 == d5b) ) {
		uint_fast8_t regs[4] = {R_ZL, 0x00, R_YL, R_XL};
		uint_fast8_t r = regs[(next_opcode & 0x000c)>>2];
		uint_fast8_t opr = next_opcode & 0x0003;
		if(opr == 1) {
			u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5rXYZ_lpm_z_post_inc_st_post_inc, d5, r, 0);
		} else {
			combining = 0;
		}
	 } else if( ((0xfe0f /* LPM_Z+.l */ & i_opcode) == (0xfe0f /* LPM_Z+.h */ & next_opcode))
			&& ((d5 + 1) == d5b) ) {
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5_lpm16_z_post_inc, d5, 0, 0);
		combining = 0;
	 } else {
		combining = 0;
	}
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5rXYZ_lpm_z_post_inc_st_post_inc)
{
	AVR_FAST_CORE_UINST_DEFN_R1_rmwR2v16le(u_opcode, d, r);
	uint_fast8_t z = R_ZL;
	AVR_FAST_CORE_UINST_DEFN_rmwRv16le(z);

	uint_fast8_t vd = avr->flash[z];

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("lpm %s, (Z[%04x]+)\n", avr_regname(d), z);
	#else
		STATE("/ lpm %s, (Z[%04x]+)\n", avr_regname(d), z);
	#endif

	_avr_fast_core_store_r(avr, d, vd);
	
	vz++;
	
	AVR_FAST_CORE_RMW_STORE_R16LE(z, vz);

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 3)); // 3 cycles

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("st %c[%04x]++, %s[0x%02x]\n", *avr_regname(r), vr,
			avr_regname(d), vd);
	#else
		STATE("\\ st %c[%04x]++, %s[0x%02x]\n", *avr_regname(r), vr,
			avr_regname(d), vd);
	#endif

	_avr_fast_core_store_ram(avr, count, vr, vd);
	vr++;
	
	AVR_FAST_CORE_RMW_STORE_R16LE(r, vr);

	
	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2)); // 2 cycles, except tinyavr
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 3 + 2)); 
}

AVR_FAST_CORE_UINST_DECL(d5_lpm16_z_post_inc)
{
	AVR_FAST_CORE_UINST_DEFN_R1(u_opcode, d);
	uint_fast8_t z = R_ZL;
	AVR_FAST_CORE_UINST_DEFN_rmwRv16le(z);

	T(uint_fast16_t vd = _avr_fast_core_flash_read16le(avr, vz));
	NO_T(uint_fast16_t vd = _avr_fast_core_fetch16(avr->flash, vz));
	
	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("lpm %s, (Z[%04x]+)\n", avr_regname(d), vz);
	#else
		STATE("lpm.w %s:%s, (Z[%04x:%04x]+) = 0x%04x\n", avr_regname(d), avr_regname(d + 1), vz, vz + 1, vd);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 3));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("lpm %s, (Z[%04x]+)\n", avr_regname(d+1), vz + 1);
	#endif

	T(_avr_fast_core_store_r16le(avr, d, vd));
	NO_T(_avr_fast_core_store_r16(avr, d, vd));
	
	vz += 2;
	
	AVR_FAST_CORE_RMW_STORE_R16LE(z, vz);

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 3));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 3 + 3));
}

AVR_FAST_CORE_UINST_DECL(d5_lsl)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v(u_opcode, d);
	uint_fast8_t res = vd << 1;

	STATE("lsl %s[%02x] = %02x\n", avr_regname(d), vd, res);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	_avr_fast_core_flags_znvs(avr, res);
	avr->sreg[S_H] = (vd >> 3) & 1;
	avr->sreg[S_C] = (vd >> 7) & 1;

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}

AVR_FAST_CORE_UINST_DECL(d5r5_lsl_lsl)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2(u_opcode, d, r);
	uint_fast8_t res = vd << r;

	STATE("lsl %s[%02x] = %02x\n", avr_regname(d), vd, res);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	_avr_fast_core_flags_znvs(avr, res);

	uint_fast8_t flags_vd = vd << (r - 1);
	avr->sreg[S_H] = (flags_vd >> 3) & 1;
	avr->sreg[S_C] = (flags_vd >> 7) & 1;

	SREG();

	/* 2 bytes/instruction, 1 cycle/instruction */ 
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(r << 1, r);
}

AVR_FAST_CORE_UINST_DECL(d5_lsl_rol)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v16le(u_opcode, d);
	uint_fast16_t res = vd << 1;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint8_t vdl = vd & 0xff; uint8_t res0 = res & 0xff);
		STATE("lsl %s[%02x] = %02x\n", avr_regname(d), vdl, res0);
		T(_avr_fast_core_flags_add_zns(avr, res0, vdl, vdl));
		SREG();
	#else
		STATE("lsr.w %s:%s[%04x] = [%04x]\n", avr_regname(d), avr_regname(d + 1), vd, res);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));
	
	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("rol %s[%02x]\n", avr_regname(d), vd >> 8);
	#endif

	AVR_FAST_CORE_RMW_STORE_R16LE(d, res);

	/* NOTE:
		where it concerns the emulation of the combination of lsl + rol,
		setting of flags only pertains to the high order byte quantity
		produced by result of the rol instruction. */
	
	_avr_fast_core_flags_znvs(avr, res >> 8);
	avr->sreg[S_H] = (vd >> 11) & 1;
	avr->sreg[S_C] = (vd >> 15) & 1;

	SREG();

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

AVR_FAST_CORE_UINST_DECL(d5_lsr)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v(u_opcode, d);
	uint_fast8_t res = vd >> 1;

	STATE("lsr %s[%02x]\n", avr_regname(d), vd);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	_avr_fast_core_flags_zcn0vs(avr, res, vd);

	SREG();
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(d5, lsr, d5)
	AVR_FAST_CORE_INST_DEFN_D5(__attribute__((__unused__))d5b, next_opcode);

	if( (0x9406 /* LSR */ == (0xfe0f & i_opcode)) && (0x9407 /* ROR */ == (0xfe0f & next_opcode))
			&& (d5 == (d5b + 1)) ) {
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5_lsr_ror, d5b, 0, 0);
	} else if(i_opcode == next_opcode) {
		int count = 1;
		do {
			new_pc += 2;
			count++;
			AVR_FAST_CORE_FLASH_OPCODE_FETCH(next_opcode, new_pc);
		} while((7 > count) && (i_opcode == next_opcode));

 		u_opcode =AVR_FAST_CORE_MAKE_UINST_OPCODE(d5r5_lsr_lsr, d5, count, 0);
	} else {
		combining = 0;
	}
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5r5_lsr_lsr)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2(u_opcode, d, r);
	uint_fast8_t res = vd >> r;

	STATE("lsr %s[%02x]\n", avr_regname(d), vd);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	_avr_fast_core_flags_zcn0vs(avr, res, (vd >> (r - 1)));

	SREG();
	
	/* 2 bytes/instruction, 1 cycle/instruction */ 
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(r << 1, r);
}

AVR_FAST_CORE_UINST_DECL(d5_lsr_ror)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v16le(u_opcode, d);
	uint_fast16_t res = vd >> 1;
	
	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint8_t vdh = vd >> 8);
		T(uint8_t res0 = vdh >> 1);

		STATE("lsr %s[%02x]\n", avr_regname(d + 1), vdh);
		T(_avr_fast_core_flags_zcn0vs(avr, res0, vdh));
		SREG();
	#else
		STATE("lsr.w %s:%s[%04x] = [%04x]\n", avr_regname(d), avr_regname(d + 1), vd, res);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));
	
	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint8_t vdl = vd & 0xff);
		STATE("ror %s[%02x]\n", avr_regname(d), vdl);
	#endif

	AVR_FAST_CORE_RMW_STORE_R16LE(d, res);

	_avr_fast_core_flags_zcnvs16(avr, res, vd);

	SREG();
	
	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

AVR_FAST_CORE_UINST_DECL(d5r5_mov)
{
	AVR_FAST_CORE_UINST_DEFN_R1(u_opcode, d);
	T(AVR_FAST_CORE_UINST_DEFN_R2v(u_opcode, r));
	NO_T(AVR_FAST_CORE_UINST_DEFN_R2(u_opcode, r));
	
	_avr_fast_core_mov_r(avr, d, r);
	
	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("mov %s, %s[%02x] = %02x\n", avr_regname(d), avr_regname(r), vr, vr);
	#else
		STATE("mov %s, %s[%02x]\n", avr_regname(d), avr_regname(r), vr);
	#endif

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5r5, mov)

AVR_FAST_CORE_UINST_DECL(d4r4_movw)
{
	T(AVR_FAST_CORE_UINST_DEFN_R1_R2v16le(u_opcode, d, r));
	NO_T(AVR_FAST_CORE_UINST_DEFN_R1_R2(u_opcode, d, r));
	
	_avr_fast_core_mov_r16(avr, d, r);

	STATE("movw %s:%s, %s:%s[%04x]\n", avr_regname(d), avr_regname(d+1), avr_regname(r), avr_regname(r+1), vr);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d4r4, movw)

AVR_FAST_CORE_UINST_DECL(d5r5_mul)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2v(u_opcode, d, r);
	uint_least16_t res = vd * vr;

	STATE("mul %s[%02x], %s[%02x] = %04x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_fast_core_store_r16le(avr, 0, res);

	_avr_fast_core_flags_zc16(avr, res);

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2);
}
AVR_FAST_CORE_INST_DEFN(d5r5, mul)

AVR_FAST_CORE_UINST_DECL(d16r16_muls)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2(u_opcode, d, r);
	int_least8_t vd = _avr_fast_core_fetch_r(avr, d);
	int_least8_t vr = _avr_fast_core_fetch_r(avr, r);
	int_least16_t res = vr * vd;

	STATE("muls %s[%d], %s[%02x] = %d\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_fast_core_store_r16le(avr, 0, res);

	_avr_fast_core_flags_zc16(avr, res);

	SREG();
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2);
}
AVR_FAST_CORE_INST_DEFN(d16r16, muls)

AVR_FAST_CORE_UINST_DECL(d5_neg)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v(u_opcode, d);
	uint_fast8_t res = 0x00 - vd;

	STATE("neg %s[%02x] = %02x\n", avr_regname(d), vd, res);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	avr->sreg[S_H] = ((res | vd) >> 3) & 1;
	avr->sreg[S_V] = res == 0x80;
	avr->sreg[S_C] = res != 0;

	_avr_fast_core_flags_zns(avr, res);

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5, neg)

AVR_FAST_CORE_UINST_DECL(x_nop)
{
	STATE("nop\n");
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(x, nop)

AVR_FAST_CORE_UINST_DECL(d5r5_or)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2v(u_opcode, d, r);
	uint_fast8_t res = vd | vr;

	STATE("or %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	_avr_fast_core_flags_znv0s(avr, res);

	SREG();
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5r5, or)

AVR_FAST_CORE_UINST_DECL(h4k8_ori)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2(u_opcode, h, k);
	uint_fast8_t res = vh | k;

	STATE("ori %s[%02x], 0x%02x\n", avr_regname(h), vh, k);

	AVR_FAST_CORE_RMW_STORE_R(h, res);

	_avr_fast_core_flags_znv0s(avr, res);

	SREG();
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(h4k8, ori)

AVR_FAST_CORE_UINST_DECL(d5a6_out)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2(u_opcode, d, a);

	STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), vd);

	_avr_fast_core_reg_io_write(avr, count, a, vd);
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(d5a6, out, d5, a6)
	if((0xb800 /* OUT */ == (0xf800 & i_opcode)) && (R_SPH == a6)) {
		AVR_FAST_CORE_INST_DEFN_A6(__attribute__((__unused__))a6b, next_opcode);
		if((0xb800 /* OUT */ == (0xf800 & next_opcode)) && (R_SREG == a6b)) {
			AVR_FAST_CORE_FLASH_OPCODE_DEFN(next_next_opcode, 2 + new_pc);
			AVR_FAST_CORE_INST_DEFN_D5(__attribute__((__unused__))d5b, next_opcode);
			AVR_FAST_CORE_INST_DEFN_A6(__attribute__((__unused__))a6c, next_next_opcode);
			if((0xb800 /* OUT */ == (0xf800 & next_next_opcode)) && (R_SPL == a6c)) {
				AVR_FAST_CORE_INST_DEFN_D5(__attribute__((__unused__))d5c, next_next_opcode);
				u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5r5_out_sph_sreg_spl, d5, d5b, d5c);
			} else {
				combining = 0;
			}
		} else {
			combining = 0;
		}
	} else {
		combining = 0;
	}
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5r5_out_sph_sreg_spl)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2v_R3v(u_opcode, d, r, d2);

	/* R_SPH */
	STATE("out %s, %s[%02x]\n", avr_regname(R_SPH), avr_regname(d), vd);

	_avr_fast_core_reg_io_write(avr, count, R_SPH, vd);

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));
	
	/* R_SREG */
	STATE("out %s, %s[%02x]\n", avr_regname(R_SREG), avr_regname(r), vr);

	_avr_fast_core_reg_io_write(avr, count, R_SREG, vr);

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));
	
	/* R_SPL */
	STATE("out %s, %s[%02x]\n", avr_regname(R_SPL), avr_regname(d2), vd2);

	_avr_fast_core_reg_io_write(avr, count, R_SPL, vd2);

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2 + 2, 1 + 1 + 1));
}

AVR_FAST_CORE_UINST_DECL(d5_pop)
{
	AVR_FAST_CORE_UINST_DEFN_R1(u_opcode, d);

	uint_fast8_t vd = _avr_fast_core_pop8(avr, count);
	_avr_fast_core_store_r(avr, d, vd);

	STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d), _avr_fast_core_sp_get(avr), vd);
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2);
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(d5, pop, d5)
	AVR_FAST_CORE_INST_DEFN_D5(__attribute__((__unused__))d5b, next_opcode);

	if( (0x900f /* POP.h*/ == (0xfe0f & i_opcode)) && (0x900f /* POP.l */ == (0xfe0f & next_opcode))
			&& (d5 == (d5b + 1)) ) {
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5_pop_pop16be, d5b, 0, 0);
	} else if( (0x900f /* POP.l */ == (0xfe0f & i_opcode)) && (0x900f /* POP.h */ == (0xfe0f & next_opcode))
			&& ((d5 + 1) == d5b) ) {
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5_pop_pop16le, d5, 0, 0);
	} else if( (0x900f /* POP */ == (0xfe0f & i_opcode)) && (0xb800 /* OUT */ == (0xf800 & next_opcode))
			&& (d5 == d5b) ) {
		AVR_FAST_CORE_INST_DEFN_A6(a6, next_opcode);
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5a6_pop_out, d5, a6, 0);
	} else {
		combining = 0;
	}
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5a6_pop_out)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2(u_opcode, d, a);

	uint_fast8_t vd = _avr_fast_core_pop8(avr, count);
	_avr_fast_core_store_r(avr, d, vd);

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d), _avr_fast_core_sp_get(avr), vd);
	#else
		STACK_STATE("/ pop %s (@%04x)[%02x]\n", avr_regname(d), _avr_fast_core_sp_get(avr), vd);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 2));
	
	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), vd);
	#else
		STATE("\\ out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), vd);
	#endif

	_avr_fast_core_reg_io_write(avr, count, a, vd);
	
	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 2 + 1));
}

AVR_FAST_CORE_UINST_DECL(d5_pop_pop16be)
{
	AVR_FAST_CORE_UINST_DEFN_R1(u_opcode, d);

	uint_fast16_t vd = _avr_fast_core_pop16be(avr, count);
	T(uint_fast16_t sp = _avr_fast_core_sp_get(avr));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d + 1), sp - 1, vd >> 8);
	#else
		STACK_STATE("/ pop %s (@%04x)[%02x]\n", avr_regname(d + 1), sp - 1, vd >> 8);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 2));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d), sp, vd & 0xff);
	#else
		STACK_STATE("\\ pop %s (@%04x)[%02x]\n", avr_regname(d), sp, vd & 0xff);
	#endif

	_avr_fast_core_store_r16le(avr, d, vd);

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2));
}

AVR_FAST_CORE_UINST_DECL(d5_pop_pop16le)
{
	AVR_FAST_CORE_UINST_DEFN_R1(u_opcode, d);

	uint_fast16_t vd = _avr_fast_core_pop16le(avr, count);
	T(uint_fast16_t sp = _avr_fast_core_sp_get(avr));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d), sp - 1, vd & 0xff);
	#else
		STACK_STATE("/ pop %s (@%04x)[%02x]\n", avr_regname(d), sp - 1, vd & 0xff);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 2));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d + 1), sp, vd >> 8);
	#else
		STACK_STATE("\\ pop %s (@%04x)[%02x]\n", avr_regname(d + 1), sp, vd >> 8);
	#endif

	_avr_fast_core_store_r16le(avr, d, vd);

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2));
}

AVR_FAST_CORE_UINST_DECL(d5_push) {
	AVR_FAST_CORE_UINST_DEFN_R1v(u_opcode, d);

	_avr_fast_core_push8(avr, count, vd);

	STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d), vd, _avr_fast_core_sp_get(avr));

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2);
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(d5, push, d5)
	AVR_FAST_CORE_INST_DEFN_D5(__attribute__((__unused__))d5b, next_opcode);

	if( (0x920f /* PUSH.l */ == (0xfe0f & i_opcode)) && (0x920f /* PUSH.h */ == (0xfe0f & next_opcode))
			&& ((d5 + 1) == d5b) ) {
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5_push_push16be, d5, 0, 0);
	} else if( (0x920f /* PUSH.h */ == (0xfe0f & i_opcode)) && (0x920f /* PUSH.l */ == (0xfe0f & next_opcode))
			&& (d5 == (d5b + 1)) ) {
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5_push_push16le, d5b, 0, 0);
	} else {
		combining = 0;
	}
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5_push_push16be)
{
	AVR_FAST_CORE_UINST_DEFN_R1v16le(u_opcode, d);

	_avr_fast_core_push16be(avr, count, vd);
	T(uint_fast16_t sp = _avr_fast_core_sp_get(avr));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d), vd & 0xff, sp + 1);
		T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 2));
		STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d + 1), vd >> 8, sp);
		T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2));
		NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2));
	#else
		STACK_STATE("push.w %s:%s[%04x] (@%04x)\n", avr_regname(d+1), avr_regname(d), vd, sp);
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2);
	#endif
}

AVR_FAST_CORE_UINST_DECL(d5_push_push16le)
{
	AVR_FAST_CORE_UINST_DEFN_R1v16le(u_opcode, d);

	_avr_fast_core_push16le(avr, count, vd);
	T(uint_fast16_t sp = _avr_fast_core_sp_get(avr));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d + 1), vd >> 8, sp + 1);
		T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2));
		STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d), vd & 0xff, sp);
		T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2));
		NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2));
	#else
		STACK_STATE("push.w %s:%s[%04x] (@%04x)\n", avr_regname(d+1), avr_regname(d), vd, sp);
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2)
	#endif
}

AVR_FAST_CORE_UINST_DECL(o12_rcall)
{
	AVR_FAST_CORE_UINST_DEFN_16R2(u_opcode, o);
	avr_flashaddr_t new_pc = 2 + avr->pc;
	avr_flashaddr_t branch_pc = new_pc + (int16_t)o;

	STATE("rcall .%d [%04x]\n", (int16_t)o >> 1, branch_pc);

	_avr_fast_core_push16be(avr, count, new_pc >> 1);

	// 'rcall .1' is used as a cheap "push 16 bits of room on the stack"
	if (o != 0) {
		TRACE_JUMP();
		STACK_FRAME_PUSH();
	}

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 3);
}
AVR_FAST_CORE_INST_DEFN(o12, rcall)

AVR_FAST_CORE_UINST_DECL(x_ret)
{
	STATE("ret\n");

	TRACE_JUMP();
	STACK_FRAME_POP();
	
	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(_avr_fast_core_pop16be(avr, count) << 1, 4);
}
AVR_FAST_CORE_INST_DEFN(x, ret)

AVR_FAST_CORE_UINST_DECL(x_reti)
{
	*count = 0;
	avr->sreg[S_I] = 1;

	STATE("reti\n");

	TRACE_JUMP();
	STACK_FRAME_POP();

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(_avr_fast_core_pop16be(avr, count) << 1, 4);
}
AVR_FAST_CORE_INST_DEFN(x, reti)

AVR_FAST_CORE_UINST_DECL(o12_rjmp)
{
	AVR_FAST_CORE_UINST_DEFN_16R2(u_opcode, o);
	avr_flashaddr_t new_pc = 2 + avr->pc;
	avr_flashaddr_t	branch_pc = new_pc + (int16_t)o;

	STATE("rjmp .%d [%04x]\n", (int16_t)o >> 1, branch_pc);

	TRACE_JUMP();

	AVR_FAST_CORE_UINST_NEXT_JMP_PC_CYCLES(branch_pc, 2);
}
AVR_FAST_CORE_INST_DEFN(o12, rjmp)

AVR_FAST_CORE_UINST_DECL(d5_rol)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v(u_opcode, d);
	uint_fast8_t res = (vd << 1) + avr->sreg[S_C];

	STATE("rol %s[%02x] = %02x\n", avr_regname(d), vd, res);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	_avr_fast_core_flags_add_zns(avr, res, vd, vd);

	SREG();
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}

AVR_FAST_CORE_UINST_DECL(d5_ror)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v(u_opcode, d);
	
	uint_fast8_t res;
	
	#ifdef AVR_FAST_CORE_BRANCHLESS_WITH_MULTIPLY
		res = (avr->sreg[S_C] * 0x80) | vd >> 1;
	#else
		res = (avr->sreg[S_C] ? 0x80 : 0) | vd >> 1;
	#endif

	STATE("ror %s[%02x]\n", avr_regname(d), vd);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	_avr_fast_core_flags_zcnvs(avr, res, vd);

	SREG();
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5, ror)

AVR_FAST_CORE_UINST_DECL(d5r5_sbc)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2v(u_opcode, d, r);
	uint_fast8_t res = vd - vr - avr->sreg[S_C];

	STATE("sbc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	_avr_fast_core_flags_sub_Rzns(avr, res, vd, vr);

	SREG();
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5r5, sbc)

AVR_FAST_CORE_UINST_DECL(h4k8_sbci)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2(u_opcode, h, k);
	uint_fast8_t res = vh - k - avr->sreg[S_C];

	STATE("sbci %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);

	AVR_FAST_CORE_RMW_STORE_R(h, res);

	#ifdef AVR_CORE_FAST_CORE_BUGS
		// CORE BUG -- standard core does not calculate H and V flags.
		avr->sreg[S_C] = (k + avr->sreg[S_C]) > vh;
		_avr_fast_core_flags_Rzns(avr, res);
	#else
		_avr_fast_core_flags_sub_Rzns(avr, res, vh, k);
	#endif

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(h4k8, sbci)

AVR_FAST_CORE_UINST_DECL(a5m8_sbi)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2(u_opcode, io, mask);
	AVR_FAST_CORE_UINST_DEFN_vIO(io);
	uint_fast8_t res = vio | mask;

	STATE("sbi %s[%04x], 0x%02x = %02x\n", avr_regname(io), vio, mask, res);

	_avr_fast_core_reg_io_write(avr, count, io, res);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2);
}
AVR_FAST_CORE_INST_DEFN(a5m8, sbi)

AVR_FAST_CORE_UINST_DECL(a5m8_16_sbic)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2(u_opcode, io, mask);
	AVR_FAST_CORE_UINST_DEFN_vIO(io);
	uint_fast8_t skip = (0 == (vio & mask));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("sbic %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), vio, mask, skip ? "":" not");
	#else
		STATE("sbic %s[%04x], 0x%02x\t; Will%s skip\n", avr_regname(io), vio, mask, skip ? "":" not");
	#endif

	#ifdef AVR_FAST_CORE_SKIP_SHIFT
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 << skip, 1 + skip);
	#else
		if (skip) {
			AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4, 2);
		}
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
	#endif
}
AVR_FAST_CORE_INST_DEFN(a5m8, 16_sbic)

AVR_FAST_CORE_UINST_DECL(a5m8_32_sbic)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2(u_opcode, io, mask);
	AVR_FAST_CORE_UINST_DEFN_vIO(io);
	
	uint_fast8_t skip = (0 == (vio & mask));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("sbic %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), vio, mask, skip ? "":" not");
	#else
		STATE("sbic %s[%04x], 0x%02x\t; Will%s skip\n", avr_regname(io), vio, mask, skip ? "":" not");
	#endif

	#ifdef AVR_FAST_CORE_32_SKIP_SHIFT
		int skip_count = skip ? 3 : 1;
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(skip_count << 1, skip_count);
	#else
		if (skip) {
			AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(6, 3);
		}
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
	#endif
}
AVR_FAST_CORE_INST_DEFN(a5m8, 32_sbic)

AVR_FAST_CORE_UINST_DECL(a5m8_16_sbis)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2(u_opcode, io, mask);
	AVR_FAST_CORE_UINST_DEFN_vIO(io);
	
	uint_fast8_t skip = (0 != (vio & mask));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("sbis %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), vio, mask, skip ? "":" not");
	#else
		STATE("sbis %s[%04x], 0x%02x\t; Will%s skip\n", avr_regname(io), vio, mask, skip ? "":" not");
	#endif

	#ifdef AVR_FAST_CORE_SKIP_SHIFT
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 << skip, 1 + skip);
	#else
		if (skip) {
			AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4, 2);
		}
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
	#endif
}
AVR_FAST_CORE_INST_DEFN(a5m8, 16_sbis)

AVR_FAST_CORE_UINST_DECL(a5m8_32_sbis)
{
	AVR_FAST_CORE_UINST_DEFN_R1_R2(u_opcode, io, mask);
	AVR_FAST_CORE_UINST_DEFN_vIO(io);
	
	uint_fast8_t skip = (0 != (vio & mask));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("sbis %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), vio, mask, skip ? "":" not");
	#else
		STATE("sbis %s[%04x], 0x%02x\t; Will%s skip\n", avr_regname(io), vio, mask, skip ? "":" not");
	#endif

	#ifdef AVR_FAST_CORE_32_SKIP_SHIFT
		int skip_count = skip ? 3 : 1;
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(skip_count << 1, skip_count);
	#else
		if (skip) {
			AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(6, 3);
		}
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
	#endif
}
AVR_FAST_CORE_INST_DEFN(a5m8, 32_sbis)

AVR_FAST_CORE_UINST_DECL(p2k6_sbiw)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v16le_R2(u_opcode, p, k);
	uint_fast16_t res = vp - k;

	STATE("sbiw %s:%s[%04x], 0x%02x\n", avr_regname(p), avr_regname(p+1), vp, k);

	AVR_FAST_CORE_RMW_STORE_R16LE(p, res);

	avr->sreg[S_V] = ((vp & (~res)) >> 15) & 1;
	avr->sreg[S_C] = ((res & (~vp)) >> 15) & 1;

	_avr_fast_core_flags_zns16(avr, res);

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2);
}
AVR_FAST_CORE_INST_DEFN(p2k6, sbiw)

AVR_FAST_CORE_UINST_DECL(d5m8_16_sbrc)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2(u_opcode, d, mask);
	int	skip = (0 == (vd & (mask)));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("sbrc %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), vd, mask, skip ? "":" not");
	#else
		STATE("sbrc %s[%02x], 0x%02x\t; Will%s skip\n", avr_regname(d), vd, mask, skip ? "":" not");
	#endif

	#ifdef AVR_FAST_CORE_SKIP_SHIFT
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 << skip, 1 + skip);
	#else
		if (skip) {
			AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4, 2);
		}
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
	#endif
}
AVR_FAST_CORE_INST_DEFN(d5m8, 16_sbrc)

AVR_FAST_CORE_UINST_DECL(d5m8_32_sbrc)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2(u_opcode, d, mask);
	int	skip = (0 == (vd & (mask)));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("sbrc %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), vd, mask, skip ? "":" not");
	#else
		STATE("sbrc %s[%02x], 0x%02x\t; Will%s skip\n", avr_regname(d), vd, mask, skip ? "":" not");
	#endif

	#ifdef AVR_FAST_CORE_32_SKIP_SHIFT
		int skip_count = skip ? 3 : 1;
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(skip_count << 1, skip_count);
	#else
		if (skip) {
			AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(6, 3);
		}
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
	#endif
}
AVR_FAST_CORE_INST_DEFN(d5m8, 32_sbrc)

AVR_FAST_CORE_UINST_DECL(d5m8_16_sbrs)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2(u_opcode, d, mask);
	int	skip = (0 != (vd & (mask)));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("sbrs %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), vd, mask, skip ? "":" not");
	#else
		STATE("sbrs %s[%02x], 0x%02x\t; Will%s skip\n", avr_regname(d), vd, mask, skip ? "":" not");
	#endif

	#ifdef AVR_FAST_CORE_SKIP_SHIFT
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 << skip, 1 + skip);
	#else
		if (skip) {
			AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4, 2);
		}
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
	#endif
}
AVR_FAST_CORE_INST_DEFN(d5m8, 16_sbrs)

AVR_FAST_CORE_UINST_DECL(d5m8_32_sbrs)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2(u_opcode, d, mask);
	int	skip = (0 != (vd & (mask)));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("sbrs %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), vd, mask, skip ? "":" not");
	#else
		STATE("sbrs %s[%02x], 0x%02x\t; Will%s skip\n", avr_regname(d), vd, mask, skip ? "":" not");
	#endif

	#ifdef AVR_FAST_CORE_32_SKIP_SHIFT
		int skip_count = skip ? 3 : 1;
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(skip_count << 1, skip_count);
	#else
		if (skip) {
			AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(6, 3);
		}
		AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
	#endif
}
AVR_FAST_CORE_INST_DEFN(d5m8, 32_sbrs)

AVR_FAST_CORE_UINST_DECL(sei)
{
	/* 
		avr instruction manual states there shall be one cycle
		after issuing an sei instruction. */

	*count = 1;
	avr->sreg[S_I]=1;

	STATE("sei\n");
	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}

AVR_FAST_CORE_UINST_DECL(sei_sleep)
{
	avr->sreg[S_I]=1;

	STATE("sei\n");
	SREG();

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));
	
	STATE("sleep\n");
	/* Don't sleep if there are interrupts about to be serviced.
	 * Without this check, it was possible to incorrectly enter a state
	 * in which the cpu was sleeping and interrupts were disabled. For more
	 * details, see the commit message. */

	if(!avr_has_pending_interrupts(avr)) {
		avr->state = cpu_Sleeping;
		#ifdef AVR_FAST_CORE_SLEEP_PREPROCESS_TIMERS_AND_INTERRUPTS
			CYCLES(1);
			T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, *count));
			NO_T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2 + 2, *count));
			_avr_fast_core_cycle_timer_process(avr, 0);
			_avr_fast_core_service_interrupts(avr);
		#else
			*count = 0;
			T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
			NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
		#endif
	}
}

AVR_FAST_CORE_UINST_DECL(x_sleep)
{
	STATE("sleep\n");
	/* Don't sleep if there are interrupts about to be serviced.
	 * Without this check, it was possible to incorrectly enter a state
	 * in which the cpu was sleeping and interrupts were disabled. For more
	 * details, see the commit message. */
	if(!avr_has_pending_interrupts(avr) || !avr->sreg[S_I]) {
		avr->state = cpu_Sleeping;
		#ifdef AVR_FAST_CORE_SLEEP_PREPROCESS_TIMERS_AND_INTERRUPTS
			CYCLES(1);
			AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, *count);
			_avr_fast_core_cycle_timer_process(avr, 0);
			_avr_fast_core_service_interrupts(avr);
		#else
			*count = 0;
			AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
		#endif
	}
}
AVR_FAST_CORE_INST_DEFN(x, sleep)

AVR_FAST_CORE_UINST_DECL(d5rXYZ_st_no_op)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2v16le(u_opcode, d, r);

	STATE("st %c[%04x], %s[%02x] \n", *avr_regname(r), vr, avr_regname(d), vd);

	_avr_fast_core_store_ram(avr, count, vr, vd);
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles, except tinyavr
}
AVR_FAST_CORE_INST_DEFN(d5rXYZ, st_no_op)

AVR_FAST_CORE_UINST_DECL(d5rXYZ_st_pre_dec)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_rmwR2v16le(u_opcode, d, r);

	STATE("st --%c[%04x], %s[%02x] \n", *avr_regname(r), vr, avr_regname(d), vd);

	vr--;
	_avr_fast_core_store_ram(avr, count, vr, vd);

	AVR_FAST_CORE_RMW_STORE_R16LE(r, vr);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles, except tinyavr
}
AVR_FAST_CORE_INST_DEFN(d5rXYZ, st_pre_dec)

AVR_FAST_CORE_UINST_DECL(d5rXYZ_st_post_inc)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_rmwR2v16le(u_opcode, d, r);

	STATE("st %c[%04x]++, %s[%02x] \n", *avr_regname(r), vr, avr_regname(d), vd);

	_avr_fast_core_store_ram(avr, count, vr, vd);
	vr++;
	
	AVR_FAST_CORE_RMW_STORE_R16LE(r, vr);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles, except tinyavr
}
AVR_FAST_CORE_INST_DEFN(d5rXYZ, st_post_inc)

AVR_FAST_CORE_UINST_DECL(d5rYZq6_std)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_R2v16le_R3(u_opcode, d, r, q);
	vr += q;

	STATE("st (%c+%d[%04x]), %s[%02x]\n", *avr_regname(r), q, vr, avr_regname(d), vd);

	_avr_fast_core_store_ram(avr, count, vr, vd);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles, except tinyavr
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(d5rYZq6, std, d5, rZY, q6)
	AVR_FAST_CORE_INST_DEFN_D5(__attribute__((__unused__)) d5b, next_opcode);
	AVR_FAST_CORE_INST_DEFN_Q6(__attribute__((__unused__)) q6b, next_opcode);

	if( ((i_opcode /* STD.h.h */ & 0xd208) == (next_opcode /* STD.l.l */& 0xd208))
			&& (d5 == (d5b + 1)) && (q6 == (q6b + 1)) ) {
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5rYZq6_std_std_hhll, d5b, rZY, q6b);
	} else	if( ((i_opcode /* STD.l.h */ & 0xd208) == (next_opcode /* STD.h.l */ & 0xd208))
			&& ((d5 + 1) == d5b) && (q6 == (q6b + 1)) ) {
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5rYZq6_std_std_hllh, d5, rZY, q6b);
	} else
		combining = 0;
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5rYZq6_std_std_hhll)
{
	AVR_FAST_CORE_UINST_DEFN_R1v16le_R2v16le_R3(u_opcode, d, r, q);
	vr += q;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("st (%c+%d[%04x]), %s[%02x]\n", *avr_regname(r), q + 1, vr + 1, avr_regname(d), vd >> 8);
	#else
		STATE("st (%c+%d:%d[%04x:%04x]), %s:%s[%04x]\n", *avr_regname(r),
			q, q + 1, vr, vr + 1, avr_regname(d), avr_regname(d + 1), vd);
	#endif

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2)); // 2 cycles, except tinyavr

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("st (%c+%d[%04x]), %s[%02x]\n", *avr_regname(r), q, vr, avr_regname(d), vd & 0xff);
	#endif

	if(likely(0xff < vr)) {
		_avr_fast_core_data_write16le(avr, vr, vd);
	} else {
		_avr_fast_core_store_ram(avr, count, vr + 1, vd >> 8);
		_avr_fast_core_store_ram(avr, count, vr, vd & 0xff);
	}

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 2)); // 2 cycles, except tinyavr
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2));
}

AVR_FAST_CORE_UINST_DECL(d5rYZq6_std_std_hllh)
{
	AVR_FAST_CORE_UINST_DEFN_R1v16le_R2v16le_R3(u_opcode, d, r, q);
	vr += q;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("st (%c+%d[%04x]), %s[%02x]\n", *avr_regname(r), q + 1, vr + 1, avr_regname(d), vd & 0xff);
	#else
		STATE("st (%c+%d:%d[%04x:%04x]), %s:%s[%04x]\n", *avr_regname(r),
			q + 1, q , vr + 1, vr, avr_regname(d), avr_regname(d + 1), vd);
	#endif

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 2)); // 2 cycles, except tinyavr

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("st (%c+%d[%04x]), %s[%02x]\n", *avr_regname(r), q, vr, avr_regname(d), vd >> 8);
	#endif

	if(likely(0xff < vr)) {
		_avr_fast_core_data_write16be(avr, vr, vd);
	} else {
		_avr_fast_core_store_ram(avr, count, vr + 1, vd & 0xff);
		_avr_fast_core_store_ram(avr, count, vr , vd >> 8);
	}

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 2)); // 2 cycles, except tinyavr
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2));
}

AVR_FAST_CORE_UINST_DECL(d5x16_sts)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_16R2(u_opcode, d, x);

	STATE("sts 0x%04x, %s[%02x]\n", x, avr_regname(d), vd);

	_avr_fast_core_store_ram(avr, count, x, vd);
		
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4, 2);
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(d5x16, sts, d5, x16)
	AVR_FAST_CORE_INST_DEFN_D5(d5b, next_opcode);
	AVR_FAST_CORE_FLASH_OPCODE_DEFN(x16b, 2 + new_pc);

	if( (0x9200 /* STS.h */ == (0xfe0f & i_opcode)) && (0x9200 /* STS.l */ == (0xfe0f & next_opcode))
			&& (d5 == (d5b + 1)) && (x16 == (x16b + 1)) ) {
		if(0xff < x16b)
			u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5x16_sts_sts_no_io, d5b, x16b, 0);
		else
			u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5x16_sts_sts, d5b, x16b, 0);
	} else {
		if(0xff < x16)
			u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(d5x16_sts_no_io, d5, x16, 0);
		
		combining = 0;
	}
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(d5x16_sts_no_io)
{
	AVR_FAST_CORE_UINST_DEFN_R1v_16R2(u_opcode, d, x);

	STATE("sts 0x%04x, %s[%02x]\n", x, avr_regname(d), vd);

	_avr_fast_core_data_write(avr, x, vd);
		
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4, 2);
}

AVR_FAST_CORE_UINST_DECL(d5x16_sts_sts)
{
	AVR_FAST_CORE_UINST_DEFN_R1v16le_16R2(u_opcode, d, x);

	/* lds low:high, sts high:low ... replicate order incase in the instance io is accessed. */

	uint_fast8_t vdh = vd >> 8;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("sts 0x%04x, %s[%02x]\n", x + 1, avr_regname(d), vdh);
	#else
		STATE("sts.w 0x%04x:0x%04x, %s:%s[%04x]\n", x, x + 1,
			avr_regname(d), avr_regname(d + 1),
			_avr_fast_core_fetch_r16(avr, d));
	#endif

	_avr_fast_core_store_ram(avr, count, x + 1, vdh);

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(4, 2));

	uint_fast8_t vdl = vd & 0xff;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		STATE("sts 0x%04x, %s[%02x]\n", x, avr_regname(d), vdl);
	#endif

	_avr_fast_core_store_ram(avr, count, x, vdl);

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(4, 2));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4 + 4, 2 + 2));
}

AVR_FAST_CORE_UINST_DECL(d5x16_sts_sts_no_io)
{
	T(AVR_FAST_CORE_UINST_DEFN_R1v16le_16R2(u_opcode, d, x));
	NO_T(AVR_FAST_CORE_UINST_DEFN_v16R1_16R2(u_opcode, d, x));
	
	/* lds low:high, sts high:low ...
		normally, replicate order incase in the instance io is accessed. */

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint_fast8_t vdh = vd >> 8);
		STATE("sts 0x%04x, %s[%02x]\n", x + 1, avr_regname(d), vdh);
	#else
		STATE("sts.w 0x%04x:0x%04x, %s:%s[%04x]\n", x, x + 1, avr_regname(d), avr_regname(d + 1), vd);
	#endif

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(4, 2));

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint_fast8_t vdl = vd & 0xff);
		STATE("sts 0x%04x, %s[%02x]\n", x, avr_regname(d), vdl);
	#endif

	T(_avr_fast_core_data_write16le(avr, x, vd));
	NO_T(_avr_fast_core_data_write16(avr, x, vd));

	T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(4, 2));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(4 + 4, 2 + 2));
}

AVR_FAST_CORE_UINST_DECL(d5r5_sub)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2v(u_opcode, d, r);
	uint_fast8_t res = vd - vr;

	STATE("sub %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	_avr_fast_core_flags_sub_zns(avr, res, vd, vr);

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5r5, sub)

AVR_FAST_CORE_UINST_DECL(h4k8_subi)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v_R2(u_opcode, h, k);
	uint_fast8_t res = vh - k;

	STATE("subi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);

	AVR_FAST_CORE_RMW_STORE_R(h, res);

	#ifdef AVR_CORE_FAST_CORE_BUGS
		// CORE BUG -- standard core does not calculate H and V flags.
		avr->sreg[S_C] = k > vh;
		_avr_fast_core_flags_zns(avr, res);
	#else
		_avr_fast_core_flags_sub_zns(avr, res, vh, k);
	#endif

	SREG();

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_BEGIN_COMBINING_INST_DEFN(h4k8, subi, h4, k8)
	AVR_FAST_CORE_INST_DEFN_H4(__attribute__((__unused__)) h4b, next_opcode);

	if( (0x5000 /* SUBI.l */ == (i_opcode & 0xf000)) && (0x4000 /* SBCI.h */ == (next_opcode & 0xf000))
			&& ((h4 + 1) == h4b) ) {
		AVR_FAST_CORE_INST_DEFN_K8(__attribute__((__unused__)) k8b, next_opcode);
		u_opcode = AVR_FAST_CORE_MAKE_UINST_OPCODE(h4k16_subi_sbci, h4, k8, k8b);
	} else
		combining = 0;
AVR_FAST_CORE_END_COMBINING_INST_DEFN

AVR_FAST_CORE_UINST_DECL(h4k16_subi_sbci)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v16le_16R2(u_opcode, h, k);
	uint_fast16_t res = vh - k;

	#ifdef AVR_CORE_FAST_CORE_DIFF_TRACE
		T(uint8_t vhl = vh & 0xff; uint8_t vkl = k & 0xff);
		T(uint8_t res0 = vhl - vkl);
		STATE("subi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vhl, vkl, res0);

		#ifdef AVR_CORE_FAST_CORE_BUGS
			// CORE BUG -- standard core does not calculate H and V flags.
			T(avr->sreg[S_C] = vkl > vhl);
			T(_avr_fast_core_flags_zns(avr, res0));
		#else
			T(_avr_fast_core_flags_sub_zns(avr, res0, vhl, vkl));
		#endif

		SREG();

		T(AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1));

		T(uint8_t vhh = vh >> 8; uint8_t vkh = k >> 8);
		T(uint8_t res1 = vhh - vkh - avr->sreg[S_C]);
		STATE("sbci %s[%02x], 0x%02x = %02x\n", avr_regname(h + 1), vhh, vkh, res1);
	#else
		STATE("subi.sbci %s:%s[%04x], 0x%04x = %04x\n", avr_regname(h), avr_regname(h + 1), vh, k, res);
	#endif
	
	AVR_FAST_CORE_RMW_STORE_R16LE(h, res);

	#ifdef AVR_CORE_FAST_CORE_BUGS
		// CORE BUG -- standard core does not calculate H and V flags.
		avr->sreg[S_C] = k > vh;
		_avr_fast_core_flags_zns16(avr, res);
	#else
		_avr_fast_core_flags_sub16_zns16(avr, res, vh, k);
	#endif

	SREG();

	T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

AVR_FAST_CORE_UINST_DECL(d5_swap)
{
	AVR_FAST_CORE_UINST_DEFN_rmwR1v(u_opcode, d);
	uint_fast8_t res = (vd >> 4) | (vd << 4);

	STATE("swap %s[%02x] = %02x\n", avr_regname(d), vd, res);

	AVR_FAST_CORE_RMW_STORE_R(d, res);

	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}
AVR_FAST_CORE_INST_DEFN(d5, swap)

AVR_FAST_CORE_UINST_DECL(d5_tst)
{
	AVR_FAST_CORE_UINST_DEFN_R1v(u_opcode, d);

	STATE("tst %s[%02x]\n", avr_regname(d), vd);

	_avr_fast_core_flags_znv0s(avr, vd);

	SREG();
	
	AVR_FAST_CORE_UINST_NEXT_PC_CYCLES(2, 1);
}

/*
 * Called when an invalid opcode is decoded
 */
static void _avr_fast_core_invalid_opcode(avr_t *avr)
{
#if CONFIG_SIMAVR_TRACE
	printf(FONT_RED "*** %04x: %-25s Invalid Opcode SP=%04x O=%04x \n" FONT_DEFAULT,
			avr->pc, avr_symbol_name_for_address(avr, avr->pc),
			_avr_fast_core_sp_get(avr), _avr_fast_core_flash_read16le(avr, avr->pc));
#else
	AVR_LOG(avr, LOG_ERROR, FONT_RED "CORE: *** %04x: Invalid Opcode SP=%04x O=%04x \n" FONT_DEFAULT,
			avr->pc, _avr_fast_core_sp_get(avr), _avr_fast_core_flash_read16le(avr, avr->pc));
	abort();
#endif
}

/*
 * Main opcode decoder
 * 
 * The decoder was written by following the datasheet in no particular order.
 * As I went along, I noticed "bit patterns" that could be used to factor opcodes
 * However, a lot of these only became apparent later on, so SOME instructions
 * (skip of bit set etc) are compact, and some could use some refactoring (the ALU
 * ones scream to be factored).
 * I assume that the decoder could easily be 2/3 of it's current size.
 * 
 * + It lacks the "extended" XMega jumps. 
 * + It also doesn't check whether the core it's
 *   emulating is supposed to have the fancy instructions, like multiply and such.
 * 
 * The number of cycles taken by instruction has been added, but might not be
 * entirely accurate.
 */
static int _avr_fast_core_inst_decode_one(avr_t *avr, int_fast32_t *count)
{
	avr_flashaddr_t new_pc = avr->pc + 2;
	T(avr_cycle_count_t __attribute__((__unused__)) startCycle = avr->cycle);
	
#if CONFIG_SIMAVR_TRACE
	/*
	 * this traces spurious reset or bad jumps
	 */
	if ((avr->pc == 0 && avr->cycle > 0) || avr->pc >= avr->codeend) {
		avr->trace = 1;
		STATE("RESET\n");
		CRASH();
	}
	avr->trace_data->touched[0] = avr->trace_data->touched[1] = avr->trace_data->touched[2] = 0;
#endif

	#ifdef AVR_FAST_CORE_TAIL_CALL
	/* !!!! DANGER !!!!
	 * tail call observed nesting funcitons before unnesting during itrace.
	 *	setting count to zero keeps decoding in order and
	 *	prevents errant tracing.
	 */
		*count = 0;
	#endif

	AVR_FAST_CORE_FLASH_OPCODE_DEFN(i_opcode, avr->pc);

	if(_AVR_FAST_CORE_DECODE_TRAP) {
		AVR_FAST_CORE_UFLASH_OPCODE_FETCH_DEFN(u_opcode, avr->pc);
		if(unlikely(u_opcode)) {
			xSTATE("opcode trap, not handled: 0x%08x [0x%04x]\n", u_opcode, i_opcode);
		}
	}

	switch (i_opcode & 0xf000) {
		case 0x0000: {
			switch (i_opcode) {
				case 0x0000: {	// NOP
					AVR_FAST_CORE_INST_CALL(x, nop);
				}	break;
				default: {
					switch (i_opcode & 0xfc00) {
						case 0x0400: {	// CPC compare with carry 0000 01rd dddd rrrr
							AVR_FAST_CORE_INST_CALL(d5r5, cpc);
						}	break;
						case 0x0c00: {	// ADD without carry 0000 11 rd dddd rrrr
							AVR_FAST_CORE_INST_CALL(d5r5, add);
						}	break;
						case 0x0800: {	// SBC subtract with carry 0000 10rd dddd rrrr
							AVR_FAST_CORE_INST_CALL(d5r5, sbc);
						}	break;
						default:
							switch (i_opcode & 0xff00) {
								case 0x0100: {	// MOVW – Copy Register Word 0000 0001 dddd rrrr
									AVR_FAST_CORE_INST_CALL(d4r4, movw);
								}	break;
								case 0x0200: {	// MULS – Multiply Signed 0000 0010 dddd rrrr
									AVR_FAST_CORE_INST_CALL(d16r16, muls);
								}	break;
								case 0x0300: {	// MUL Multiply 0000 0011 fddd frrr
									int8_t r = 16 + (i_opcode & 0x7);
									int8_t d = 16 + ((i_opcode >> 4) & 0x7);
									int16_t res = 0;
									uint8_t c = 0;
									T(const char * name = "";)
									switch (i_opcode & 0x88) {
										case 0x00: 	// MULSU – Multiply Signed Unsigned 0000 0011 0ddd 0rrr
											res = ((uint8_t)avr->data[r]) * ((int8_t)avr->data[d]);
											c = (res >> 15) & 1;
											T(name = "mulsu";)
											break;
										case 0x08: 	// FMUL Fractional Multiply Unsigned 0000 0011 0ddd 1rrr
											res = ((uint8_t)avr->data[r]) * ((uint8_t)avr->data[d]);
											c = (res >> 15) & 1;
											res <<= 1;
											T(name = "fmul";)
											break;
										case 0x80: 	// FMULS – Multiply Signed  0000 0011 1ddd 0rrr
											res = ((int8_t)avr->data[r]) * ((int8_t)avr->data[d]);
											c = (res >> 15) & 1;
											res <<= 1;
											T(name = "fmuls";)
											break;
										case 0x88: 	// FMULSU – Multiply Signed Unsigned 0000 0011 1ddd 1rrr
											res = ((uint8_t)avr->data[r]) * ((int8_t)avr->data[d]);
											c = (res >> 15) & 1;
											res <<= 1;
											T(name = "fmulsu";)
											break;
									}
									STATE("%s %s[%d], %s[%02x] = %d\n", name, avr_regname(d), ((int8_t)avr->data[d]), avr_regname(r), ((int8_t)avr->data[r]), res);
									_avr_fast_core_store_r16le(avr, 0, res);
									avr->sreg[S_C] = c;
									avr->sreg[S_Z] = res == 0;
									SREG();
									AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 2);
								}	break;
								default: _avr_fast_core_invalid_opcode(avr);
							}
					}
				}
			}
		}	break;

		case 0x1000: {
			switch (i_opcode & 0xfc00) {
				case 0x1800: {	// SUB without carry 0000 10 rd dddd rrrr
					AVR_FAST_CORE_INST_CALL(d5r5, sub);
				}	break;
				case 0x1000: {	// CPSE Compare, skip if equal 0000 00 rd dddd rrrr
					if(_avr_is_instruction_32_bits(avr, new_pc))
						AVR_FAST_CORE_INST_CALL(d5r5, 32_cpse);
					else
						AVR_FAST_CORE_INST_CALL(d5r5, 16_cpse);
				}	break;
				case 0x1400: {	// CP Compare 0000 01 rd dddd rrrr
					AVR_FAST_CORE_INST_CALL(d5r5, cp);
				}	break;
				case 0x1c00: {	// ADD with carry 0001 11 rd dddd rrrr
					AVR_FAST_CORE_INST_CALL(d5r5, adc);
				}	break;
				default: _avr_fast_core_invalid_opcode(avr);
			}
		}	break;

		case 0x2000: {
			switch (i_opcode & 0xfc00) {
				case 0x2000: {	// AND	0010 00rd dddd rrrr
					AVR_FAST_CORE_INST_CALL(d5r5, and);
				}	break;
				case 0x2400: {	// EOR	0010 01rd dddd rrrr
					AVR_FAST_CORE_INST_CALL(d5r5, eor);
				}	break;
				case 0x2800: {	// OR Logical OR	0010 10rd dddd rrrr
					AVR_FAST_CORE_INST_CALL(d5r5, or);
				}	break;
				case 0x2c00: {	// MOV	0010 11rd dddd rrrr
					AVR_FAST_CORE_INST_CALL(d5r5, mov);
				}	break;
				default: _avr_fast_core_invalid_opcode(avr);
			}
		}	break;

		case 0x3000: {	// CPI 0011 KKKK dddd KKKK
			AVR_FAST_CORE_INST_CALL(h4k8, cpi);
		}	break;

		case 0x4000: {	// SBCI Subtract Immediate With Carry 0101 10 kkkk dddd kkkk
			AVR_FAST_CORE_INST_CALL(h4k8, sbci);
		}	break;

		case 0x5000: {	// SUB Subtract Immediate 0101 10 kkkk dddd kkkk
			AVR_FAST_CORE_INST_CALL(h4k8, subi);
		}	break;

		case 0x6000: {	// ORI aka SBR	Logical AND with Immediate	0110 kkkk dddd kkkk
			AVR_FAST_CORE_INST_CALL(h4k8, ori);
		}	break;

		case 0x7000: {	// ANDI	Logical AND with Immediate	0111 kkkk dddd kkkk
			AVR_FAST_CORE_INST_CALL(h4k8, andi);
		}	break;

		case 0xa000:
		case 0x8000: {
			switch (i_opcode & 0xd200) {
				case 0x8000:
				case 0xa000: {	// LD (LDD) – Load Indirect using Y/Z 11q0 qq0r rrrr 0qqq
						AVR_FAST_CORE_INST_CALL(d5rYZq6, ldd);
				}	break;
				case 0x8200:
				case 0xa200: {	// ST (STD) – Store Indirect using Y/Z 10q0 qqsr rrrr iqqq
						AVR_FAST_CORE_INST_CALL(d5rYZq6, std);
				}	break;
				default: _avr_fast_core_invalid_opcode(avr);
			}
		}	break;

		case 0x9000: {
			switch (i_opcode) {
				case 0x9588: { // SLEEP
					AVR_FAST_CORE_INST_CALL(x, sleep);
				}	break;
				case 0x9598: { // BREAK
					STATE("break\n");
					if (avr->gdb) {
						// if gdb is on, we break here as in here
						// and we do so until gdb restores the instruction
						// that was here before
						avr->state = cpu_StepDone;
						AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 3);
					}
				}	break;
				case 0x95a8: { // WDR
					STATE("wdr\n");
					avr_ioctl(avr, AVR_IOCTL_WATCHDOG_RESET, 0);
					AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1);
				}	break;
				case 0x95e8: { // SPM
					STATE("spm\n");
					avr_ioctl(avr, AVR_IOCTL_FLASH_SPM, 0);
					AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1);
					*count = 0;
				}	break;
				case 0x9409: {   // IJMP Indirect jump 					1001 0100 0000 1001
					AVR_FAST_CORE_INST_CALL(x, ijmp);
				}	break;
				case 0x9419: {  // EIJMP Indirect jump 					1001 0100 0001 1001   bit 4 is "indirect"
					if (!avr->eind) {
						_avr_fast_core_invalid_opcode(avr);
						AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1);
					} else
						AVR_FAST_CORE_INST_CALL(x, eind_eijmp);
				}	break;
				case 0x9509: {  // ICALL Indirect Call to Subroutine		1001 0101 0000 1001
					AVR_FAST_CORE_INST_CALL(x, icall);
				}	break;
				case 0x9519: { // EICALL Indirect Call to Subroutine	1001 0101 0001 1001   bit 8 is "push pc"
					if (!avr->eind) {
						_avr_fast_core_invalid_opcode(avr);
						AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 1);
					} else
						AVR_FAST_CORE_INST_CALL(x, eind_eicall);
				}	break;
				case 0x9518: {	// RETI
					if(avr->eind)
						AVR_FAST_CORE_INST_CALL(x, eind_reti);
					else	
						AVR_FAST_CORE_INST_CALL(x, reti);
				}	break;
				case 0x9508: {	// RET
					if(avr->eind)
						AVR_FAST_CORE_INST_CALL(x, eind_ret);
					else	
						AVR_FAST_CORE_INST_CALL(x, ret);
				}	break;
				case 0x95c8: {	// LPM Load Program Memory R0 <- (Z) 1001 0101 1100 1000
					i_opcode = 0x9004;
					AVR_FAST_CORE_INST_CALL(d5, lpm_z);
				}	break;
				case 0x9408:case 0x9418:case 0x9428:case 0x9438:case 0x9448:case 0x9458:case 0x9468:
				case 0x9478: // BSET 1001 0100 0ddd 1000
				{	AVR_FAST_CORE_INST_CALL(b3, bset);
				}	break;
				case 0x9488:case 0x9498:case 0x94a8:case 0x94b8:case 0x94c8:case 0x94d8:case 0x94e8:
				case 0x94f8:	// bit 7 is 'clear vs set'
				{	// BCLR 1001 0100 1ddd 1000
					AVR_FAST_CORE_INST_CALL(b3, bclr);
				}	break;
				default:  {
					switch (i_opcode & 0xfe0f) {
						case 0x9000: {	// LDS Load Direct from Data Space, 32 bits
							AVR_FAST_CORE_INST_CALL(d5x16, lds);
						}	break;
						case 0x9005: {	// LPM Load Program Memory 1001 000d dddd 01oo
							AVR_FAST_CORE_INST_CALL(d5, lpm_z_post_inc);
						}	break;
						case 0x9004: {	// LPM Load Program Memory 1001 000d dddd 01oo
							AVR_FAST_CORE_INST_CALL(d5, lpm_z);
						}	break;
						case 0x9006:
						case 0x9007: {	// ELPM Extended Load Program Memory 1001 000d dddd 01oo
							if (!avr->rampz)
								_avr_fast_core_invalid_opcode(avr);
							uint32_t z = _avr_fast_core_fetch_r16le(avr, R_ZL) | (avr->data[avr->rampz] << 16);

							uint_fast8_t r = (i_opcode >> 4) & 0x1f;
							int op = i_opcode & 3;
							STATE("elpm %s, (Z[%02x:%04x]%s)\n", avr_regname(r), z >> 16, z&0xffff, i_opcode?"+":"");
							_avr_fast_core_store_r(avr, r, avr->flash[z]);
							if (op == 3) {
								z++;
								_avr_fast_core_store_r(avr, avr->rampz, z >> 16);
								_avr_fast_core_store_r16le(avr, R_ZL, (z&0xffff));
							}
							AVR_FAST_CORE_UINST_STEP_PC_CYCLES(2, 3); // 3 cycles
						}	break;
						/*
						 * Load store instructions
						 *
						 * 1001 00sr rrrr iioo
						 * s = 0 = load, 1 = store
						 * ii = 16 bits register index, 11 = Z, 10 = Y, 00 = X
						 * oo = 1) post increment, 2) pre-decrement
						 */
						case 0x900c: {	// LD Load Indirect from Data using X 1001 000r rrrr 11oo
							AVR_FAST_CORE_INST_CALL(d5rXYZ, ld_no_op);
						}	break;
						case 0x920c: {	// ST Store Indirect Data Space 1001 001r rrrr iioo
							AVR_FAST_CORE_INST_CALL(d5rXYZ, st_no_op);
						}	break;
						case 0x9001:
						case 0x9009:
						case 0x900d: {	// LD Load Indirect from Data  1001 00sr rrrr iioo
							AVR_FAST_CORE_INST_CALL(d5rXYZ, ld_post_inc);
						}	break;
						case 0x9002:
						case 0x900a:
						case 0x900e: {	// LD Load Indirect from Data 1001 00sr rrrr iioo
							AVR_FAST_CORE_INST_CALL(d5rXYZ, ld_pre_dec);
						}	break;
						case 0x9201:
						case 0x9209:
						case 0x920d:  {	// ST Store Indirect Data space 1001 000r rrrr iioo
							AVR_FAST_CORE_INST_CALL(d5rXYZ, st_post_inc);
						}	break;
						case 0x9202:
						case 0x920a:
						case 0x920e: {	// ST Store Indirect Data Space 1001 001r rrrr iioo
							AVR_FAST_CORE_INST_CALL(d5rXYZ, st_pre_dec);
						}	break;
						case 0x9200: {	// STS ! Store Direct to Data Space, 32 bits
							AVR_FAST_CORE_INST_CALL(d5x16, sts);
						}	break;
						case 0x900f: {	// POP 1001 000d dddd 1111
							AVR_FAST_CORE_INST_CALL(d5, pop);
						}	break;
						case 0x920f: {	// PUSH 1001 001d dddd 1111
							AVR_FAST_CORE_INST_CALL(d5, push);
						}	break;
						case 0x9400: {	// COM – One’s Complement
							AVR_FAST_CORE_INST_CALL(d5, com);
						}	break;
						case 0x9401: {	// NEG – Two’s Complement
							AVR_FAST_CORE_INST_CALL(d5, neg);
						}	break;
						case 0x9402: {	// SWAP – Swap Nibbles
							AVR_FAST_CORE_INST_CALL(d5, swap);
						}	break;
						case 0x9403: {	// INC – Increment
							AVR_FAST_CORE_INST_CALL(d5, inc);
						}	break;
						case 0x9405: {	// ASR – Arithmetic Shift Right 1001 010d dddd 0101
							AVR_FAST_CORE_INST_CALL(d5, asr);
						}	break;
						case 0x9406: {	// LSR 1001 010d dddd 0110
							AVR_FAST_CORE_INST_CALL(d5, lsr);
						}	break;
						case 0x9407: {	// ROR 1001 010d dddd 0111
							AVR_FAST_CORE_INST_CALL(d5, ror);
						}	break;
						case 0x940a: {	// DEC – Decrement
							AVR_FAST_CORE_INST_CALL(d5, dec);
						}	break;
						case 0x940c:
						case 0x940d: {	// JMP Long Call to sub, 32 bits
							AVR_FAST_CORE_INST_CALL(x22, jmp);
						}	break;
						case 0x940e:
						case 0x940f: {	// CALL Long Call to sub, 32 bits
							if(avr->eind)
								AVR_FAST_CORE_INST_CALL(x22, eind_call);
							else	
								AVR_FAST_CORE_INST_CALL(x22, call);
						}	break;

						default: {
							switch (i_opcode & 0xff00) {
								case 0x9600: {	// ADIW - Add Immediate to Word 1001 0110 KKdd KKKK
									AVR_FAST_CORE_INST_CALL(p2k6, adiw);
								}	break;
								case 0x9700: {	// SBIW - Subtract Immediate from Word 1001 0111 KKdd KKKK
									AVR_FAST_CORE_INST_CALL(p2k6, sbiw);
								}	break;
								case 0x9800: {	// CBI - Clear Bit in I/O Register 1001 1000 AAAA Abbb
									AVR_FAST_CORE_INST_CALL(a5m8, cbi);
								}	break;
								case 0x9900: {	// SBIC - Skip if Bit in I/O Register is Cleared 1001 0111 AAAA Abbb
									if(_avr_is_instruction_32_bits(avr, new_pc))
										AVR_FAST_CORE_INST_CALL(a5m8, 32_sbic);
									else
										AVR_FAST_CORE_INST_CALL(a5m8, 16_sbic);
								}	break;
								case 0x9a00: {	// SBI - Set Bit in I/O Register 1001 1000 AAAA Abbb
									AVR_FAST_CORE_INST_CALL(a5m8, sbi);
								}	break;
								case 0x9b00: {	// SBIS - Skip if Bit in I/O Register is Set 1001 1011 AAAA Abbb
									if(_avr_is_instruction_32_bits(avr, new_pc))
										AVR_FAST_CORE_INST_CALL(a5m8, 32_sbis);
									else
										AVR_FAST_CORE_INST_CALL(a5m8, 16_sbis);
								}	break;
								default:
									switch (i_opcode & 0xfc00) {
										case 0x9c00: {	// MUL - Multiply Unsigned 1001 11rd dddd rrrr
											AVR_FAST_CORE_INST_CALL(d5r5, mul);
										}	break;
										default: _avr_fast_core_invalid_opcode(avr);
									}
							}
						}	break;
					}
				}	break;
			}
		}	break;
		case 0xb000: {
			switch (i_opcode & 0xf800) {
				case 0xb800: {	// OUT A,Rr 1011 1AAr rrrr AAAA
					AVR_FAST_CORE_INST_CALL(d5a6, out);
				}	break;
				case 0xb000: {	// IN Rd,A 1011 0AAr rrrr AAAA
					AVR_FAST_CORE_INST_CALL(d5a6, in);
				}	break;
				default: _avr_fast_core_invalid_opcode(avr);
			}
		}	break;
		case 0xc000: {	// RJMP 1100 kkkk kkkk kkkk
			AVR_FAST_CORE_INST_CALL(o12, rjmp);
		}	break;
		case 0xd000: {
			// RCALL 1100 kkkk kkkk kkkk
			if(avr->eind)
				AVR_FAST_CORE_INST_CALL(o12, eind_rcall);
			else	
				AVR_FAST_CORE_INST_CALL(o12, rcall);
		}	break;
		case 0xe000: {	// LDI Rd, K 1110 KKKK RRRR KKKK -- aka SER (LDI r, 0xff)
			AVR_FAST_CORE_INST_CALL(h4k8, ldi);
		}	break;
		case 0xf000: {
			switch (i_opcode & 0xfe00) {
				case 0xf000:
				case 0xf200: {
					AVR_FAST_CORE_INST_CALL(b3o7, brxs);
				}	break;
				case 0xf400:
				case 0xf600: {
					AVR_FAST_CORE_INST_CALL(b3o7, brxc);
				}	break;
				case 0xf800:
				case 0xf900: {	// BLD – Bit Store from T into a Bit in Register 1111 100r rrrr 0bbb
					AVR_FAST_CORE_INST_CALL(d5m8, bld);
				}	break;
				case 0xfa00:
				case 0xfb00:{	// BST – Bit Store into T from bit in Register 1111 100r rrrr 0bbb
					AVR_FAST_CORE_INST_CALL(d5b3, bst);
				}	break;
				case 0xfc00: {	// SBRC – Skip if Bit in Register is Clear 1111 11sr rrrr 0bbb
					if(_avr_is_instruction_32_bits(avr, new_pc))
						AVR_FAST_CORE_INST_CALL(d5m8, 32_sbrc);
					else
						AVR_FAST_CORE_INST_CALL(d5m8, 16_sbrc);
				}	break;
				case 0xfe00: {	// SBRS – Skip if Bit in Register is Set 1111 11sr rrrr 0bbb
					if(_avr_is_instruction_32_bits(avr, new_pc))
						AVR_FAST_CORE_INST_CALL(d5m8, 32_sbrs);
					else
						AVR_FAST_CORE_INST_CALL(d5m8, 16_sbrs);
				}	break;
				default: _avr_fast_core_invalid_opcode(avr);
			}
		}	break;
		default: _avr_fast_core_invalid_opcode(avr);
	}
	return(*count);
}

AVR_FAST_CORE_UINST_DECL(avr_decode_one)
{
	_avr_fast_core_inst_decode_one(avr, count);
}

#define STR(name) #name
#undef AVR_FAST_CORE_UINST_ESAC_DEFN
#define AVR_FAST_CORE_UINST_ESAC_DEFN(name) STR(_avr_fast_core_uinst_##name),
static const char __attribute__((__unused__)) *_avr_fast_core_uinst_op_names[256] = {
	AVR_FAST_CORE_UINST_ESAC_TABLE_DEFN
};

#undef AVR_FAST_CORE_UINST_ESAC_DEFN
#define AVR_FAST_CORE_UINST_ESAC_DEFN(name) _avr_fast_core_uinst_##name,
static pfnInst_p _avr_fast_core_uinst_op_table[256] = {
	AVR_FAST_CORE_UINST_ESAC_TABLE_DEFN
};

static int _avr_fast_core_run_one(avr_t* avr, int_fast32_t *count)
{
	AVR_FAST_CORE_UFLASH_OPCODE_FETCH_DEFN(u_opcode, avr->pc);
	AVR_FAST_CORE_UINST_DEFN_R0(u_opcode, u_opcode_op);
	AVR_FAST_CORE_PROFILER_PROFILE_ISEQ(u_opcode_op);

	pfnInst_p pfn = _avr_fast_core_uinst_op_table[u_opcode_op];

	AVR_FAST_CORE_PROFILER_PROFILE(uinst[u_opcode_op], AVR_FAST_CORE_PFN_UINST_CALL(u_opcode));

	return(*count);
}

void avr_fast_core_run_many(avr_t* avr)
{
	AVR_FAST_CORE_PROFILER_PROFILE_START(core_loop);
	int_fast32_t count = _avr_fast_core_cycle_timer_process(avr, 0);
	
	if(0 == avr->sreg[S_I]) {
/* no interrupt free run */
		if(likely(cpu_Running == avr->state)) {
			while(0 < _avr_fast_core_run_one(avr, &count))
				AVR_FAST_CORE_PROFILER_PROFILE_IPS();

			if (avr->sreg[S_I] && !avr->i_shadow) {
				avr->interrupts.pending_wait++;
				goto interrupts_enabled;
			}

interrupts_disabled:
			_avr_fast_core_cycle_timer_process(avr, 0);
		} else if(cpu_Sleeping == avr->state) {
				if (avr->log)
					AVR_LOG(avr, LOG_TRACE, "simavr: sleeping with interrupts off, quitting gracefully\n");
				avr->state = cpu_Done;
				return;
		}
	} else {
/* slow(er) run with interrupt check */
		if(likely(cpu_Running == avr->state)) {
			while(0 < _avr_fast_core_run_one(avr, &count))
				AVR_FAST_CORE_PROFILER_PROFILE_IPS();

			if(!avr->sreg[S_I] && avr->i_shadow) {
				goto interrupts_disabled;
			}
		}
		
interrupts_enabled:
		avr->i_shadow = avr->sreg[S_I];
			
		_avr_fast_core_cycle_timer_process(avr, 0);
		
		if(cpu_Sleeping == avr->state) {
			avr->sleep(avr, count);
			avr->cycle += (1 + count);
		}

		_avr_fast_core_service_interrupts(avr);
	}
	AVR_FAST_CORE_PROFILER_PROFILE_STOP(core_loop);
}

void avr_fast_core_init(avr_t* avr)
{
	/* avr program memory is 16 bits wide, byte addressed. */
	uint32_t flashsize = avr->flashend + 1; // 2

	uint32_t uflashsize = AVR_CORE_FLASH_UFLASH_SIZE_SHIFT(flashsize);

	#if !defined(AVR_FAST_CORE_GLOBAL_FLASH_ACCESS) && defined(AVR_FAST_CORE_IO_DISPATCH_TABLES)
		uflashsize += sizeof(_avr_fast_core_data_t);
	#endif

	avr->flash = realloc(avr->flash, flashsize + uflashsize);
	assert(0 != avr->flash);
	
	memset(&avr->flash[flashsize], 0, uflashsize);

	AVR_FAST_CORE_PROFILER_INIT(_avr_fast_core_uinst_op_names);

	AVR_FAST_CORE_PROFILER_PROFILE_ISEQ_FLUSH(avr_decode_one);
	AVR_FAST_CORE_PROFILER_PROFILE_ISEQ_FLUSH(b3o7_brxc);
	AVR_FAST_CORE_PROFILER_PROFILE_ISEQ_FLUSH(b3o7_brxs);
	AVR_FAST_CORE_PROFILER_PROFILE_ISEQ_FLUSH(x22_call);
	AVR_FAST_CORE_PROFILER_PROFILE_ISEQ_FLUSH(o12_rjmp);
	AVR_FAST_CORE_PROFILER_PROFILE_ISEQ_FLUSH(d5m8_16_sbrc);
	AVR_FAST_CORE_PROFILER_PROFILE_ISEQ_FLUSH(d5m8_32_sbrc);
	AVR_FAST_CORE_PROFILER_PROFILE_ISEQ_FLUSH(d5m8_16_sbrs);
	AVR_FAST_CORE_PROFILER_PROFILE_ISEQ_FLUSH(d5m8_32_sbrs);

	#ifdef AVR_FAST_CORE_GLOBAL_FLASH_ACCESS
		_avr_fast_core_uflash = AVR_CORE_FLASH_FAST_CORE_UFLASH;
	#endif
}

avr_flashaddr_t avr_fast_core_run_one(avr_t *avr)
{
	static int inited = 0;
	int count = 1;
	
	if(!inited) {
		avr_fast_core_init(avr);
		inited = 1;
	}

	avr_flashaddr_t old_pc = avr->pc;
	_avr_fast_core_run_one(avr, &count);
	avr_flashaddr_t new_pc = avr->pc;
	avr->pc = old_pc;
	
	return(new_pc);
}

#endif /* #ifndef __SIM_FAST_CORE_C */

