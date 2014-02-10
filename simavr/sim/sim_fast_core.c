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

#define CONFIG_SIMAVR_FAST_CORE

/* FAST_CORE_UINST_PROFILING
	tracks dtime and count of each instruction executed
	 enable/disable via 
	 	#define FAST_CORE_UINST_PROFILING
	 in sim_fast_core_profiling.h */
#include "sim_fast_core_profiler.h"

#ifdef FAST_CORE_UINST_PROFILING
core_profile_t core_profile;
#endif

/* FAST_CORE_USE_GLOBAL_FLASH_ACCESS
	uses globals for uflash and io_table access, will not work for multiple avr cores */
/* FAST_CORE_COMBINING
	common instruction sequences are combined as well as allowing 16 bit access tricks. */
/* FAST_CORE_COMMON_DATA
	puts avr->data local data for multiple register references */
/* FAST_CORE_BRANCH_HINTS
	via likely() and unlikely() macros provide the compiler (and possibly passed 
	onto the processor) hints to help reduce pipeline stalls due to 
	misspredicted branches. USE WITH CARE! :) */
/* FAST_CORE_SKIP_SHIFT
	use shifts vs comparisons and branching where shifting is less expensive
	then branching. Some processors have specialized instructions to handle
	such cases and may be faster disabled. */
/* FAST_CORE_32_SKIP_SHIFT
	long instruction skip is not as strait forward as a simple skip...
	overall results may vary. */
/* FAST_CORE_READ_MODIFY_WRITE
	reduces redundancy inherent in register access...  and cuts back on 
	some unecessary checks. */
/* FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	for processors with fast multiply, helps reduce branches in comparisons 
	some processors may have specialized instructions making this slower */
/* FAST_CORE_SLEEP_PREPROCESS_TIMERS_AND_INTERRUPTS
	fast forwards avr->cycle then calls timer and interrupt service routines in sleep */
/* FAST_CORE_IO_DISPTACH_TABLES
	initial io access will be trapped, later access will happen through dispatch tables. */
	
//#define FAST_CORE_USE_GLOBAL_FLASH_ACCESS
#define FAST_CORE_COMBINING
#define FAST_CORE_COMMON_DATA
#define FAST_CORE_BRANCH_HINTS
#define FAST_CORE_SKIP_SHIFT
#define FAST_CORE_32_SKIP_SHIFT
#define FAST_CORE_READ_MODIFY_WRITE
#define FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
#define FAST_CORE_SLEEP_PREPROCESS_TIMERS_AND_INTERRUPTS
#define FAST_CORE_IO_DISPTACH_TABLES

/*
	NOTE!!!  Concerning FAST_CORE_USE_CPI_BRXX and FAST_CORE_CPI_BRXX_CODE...
	
	FAST_CORE_USE_CPI_BRXX
		controls the actual usage of instruction sequences.
	FAST_CORE_CPI_BRXX_CODE
		determines if the actual code is sent through the compiler
			or left out during the build.
	FAST_CORE_CPI_BRXX_COMMON_BRANCH_CODE
		uses a central funciton to do the actual branch.

	WHY??:
		Sometimes using the code does not always result in improved performance..
			even where theory would dictate otherwise.
		Unforunately taking the code bodies out duing test cases has also
			shown to negatively impact performance.
		Overall the results have a lot to do with code size, cache sizes,
			and code arrangement...  Wether it be function bodies or
			branches/jumps.
*/

#define FAST_CORE_USE_CPI_BRXX
#define FAST_CORE_CPI_BRXX_CODE
#define FAST_CORE_CPI_BRXX_COMMON_BRANCH_CODE

#ifndef FAST_CORE_UINST_PROFILING
/* FAST_CORE_TAIL_CALL
	build using tail calls, not all compilers may support the features
	necessary and/or may be compiler dependant. */
#define FAST_CORE_TAIL_CALL
#endif

/* FAST_CORE_DECODE_TRAP
	specific trap to catch missing instruction handlers */
/* FAST_CORE_AGGRESSIVE_CHECKS
	bounds checking at multiple points. */

//#define FAST_CORE_DECODE_TRAP
//#define FAST_CORE_AGGRESSIVE_CHECKS

#ifndef CONFIG_SIMAVR_TRACE
/* FAST_CORE_LOCAL_TRACE
	set this to bypass mucking with the makefiles and possibly needing to 
	rebuild the entire project... allows tracing specific to the fast core. */
/* CORE_FAST_CORE_DIFF_TRACE
	Some trace statements are slightly different than the original core...
	also, some operations have bug fixes included not in original core.
	defining CORE_FAST_CORE_DIFF_TRACE returns operation close to original 
	core to make diffing trace output from cores easier for debugging. */
/* FAST_CORE_ITRACE
	helps to track flash -> uflash instruction opcode as instructions are 
	translated to uoperations...  after which quiets down as instructions 
	are picked up by the faster core loop. */
/* FAST_CORE_STACK_TRACE
	adds more verbose detail to stack operations */

//#define FAST_CORE_LOCAL_TRACE
//#define CORE_FAST_CORE_DIFF_TRACE
#ifndef CORE_FAST_CORE_DIFF_TRACE
//#define FAST_CORE_ITRACE
//#define FAST_CORE_STACK_TRACE
#endif
#else
/* do not touch these here...  set above. */
#define FAST_CORE_STACK_TRACE
#define CORE_FAST_CORE_DIFF_TRACE
#endif

/* ****
	>>>>	END OF OPTION FLAGS SECTION
**** */

// 1,2,4,8,16
#define kUFlashAddrShift 2
// 4,8,16,32,64
#define kUFlashSizeShift (kUFlashAddrShift << 1)

#ifdef FAST_CORE_IO_DISPTACH_TABLES
typedef uint_fast8_t (*_fast_core_io_read_fn)(avr_t * avr, int_fast32_t * count, uint_fast16_t addr);
typedef void (*_fast_core_io_write_fn)(avr_t * avr, int_fast32_t * count, uint_fast16_t addr, uint_fast8_t v);
#define SET_IO_READ_FN_DO_RET(addr, name) FAST_CORE_IO_READ_FN(addr) = name; return(name(avr, count, addr))
#define SET_IO_WRITE_FN_DO_RET(addr, name) FAST_CORE_IO_WRITE_FN(addr) = name; return(name(avr, count, addr, v))
#define FAST_CORE_IO_READ_FN_DO(avr, count, addr) FAST_CORE_IO_READ_FN(addr)(avr, count, addr)
#define FAST_CORE_IO_WRITE_FN_DO(avr, count, addr, v) FAST_CORE_IO_WRITE_FN(addr)(avr, count, addr, v)
#define IO_INLINE
#else
#define SET_IO_READ_FN_DO_RET(addr, name) return(name(avr, count, addr))
#define SET_IO_WRITE_FN_DO_RET(addr, name) return(name(avr, count, addr, v))
#define FAST_CORE_IO_READ_FN_DO(avr, count, addr)
#define FAST_CORE_IO_WRITE_FN_DO(avr, count, addr, v)
#define IO_INLINE inline
#endif

#define AVR_FLASH_UFLASH ((uint32_t*)&((uint8_t *)avr->flash)[avr->flashend + 1])

#ifdef FAST_CORE_USE_GLOBAL_FLASH_ACCESS
uint32_t *_uflash;
#define UFLASH_ADDR_AT(addr) _uflash[addr << kUFlashAddrShift]
#ifdef FAST_CORE_IO_DISPTACH_TABLES
_fast_core_io_read_fn _avr_io_read_fn[MAX_IOs];
_fast_core_io_write_fn _avr_io_write_fn[MAX_IOs];
#define FAST_CORE_IO_READ_FN(addr) _avr_io_read_fn[addr]
#define FAST_CORE_IO_WRITE_FN(addr) _avr_io_write_fn[addr]
#endif
#else
#ifdef FAST_CORE_IO_DISPTACH_TABLES
typedef struct fast_core_data_t {
	uint32_t *		_uflash;
	_fast_core_io_read_fn	_avr_io_read_fn[MAX_IOs];
	_fast_core_io_write_fn	_avr_io_write_fn[MAX_IOs];
}fast_core_data_t, *fast_core_data_p;
#define AVR_FLASH_CORE_DATA ((fast_core_data_p)&((uint8_t *)avr->flash)[avr->flashend + 1])
#define UFLASH_ADDR_AT(addr) AVR_FLASH_CORE_DATA->_uflash[addr << kUFlashAddrShift]
#define FAST_CORE_IO_READ_FN(addr) AVR_FLASH_CORE_DATA->_avr_io_read_fn[addr]
#define FAST_CORE_IO_WRITE_FN(addr) AVR_FLASH_CORE_DATA->_avr_io_write_fn[addr]
#else
#define UFLASH_ADDR_AT(addr) AVR_FLASH_UFLASH[addr << kUFlashAddrShift]
#endif
#endif

#ifdef FAST_CORE_COMBINING
const int _FAST_CORE_COMBINING = 1;
#else
const int _FAST_CORE_COMBINING = 0;
#endif

#define CYCLES(x) { if(1==(x)) { avr->cycle++; (*count)--; } else { avr->cycle += (x); (*count) -= (x); }}

#define _avr_sp_get _avr_fast_core_sp_get
#define _avr_sp_set _avr_fast_core_sp_set

#ifdef FAST_CORE_BRANCH_HINTS
#define likely(x) __builtin_expect(!!(x),1)
#define unlikely(x) __builtin_expect(!!(x),0)
#else
#define likely(x) x
#define unlikely(x) x
#endif

#ifdef FAST_CORE_UINST_PROFILING
void _avr_cycle_timer_process(avr_t * avr) {
	avr_cycle_timer_pool_t * pool = &avr->cycle_timers;
	uint_fast8_t pool_count = pool->count;
	
	if(pool_count) {
		avr_cycle_timer_slot_t  cycle_timer = pool->timer[pool_count-1];
		avr_cycle_count_t when = cycle_timer.when;
		if (when < avr->cycle) {
			PROFILE(timer, avr_cycle_timer_process(avr));
		}
	}
}

void _avr_service_interrupts(avr_t * avr) {
	if(avr->sreg[S_I]) {
		if(avr_has_pending_interrupts(avr)) {
			PROFILE(isr, avr_service_interrupts(avr));
		}
	}
}
#else
#define _avr_cycle_timer_process(avr) avr_cycle_timer_process(avr)
#define _avr_service_interrupts(avr) avr_service_interrupts(avr)
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

#ifdef FAST_CORE_STACK_TRACE
#ifdef CORE_FAST_CORE_DIFF_TRACE
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
extern void avr_dump_state(avr_t * avr);
#endif

static inline uint_fast8_t _avr_data_read(avr_t* avr, uint_fast16_t addr) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(addr > avr->ramend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif
	return(avr->data[addr]);
}

static inline void _avr_data_write(avr_t* avr, uint_fast16_t addr, uint_fast8_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(addr > avr->ramend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif
	avr->data[addr]=data;
}

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint_fast16_t _avr_bswap16le(uint_fast16_t v) {
	return(v);
}

static inline uint_fast16_t _avr_bswap16be(uint_fast16_t v) {
	return(((v & 0xff00) >> 8) | ((v & 0x00ff) << 8));
}
#else
static inline uint_fast16_t _avr_bswap16le(uint_fast16_t v) {
	return(((v & 0xff00) >> 8) | ((v & 0x00ff) << 8));
}

static inline uint_fast16_t _avr_bswap16be(uint_fast16_t v) {
	return(v);
}
#endif

static inline uint_fast16_t _avr_fetch16(void* p, uint_fast16_t addr) {
	return(*((uint16_t *)&((uint8_t *)p)[addr]));
}

static inline void _avr_store16(void*p, uint_fast16_t addr, uint_fast16_t data) {
	*((uint16_t *)&((uint8_t *)p)[addr])=data;
}

static inline void _avr_data_mov(avr_t* avr, uint_fast16_t dst, uint_fast16_t src) {
	avr->data[dst] = avr->data[src];
}

static inline void _avr_data_mov16(avr_t* avr, uint_fast16_t dst, uint_fast16_t src) {
	uint8_t* data = avr->data;

	uint16_t* ptr_src = (uint16_t *)&data[src];
	uint16_t* ptr_dst = (uint16_t *)&data[dst];

	*ptr_dst = *ptr_src;
}

static inline uint_fast16_t _avr_data_read16(avr_t* avr, uint_fast16_t addr) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif
	return(_avr_fetch16(avr->data, addr));
}

static inline void _avr_data_write16(avr_t* avr, uint_fast16_t addr, uint_fast16_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif
	_avr_store16(avr->data, addr, data);
}

static inline uint_fast16_t _avr_data_read16be(avr_t* avr, uint_fast16_t addr) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif
	return(_avr_bswap16be(_avr_fetch16(avr->data, addr)));
}

static inline uint_fast16_t _avr_data_read16le(avr_t* avr, uint_fast16_t addr) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif
	return(_avr_bswap16le(_avr_fetch16(avr->data, addr)));
}

static inline void _avr_data_write16be(avr_t* avr, uint_fast16_t addr, uint_fast16_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif
	_avr_store16(avr->data, addr, _avr_bswap16be(data));
}

static inline void _avr_data_write16le(avr_t* avr, uint_fast16_t addr, uint_fast16_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif
	_avr_store16(avr->data, addr, _avr_bswap16le(data));
}

/*
 * Stack pointer access
 */
static inline uint_fast16_t _avr_sp_get(avr_t * avr)
{
	return(_avr_data_read16le(avr, R_SPL));
}

static inline void _avr_sp_set(avr_t * avr, uint_fast16_t sp)
{
	_avr_data_write16le(avr, R_SPL, sp);
}

/*
 * Register access funcitons
 */
static inline uint_fast8_t _avr_get_r(avr_t* avr, uint_fast8_t reg) {
	return(_avr_data_read(avr, reg));
}

static inline void _avr_mov_r(avr_t* avr, uint_fast8_t dst, uint_fast8_t src) {
	_avr_data_mov(avr, dst, src);
}
static inline void _avr_set_r(avr_t* avr, uint_fast8_t reg, uint_fast8_t v) {
	_avr_data_write(avr, reg, v);
}

static inline uint_fast16_t _avr_get_r16(avr_t* avr, uint_fast8_t addr) {
	return(_avr_data_read16(avr, addr));
}

static inline void _avr_mov_r16(avr_t* avr, uint_fast8_t dst, uint_fast8_t src) {
	_avr_data_mov16(avr, dst, src);
}

static inline void _avr_set_r16(avr_t* avr, uint_fast8_t addr, uint_fast16_t data) {
	_avr_data_write16(avr, addr, data);
}

static inline uint_fast16_t _avr_get_r16le(avr_t* avr, uint_fast8_t addr) {
	return(_avr_data_read16le(avr, addr));
}

static inline void _avr_set_r16le(avr_t* avr, uint_fast8_t addr, uint_fast16_t data) {
	_avr_data_write16le(avr, addr, data);
}


/*
 * Flash accessors
 */
static inline uint_fast16_t _avr_flash_read16le(avr_t* avr, uint_fast16_t addr) {
	return(_avr_bswap16le(_avr_fetch16(avr->flash, addr)));
}

static inline uint_fast16_t _avr_flash_read16be(avr_t* avr, uint_fast16_t addr) {
	return(_avr_bswap16be(_avr_fetch16(avr->flash, addr)));
}

/*
 * Add a "jump" address to the jump trace buffer
 */
#if CONFIG_SIMAVR_TRACE
#define TRACE_JUMP()\
	avr->trace_data->old[avr->trace_data->old_pci].pc = avr->pc;\
	avr->trace_data->old[avr->trace_data->old_pci].sp = _avr_sp_get(avr);\
	avr->trace_data->old_pci = (avr->trace_data->old_pci + 1) & (OLD_PC_SIZE-1);\

#if AVR_STACK_WATCH
#define STACK_FRAME_PUSH()\
	avr->trace_data->stack_frame[avr->trace_data->stack_frame_index].pc = avr->pc;\
	avr->trace_data->stack_frame[avr->trace_data->stack_frame_index].sp = _avr_sp_get(avr);\
	avr->trace_data->stack_frame_index++; 
#define STACK_FRAME_POP()\
	if (avr->trace_data->stack_frame_index > 0) \
		avr->trace_data->stack_frame_index--;
#else
#define STACK_FRAME_PUSH()
#define STACK_FRAME_POP()
#endif
#else /* CONFIG_SIMAVR_TRACE */

#define TRACE_JUMP()
#define STACK_FRAME_PUSH()
#define STACK_FRAME_POP()

#endif

/*
 * Handle "touching" registers, marking them changed.
 * This is used only for debugging purposes to be able to
 * print the effects of each instructions on registers
 */
#if CONFIG_SIMAVR_TRACE

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
#ifdef FAST_CORE_LOCAL_TRACE
#define T(w) w
#define NO_T(w)
#define REG_TOUCH(a, r)
#define STATE(_f, args...) xSTATE(_f, ## args)
#define SREG() xSREG()
#else
#define REG_TOUCH(a, r)
#ifdef FAST_CORE_ITRACE
extern inline uint32_t uFlashRead(avr_t* avr, avr_flashaddr_t addr);
#define T(w) w
#define NO_T(w)
#define STATE(_f, args...) { if(0==uFlashRead(avr, avr->pc)) xSTATE(_f, ## args) }
#else
#define T(w)
#define NO_T(w) w
#define STATE(_f, args...)
#endif
#define SREG()
#endif
#endif

static inline void SEI(avr_t * avr) {
	if(!avr->i_shadow)
		avr->interrupts.pending_wait++;
	avr->i_shadow = avr->sreg[S_I];
}

static inline void CLI(avr_t * avr) {
	avr->interrupts.pending_wait = 0;
	avr->i_shadow = avr->sreg[S_I];
}

static IO_INLINE void _avr_reg_io_write_sreg(avr_t * avr, int_fast32_t * count, uint_fast16_t addr, uint_fast8_t v) {
	PROFILE_START(iow_sreg);
	REG_TOUCH(avr, addr);

	_avr_data_write(avr, addr, v);
	SET_SREG_FROM(avr, v);

	if(avr->sreg[S_I] != avr->i_shadow)
		*count = 0;

	SREG();
	PROFILE_STOP(iow_sreg);
}

static IO_INLINE void _avr_reg_io_write_data(avr_t * avr, int_fast32_t * count, uint_fast16_t addr, uint_fast8_t v) {
	PROFILE_START(iow_data);
	REG_TOUCH(avr, addr);

	_avr_data_write(avr, addr, v);
	PROFILE_STOP(iow_data);
}

static IO_INLINE void _avr_reg_io_write_wc(avr_t * avr, int_fast32_t * count, uint_fast16_t addr, uint_fast8_t v) {
	PROFILE_START(iow_wc);
	REG_TOUCH(avr, addr);

	uint8_t io = AVR_DATA_TO_IO(addr);

	avr->io[io].w.c(avr, addr, v, avr->io[io].w.param);

	*count = 0;
	PROFILE_STOP(iow_wc);
}

static IO_INLINE void _avr_reg_io_write_irq(avr_t * avr, int_fast32_t * count, uint_fast16_t addr, uint_fast8_t v) {
	PROFILE_START(iow_irq);
	REG_TOUCH(avr, addr);

	uint8_t io = AVR_DATA_TO_IO(addr);

	_avr_data_write(avr, addr, v);

	avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
	for (int i = 0; i < 8; i++)
		avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);

	*count = 0;
	PROFILE_STOP(iow_irq);
}

static IO_INLINE void _avr_reg_io_write_wc_irq(avr_t * avr, int_fast32_t * count, uint_fast16_t addr, uint_fast8_t v) {
	PROFILE_START(iow_wc_irq);
	REG_TOUCH(avr, addr);

	uint8_t io = AVR_DATA_TO_IO(addr);

	avr->io[io].w.c(avr, addr, v, avr->io[io].w.param);
	*count = 0;

	avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
	for (int i = 0; i < 8; i++)
		avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);

	PROFILE_STOP(iow_wc_irq);
}

static void _avr_reg_io_write_trap(avr_t * avr, int_fast32_t * count, uint_fast16_t addr, uint_fast8_t v) {
	if (addr == R_SREG) {
		SET_IO_WRITE_FN_DO_RET(addr, _avr_reg_io_write_sreg);
#ifndef FAST_CORE_IO_DISPTACH_TABLES
	} else if ((addr == R_SPL) || (addr == R_SPH)) {
		SET_IO_WRITE_FN_DO_RET(addr, _avr_reg_io_write_data);
#endif
	} else if (addr > 31) {
		uint8_t io = AVR_DATA_TO_IO(addr);

		if((avr->io[io].w.c) && (avr->io[io].irq)) {
			SET_IO_WRITE_FN_DO_RET(addr, _avr_reg_io_write_wc_irq);
		} else if(avr->io[io].w.c) {
			SET_IO_WRITE_FN_DO_RET(addr, _avr_reg_io_write_wc);
		} else if(avr->io[io].irq) {
			SET_IO_WRITE_FN_DO_RET(addr, _avr_reg_io_write_irq);
		} else {
			SET_IO_WRITE_FN_DO_RET(addr, _avr_reg_io_write_data);
		}
	} else {
		SET_IO_WRITE_FN_DO_RET(addr, _avr_reg_io_write_data);
	}
}

static inline void _avr_reg_io_write(avr_t * avr, int_fast32_t * count, uint_fast16_t addr, uint_fast8_t v) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(addr > avr->ramend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif

#ifdef FAST_CORE_IO_DISPTACH_TABLES
	if(likely(FAST_CORE_IO_WRITE_FN(addr))) {
		PROFILE(iow, FAST_CORE_IO_WRITE_FN_DO(avr, count, addr, v));
	} else
#endif
		PROFILE(iow, _avr_reg_io_write_trap(avr, count, addr, v));
}

/*
 * Set any address to a value; split between registers and SRAM
 */
static inline void _avr_set_ram(avr_t * avr, int_fast32_t * count, uint_fast16_t addr, uint_fast8_t v) {
	if (likely(addr >= 256 && addr <= avr->ramend))
		return(_avr_data_write(avr, addr, v));
	else
		return(_avr_reg_io_write(avr, count, addr, v));
}

static IO_INLINE uint_fast8_t _avr_reg_io_read_sreg(avr_t * avr, int_fast32_t * count, uint_fast16_t addr) {
	PROFILE_START(ior_sreg);

	register uint_fast8_t sreg = 0;
	
	READ_SREG_INTO(avr, sreg);
	_avr_data_write(avr, R_SREG, sreg);

	PROFILE_STOP(ior_sreg);
	return(sreg);
}

static IO_INLINE uint_fast8_t _avr_reg_io_read_data(avr_t * avr, int_fast32_t * count, uint_fast16_t addr) {
	PROFILE_START(ior_data);
	
	uint_fast8_t  data = _avr_data_read(avr, addr);
	
	PROFILE_STOP(ior_data);
	return(data);
}

static IO_INLINE uint_fast8_t _avr_reg_io_read_rc(avr_t * avr, int_fast32_t * count, uint_fast16_t addr) {
	PROFILE_START(ior_rc);
	uint8_t io = AVR_DATA_TO_IO(addr);

	register uint_fast8_t data = avr->io[io].r.c(avr, addr, avr->io[io].r.param);
	_avr_data_write(avr, addr, data);
	
	*count = 0;
	
	PROFILE_STOP(ior_rc);
	return(data);
}

static IO_INLINE uint_fast8_t _avr_reg_io_read_irq(avr_t * avr, int_fast32_t * count, uint_fast16_t addr) {
	PROFILE_START(ior_irq);
	uint8_t io = AVR_DATA_TO_IO(addr);

	uint8_t v = _avr_data_read(avr, addr);
	avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
	for (int i = 0; i < 8; i++)
		avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);
	*count = 0;
	
	uint_fast8_t data = _avr_data_read(avr, addr);
	
	PROFILE_STOP(ior_irq);
	return(data);
}

static IO_INLINE uint_fast8_t _avr_reg_io_read_rc_irq(avr_t * avr, int_fast32_t * count, uint_fast16_t addr) {
	PROFILE_START(ior_rc_irq);
	uint8_t io = AVR_DATA_TO_IO(addr);

	uint_fast8_t data = avr->io[io].r.c(avr, addr, avr->io[io].r.param);
	_avr_data_write(avr, addr, data);

	avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, data);
	for (int i = 0; i < 8; i++)
		avr_raise_irq(avr->io[io].irq + i, (data >> i) & 1);

	*count = 0;
	
	data = _avr_data_read(avr, addr);
	
	PROFILE_STOP(ior_rc_irq);
	return(data);
}

static uint_fast8_t _avr_reg_io_read_trap(avr_t * avr, int_fast32_t * count, uint_fast16_t addr) {
	if (addr == R_SREG) {
		SET_IO_READ_FN_DO_RET(addr, _avr_reg_io_read_sreg);
#ifndef FAST_CORE_IO_DISPTACH_TABLES
	} else if ((addr == R_SPL) || (addr == R_SPH)) {
		SET_IO_READ_FN_DO_RET(addr, _avr_reg_io_read_data);
#endif
	} else if (addr > 31) {
		uint8_t io = AVR_DATA_TO_IO(addr);
	
		if((avr->io[io].r.c) && (avr->io[io].irq)) {
			SET_IO_READ_FN_DO_RET(addr, _avr_reg_io_read_rc_irq);
		} else if(avr->io[io].r.c) {
			SET_IO_READ_FN_DO_RET(addr, _avr_reg_io_read_rc);
		} else if(avr->io[io].irq) {
			SET_IO_READ_FN_DO_RET(addr, _avr_reg_io_read_irq);
		} else {
			SET_IO_READ_FN_DO_RET(addr, _avr_reg_io_read_data);
		}
	} else {
		SET_IO_READ_FN_DO_RET(addr, _avr_reg_io_read_data);
	}
}

static inline uint_fast8_t _avr_reg_io_read(avr_t * avr, int_fast32_t * count, uint_fast16_t addr) {
	uint_fast8_t data;
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(addr > avr->ramend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif

	PROFILE_START(ior);
	
#ifdef FAST_CORE_IO_DISPTACH_TABLES
	if(likely(FAST_CORE_IO_READ_FN(addr)))
		data =  FAST_CORE_IO_READ_FN_DO(avr, count, addr);
	else
#endif
		data = _avr_reg_io_read_trap(avr, count, addr);
		
	PROFILE_STOP(ior);
	return(data);
}

/*
 * Get a value from SRAM.
 */
static inline uint_fast8_t _avr_get_ram(avr_t * avr, int_fast32_t * count, uint_fast16_t addr)
{
	uint_fast8_t data;
	if(likely(addr >= 256 && addr <= avr->ramend))
		return(_avr_data_read(avr, addr));
	else {
		data = _avr_reg_io_read(avr, count, addr);
		return(data);
	}
}

#ifdef FAST_CORE_READ_MODIFY_WRITE
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
typedef struct avr_rmw_t {
	void *	data;
	int	flags;
}avr_rmw_t, *avr_rmw_p;
#define AVR_RMW(name) avr_rmw_t rmw_##name##_rmw; avr_rmw_p name = &rmw_##name##_rmw
#else
typedef void *avr_rmw_p;
#define AVR_RMW(name) avr_rmw_p name;
#endif

typedef avr_rmw_p * const avr_rmw_h;

/*
	generic rmw functions */

static inline uint8_t avr_rmw8_ptr_set_fetch(avr_rmw_h rmw, uint8_t *data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	(*rmw)->data = data;
	(*rmw)->flags = 8;
#else
	*(uint8_t **)rmw =data;
#endif
	return(*data);
}

static inline void avr_rmw8_store(avr_rmw_p rmw, uint8_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(!rmw) {
		printf("FAST-CORE (%s): null pointer\n", __FUNCTION__);
		abort();
	} else if(rmw->flags != 8) {
		printf("FAST-CORE (%s): get from invalid set pointer type, %p[.data: %p .flags: 0x%04x]\n",
			__FUNCTION__, rmw, (uint8_t *)rmw->data, rmw->flags);
	}
	*(uint8_t *)rmw->data = data;
#else
	*(uint8_t *)rmw = data;
#endif
}

static inline uint16_t avr_rmw16_ptr_set_fetch(avr_rmw_h rmw, uint16_t *data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	(*rmw)->data = data;
	(*rmw)->flags = 16;
#else
	*(uint16_t **)rmw = data;
#endif
	return(*data);
}

static inline void avr_rmw16_store(avr_rmw_p rmw, uint16_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(!rmw) {
		printf("FAST-CORE (%s): null pointer\n", __FUNCTION__);
		abort();
	} else if(rmw->flags != 16) {
		printf("FAST-CORE (%s): get from invalid set pointer type, %p[.data: %p .flags: 0x%04x]\n",
			__FUNCTION__, rmw, (uint16_t *)rmw->data, rmw->flags);
	}
	*(uint16_t *)rmw->data = data;
#else
	*(uint16_t *)rmw = data;
#endif
}

static inline uint_fast16_t _avr_rmw_fetch16(void* p, uint_fast16_t addr, avr_rmw_h ptr_data) {
	return(avr_rmw16_ptr_set_fetch(ptr_data, ((uint16_t*)&((uint8_t *)p)[addr])));
}

/*
	avr data function */

static inline uint_fast8_t _avr_data_rmw(avr_t* avr, uint_fast16_t addr, avr_rmw_h ptr_reg) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(addr > avr->ramend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif
	return(avr_rmw8_ptr_set_fetch(ptr_reg, &avr->data[addr]));
}

static inline uint_fast16_t _avr_data_rmw16le(avr_t* avr, uint_fast16_t addr, avr_rmw_h ptr_reg) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif
	return(_avr_bswap16le(_avr_rmw_fetch16(avr->data, addr, ptr_reg)));
}

static inline void _avr_set_rmw(avr_rmw_p ptr_reg, uint_fast8_t data) {
	avr_rmw8_store(ptr_reg, data);
}

static inline void _avr_rmw_write16le(avr_rmw_p ptr_data, uint_fast16_t data) {
	avr_rmw16_store(ptr_data, _avr_bswap16le(data));
}

/*
	stack accessors */

static inline uint_fast16_t _avr_rmw_sp(avr_t * avr, avr_rmw_h ptr_sp) {
	return(_avr_data_rmw16le(avr, R_SPL, ptr_sp));
}

/*
	avr register functions */

static inline uint_fast8_t _avr_rmw_r(avr_t* avr, uint_fast8_t reg, avr_rmw_h reg_ptr) {
	return(_avr_data_rmw(avr, reg, reg_ptr));
}

static inline uint_fast16_t _avr_rmw_r16le(avr_t* avr, uint_fast8_t addr, avr_rmw_h reg_ptr) {
	return(_avr_data_rmw16le(avr, addr, reg_ptr));
}

#define RMW(w) w
#define NO_RMW(w) 
#else
#define RMW(w) 
#define NO_RMW(w) w
#endif

/*
 * Stack push accessors. Push/pop 8 and 16 bits
 */
static inline void _avr_push8(avr_t * avr, int_fast32_t * count, uint_fast8_t v)
{
	NO_RMW(uint_fast16_t sp = _avr_sp_get(avr))
	RMW(AVR_RMW(ptr_sp); uint_fast16_t sp = _avr_rmw_sp(avr, &ptr_sp));

	TSTACK(printf("%s @0x%04x[0x%04x]\n", __FUNCTION__, sp, v));
	_avr_set_ram(avr, count, sp, v);

	NO_RMW(_avr_sp_set(avr, sp-1));
	RMW(_avr_rmw_write16le(ptr_sp, sp - 1));
}

static inline uint_fast8_t _avr_pop8(avr_t * avr, int_fast32_t * count)
{
	NO_RMW(uint_fast16_t sp = _avr_sp_get(avr) + 1);
	RMW(AVR_RMW(ptr_sp); uint_fast16_t sp = _avr_rmw_sp(avr, &ptr_sp) + 1);

	uint_fast8_t res = _avr_get_ram(avr, count, sp);
	TSTACK(printf("%s @0x%04x[0x%04x]\n", __FUNCTION__, sp, res));

	NO_RMW(_avr_sp_set(avr, sp));
	RMW(_avr_rmw_write16le(ptr_sp, sp));
	return res;
}

#ifdef FAST_CORE_COMBINING
static inline void _avr_push16xx(avr_t * avr, uint_fast16_t v) {
	NO_RMW(uint_fast16_t sp = _avr_sp_get(avr));
	RMW(AVR_RMW(ptr_sp); uint_fast16_t sp = _avr_rmw_sp(avr, &ptr_sp));

#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(256 > sp)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): stack pointer at 0x%04x, below end of io space, aborting.",
			__FUNCTION__, sp);
		CRASH();
	}
#endif

	if(likely(sp <= avr->ramend)) {
		_avr_data_write16(avr, sp - 1, v);
		NO_RMW(_avr_sp_set(avr, sp - 2));
		RMW(_avr_rmw_write16le(ptr_sp, sp - 2));
	} else {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): stack pointer at 0x%04x, above ramend... aborting.",
			__FUNCTION__, sp);
		CRASH();
	}
}
#endif

static inline void _avr_push16be(avr_t * avr, int_fast32_t * count, uint_fast16_t v) {
#ifdef FAST_CORE_COMBINING
	TSTACK(uint_fast16_t sp = _avr_sp_get(avr));
	STACK("push.w ([%02x]@%04x):([%02x]@%04x)\n", 
		v >> 8, sp - 1, v & 0xff, sp);
	_avr_push16xx(avr, _avr_bswap16be(v));
#else
	_avr_push8(avr, count, v);
	_avr_push8(avr, count, v >> 8);
#endif
}

static inline void _avr_push16le(avr_t * avr, int_fast32_t * count, uint_fast16_t v) {
#ifdef FAST_CORE_COMBINING
	TSTACK(uint_fast16_t sp = _avr_sp_get(avr));
	STACK("push.w ([%02x]@%04x):([%02x]@%04x)\n", 
		v & 0xff, sp - 1, v >> 8, sp);
	_avr_push16xx(avr, _avr_bswap16le(v));
#else
	_avr_push8(avr, count, v >> 8);
	_avr_push8(avr, count, v);
#endif
}

#ifdef FAST_CORE_COMBINING
static inline uint_fast16_t _avr_pop16xx(avr_t * avr) {
	NO_RMW(uint_fast16_t sp = _avr_sp_get(avr) + 2);
	RMW(AVR_RMW(ptr_sp); uint_fast16_t sp = _avr_rmw_sp(avr, &ptr_sp) + 2);

#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(256 > sp)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): stack pointer at 0x%04x, below end of io space, aborting.",
			__FUNCTION__, sp);
		CRASH();
	}
#endif

	if(likely(sp <= avr->ramend)) {
		uint_fast16_t data = _avr_data_read16(avr, sp - 1);
		NO_RMW(_avr_sp_set(avr, sp));
		RMW(_avr_rmw_write16le(ptr_sp, sp));
		return(data);
	} else {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): stack pointer at 0x%04x, above ramend... aborting.",
			__FUNCTION__, sp);
		CRASH();
	}

	return(0);
}
#endif

static inline uint_fast16_t _avr_pop16be(avr_t * avr, int_fast32_t * count) {
	uint_fast16_t data;
#ifdef FAST_CORE_COMBINING
	TSTACK(uint_fast16_t sp = _avr_sp_get(avr));
	data = _avr_bswap16be(_avr_pop16xx(avr));
	STACK("pop.w ([%02x]@%04x):([%02x]@%04x)\n", 
		data >> 8, sp + 1, data & 0xff, sp + 2);
#else
	data = _avr_pop8(avr, count) << 8;
	data |= _avr_pop8(avr, count);
#endif	
	return(data);
}

static inline uint_fast16_t _avr_pop16le(avr_t * avr, int_fast32_t * count) {
	uint_fast16_t data;
#ifdef FAST_CORE_COMBINING
	TSTACK(uint_fast16_t sp = _avr_sp_get(avr));
	data = _avr_bswap16le(_avr_pop16xx(avr));
	STACK("pop.w ([%02x]@%04x):([%02x]@%04x)\n",
		data & 0xff, sp + 1, data >> 8, sp + 2);
#else
	data = _avr_pop8(avr, count);
	data |= (_avr_pop8(avr, count) << 8);
#endif
	return(data);
}

static inline int _avr_is_instruction_32_bits(avr_t * avr, avr_flashaddr_t pc)
{
	int o = (_avr_flash_read16le(avr, pc)) & 0xfc0f;
	
	return	o == 0x9200 || // STS ! Store Direct to Data Space
			o == 0x9000 || // LDS Load Direct from Data Space
			o == 0x940c || // JMP Long Jump
			o == 0x940d || // JMP Long Jump
			o == 0x940e ||  // CALL Long Call to sub
			o == 0x940f; // CALL Long Call to sub
}

static inline void _avr_flags_zc16(avr_t* avr, const uint_fast16_t res) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = (res >> 15) & 1;
}


static inline void _avr_flags_zcnvs(avr_t* avr, const uint_fast8_t res, const uint_fast8_t vr) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vr & 1;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_flags_zcn0vs(avr_t* avr, const uint_fast8_t res, const uint_fast8_t vr) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vr & 1;
	avr->sreg[S_N] = 0;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_flags_zcnvs16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t vr) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vr & 1;
	avr->sreg[S_N] = (res >> 15) & 1;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_flags_zcn0vs16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t vr) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vr & 1;
	avr->sreg[S_N] = 0;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_flags_zns(avr_t* avr, const uint_fast8_t res) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_flags_znvs(avr_t* avr, const uint_fast8_t res) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_flags_Rzns(avr_t* avr, const uint_fast8_t res) {
	if (res)
		avr->sreg[S_Z] = 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_flags_zns16(avr_t* avr, const uint_fast16_t res) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 15) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_flags_Rzns16(avr_t* avr, const uint_fast16_t res) {
	if (res)
		avr->sreg[S_Z] = 0;
	avr->sreg[S_N] = (res >> 15) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_flags_znv0s(avr_t* avr, const uint_fast8_t res) {
	avr->sreg[S_V] = 0;
	_avr_flags_zns(avr, res);
}

static inline void _avr_flags_znv0s16(avr_t* avr, const uint_fast16_t res) {
	avr->sreg[S_V] = 0;
	_avr_flags_zns16(avr, res);
}

/* solutions pulled from NO EXECUTE website, bochs, qemu */
static inline void _avr_flags_add(avr_t* avr, const uint_fast8_t res, const uint_fast8_t rd, const uint_fast8_t rr) {
	uint_fast8_t xvec = (rd ^ rr);
	uint_fast8_t ovec = (rr ^ res) & ~xvec;
	uint_fast8_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 3) & 1;
	avr->sreg[S_C] = (rxor >> 7) & 1;

	avr->sreg[S_V] = (ovec >> 7) & 1;
}

static inline void _avr_flags_add_zns(avr_t* avr, const uint_fast8_t res, const uint_fast8_t rd, const uint_fast8_t rr) {
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

static inline void _avr_flags_add16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t rd, const uint_fast16_t rr) {
	uint_fast16_t xvec = (rd ^ rr);
	uint_fast16_t ovec = (rr ^ res) & ~xvec;
	uint_fast16_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 11) & 1;
	avr->sreg[S_C] = (rxor >> 15) & 1;

	avr->sreg[S_V] = (ovec >> 15) & 1;
}

static inline void _avr_flags_add16_zns16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t rd, const uint_fast16_t rr) {
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
		_avr_flags_add16_zns16 function...
		
		prior implimentation was:
			_avr_flags_add16(avr, res, vd, vr);
			_avr_flags_zns16(avr, res & 0xff00);
		
		in particular it was found that zero flag was incorrectly set. */

	avr->sreg[S_Z] = (res & 0xff00) == 0;
	avr->sreg[S_N] = (res >> 15) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_flags_sub(avr_t* avr, const uint_fast8_t res, const uint_fast8_t rd, const uint_fast8_t rr) {
	uint_fast8_t xvec = (rd ^ rr);
	uint_fast8_t ovec = (rd ^ res) & xvec;
	uint_fast8_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 3) & 1;
	avr->sreg[S_C] = (rxor >> 7) & 1;

	avr->sreg[S_V] = (ovec >> 7) & 1;
}

static inline void _avr_flags_sub_zns(avr_t* avr, const uint_fast8_t res, const uint_fast8_t rd, const uint_fast8_t rr) {
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

static inline void _avr_flags_sub_Rzns(avr_t* avr, const uint_fast8_t res, const uint_fast8_t rd, const uint_fast8_t rr) {
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

static inline void _avr_flags_sub16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t rd, const uint_fast16_t rr) {
	uint_fast16_t xvec = (rd ^ rr);
	uint_fast16_t ovec = (rd ^ res) & xvec;
	uint_fast16_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 11) & 1;
	avr->sreg[S_C] = (rxor >> 15) & 1;

	avr->sreg[S_V] = (ovec >> 15) & 1;
}

static inline void _avr_flags_sub16_zns16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t rd, const uint_fast16_t rr) {
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

#if defined(FAST_CORE_USE_CPI_BRXX) || defined(FAST_CORE_CPI_BRXX_CODE)
#define FAST_CORE_CPI_BRXX_GROUP \
		UINST_ESAC_DEF(h4k8o7_cpi_brcc) \
		UINST_ESAC_DEF(h4k8o7_cpi_brcs) \
		UINST_ESAC_DEF(h4k8o7_cpi_breq) \
		UINST_ESAC_DEF(h4k8o7_cpi_brne)
#else
#define FAST_CORE_CPI_BRXX_GROUP
#endif

#define UINST_ESAC_DEF_TABLE \
		UINST_ESAC_DEF(avr_decode_one) \
		UINST_ESAC_DEF(d5r5_adc) \
		UINST_ESAC_DEF(d5r5_add) \
		UINST_ESAC_DEF(d5r5_add_adc) \
		UINST_ESAC_DEF(p2k6_adiw) \
		UINST_ESAC_DEF(d5r5_and) \
		UINST_ESAC_DEF(h4k8_andi) \
		UINST_ESAC_DEF(h4k16_andi_andi) \
		UINST_ESAC_DEF(h4r5k8_andi_or) \
		UINST_ESAC_DEF(h4k8k8_andi_ori) \
		UINST_ESAC_DEF(d5_asr) \
		UINST_ESAC_DEF(b3_bclr) \
		UINST_ESAC_DEF(d5m8_bld) \
		UINST_ESAC_DEF(o7_brcc) \
		UINST_ESAC_DEF(o7_brcs) \
		UINST_ESAC_DEF(o7_breq) \
		UINST_ESAC_DEF(o7_brne) \
		UINST_ESAC_DEF(o7_brpl) \
		UINST_ESAC_DEF(o7b3_brxc) \
		UINST_ESAC_DEF(o7b3_brxs) \
		UINST_ESAC_DEF(b3_bset) \
		UINST_ESAC_DEF(d5b3_bst) \
		UINST_ESAC_DEF(x22_call) \
		UINST_ESAC_DEF(a5m8_cbi) \
		UINST_ESAC_DEF(cli) \
		UINST_ESAC_DEF(d5_clr) \
		UINST_ESAC_DEF(d5_com) \
		UINST_ESAC_DEF(d5r5_cp) \
		UINST_ESAC_DEF(d5r5_cp_cpc) \
		UINST_ESAC_DEF(d5r5o7_cp_cpc_brne) \
		UINST_ESAC_DEF(d5r5_cpc) \
		UINST_ESAC_DEF(h4k8_cpi) \
			FAST_CORE_CPI_BRXX_GROUP \
		UINST_ESAC_DEF(h4r5k8_cpi_cpc) \
		UINST_ESAC_DEF(d5r5_16_cpse) \
		UINST_ESAC_DEF(d5r5_32_cpse) \
		UINST_ESAC_DEF(d5_dec) \
		UINST_ESAC_DEF(eicall) \
		UINST_ESAC_DEF(eijmp) \
		UINST_ESAC_DEF(d5r5_eor) \
		UINST_ESAC_DEF(icall) \
		UINST_ESAC_DEF(ijmp) \
		UINST_ESAC_DEF(d5a6_in) \
		UINST_ESAC_DEF(d5a6k8_in_andi_out) \
		UINST_ESAC_DEF(d5a6k8_in_ori_out) \
		UINST_ESAC_DEF(d5a6_in_push) \
		UINST_ESAC_DEF(d5a6m8_in_sbrs) \
		UINST_ESAC_DEF(d5_inc) \
		UINST_ESAC_DEF(x22_jmp) \
		UINST_ESAC_DEF(d5rXYZ_ld_no_op) \
		UINST_ESAC_DEF(d5rXYZ_ld_pre_dec) \
		UINST_ESAC_DEF(d5rXYZ_ld_post_inc) \
		UINST_ESAC_DEF(d5rYZq6_ldd) \
		UINST_ESAC_DEF(d5rYZq6_ldd_ldd) \
		UINST_ESAC_DEF(h4k8_ldi) \
		UINST_ESAC_DEF(h4k16_ldi_ldi) \
		UINST_ESAC_DEF(h4k8a6_ldi_out) \
		UINST_ESAC_DEF(d5x16_lds) \
		UINST_ESAC_DEF(d5x16_lds_no_io) \
		UINST_ESAC_DEF(d5x16_lds_lds) \
		UINST_ESAC_DEF(d5x16_lds_lds_no_io) \
		UINST_ESAC_DEF(d5x16_lds_no_io_tst) \
		UINST_ESAC_DEF(d5x16_lds_tst) \
		UINST_ESAC_DEF(d5_lpm_z) \
		UINST_ESAC_DEF(d5rXYZ_lpm_z_post_inc_st_post_inc) \
		UINST_ESAC_DEF(d5_lpm_z_post_inc) \
		UINST_ESAC_DEF(d5_lpm16_z_post_inc) \
		UINST_ESAC_DEF(d5_lsl) \
		UINST_ESAC_DEF(d5r5_lsl_lsl) \
		UINST_ESAC_DEF(d5_lsl_rol) \
		UINST_ESAC_DEF(d5_lsr) \
		UINST_ESAC_DEF(d5r5_lsr_lsr) \
		UINST_ESAC_DEF(d5_lsr_ror) \
		UINST_ESAC_DEF(d5r5_mov) \
		UINST_ESAC_DEF(d4r4_movw) \
		UINST_ESAC_DEF(d5r5_mul) \
		UINST_ESAC_DEF(d16r16_muls) \
		UINST_ESAC_DEF(d5_neg) \
		UINST_ESAC_DEF(nop) \
		UINST_ESAC_DEF(d5r5_or) \
		UINST_ESAC_DEF(h4k8_ori) \
		UINST_ESAC_DEF(d5a6_out) \
		UINST_ESAC_DEF(d5r5_out_sph_sreg_spl) \
		UINST_ESAC_DEF(d5_pop) \
		UINST_ESAC_DEF(d5a6_pop_out) \
		UINST_ESAC_DEF(d5_pop_pop16be) \
		UINST_ESAC_DEF(d5_pop_pop16le) \
		UINST_ESAC_DEF(d5_push) \
		UINST_ESAC_DEF(d5_push_push16be) \
		UINST_ESAC_DEF(d5_push_push16le) \
		UINST_ESAC_DEF(o12_rcall) \
		UINST_ESAC_DEF(ret) \
		UINST_ESAC_DEF(reti) \
		UINST_ESAC_DEF(o12_rjmp) \
		UINST_ESAC_DEF(d5_rol) \
		UINST_ESAC_DEF(d5_ror) \
		UINST_ESAC_DEF(d5r5_sbc) \
		UINST_ESAC_DEF(h4k8_sbci) \
		UINST_ESAC_DEF(a5m8_sbi) \
		UINST_ESAC_DEF(a5m8_16_sbic) \
		UINST_ESAC_DEF(a5m8_32_sbic) \
		UINST_ESAC_DEF(a5m8_16_sbis) \
		UINST_ESAC_DEF(a5m8_32_sbis) \
		UINST_ESAC_DEF(p2k6_sbiw) \
		UINST_ESAC_DEF(d5m8_16_sbrc) \
		UINST_ESAC_DEF(d5m8_32_sbrc) \
		UINST_ESAC_DEF(d5m8_16_sbrs) \
		UINST_ESAC_DEF(d5m8_32_sbrs) \
		UINST_ESAC_DEF(sei) \
		UINST_ESAC_DEF(sei_sleep) \
		UINST_ESAC_DEF(sleep) \
		UINST_ESAC_DEF(d5rXYZ_st_no_op) \
		UINST_ESAC_DEF(d5rXYZ_st_pre_dec) \
		UINST_ESAC_DEF(d5rXYZ_st_post_inc) \
		UINST_ESAC_DEF(d5rYZq6_std) \
		UINST_ESAC_DEF(d5rYZq6_std_std_hhll) \
		UINST_ESAC_DEF(d5rYZq6_std_std_hllh) \
		UINST_ESAC_DEF(d5x16_sts) \
		UINST_ESAC_DEF(d5x16_sts_no_io) \
		UINST_ESAC_DEF(d5x16_sts_sts) \
		UINST_ESAC_DEF(d5x16_sts_sts_no_io) \
		UINST_ESAC_DEF(d5r5_sub) \
		UINST_ESAC_DEF(h4k8_subi) \
		UINST_ESAC_DEF(h4k16_subi_sbci) \
		UINST_ESAC_DEF(d5_swap) \
		UINST_ESAC_DEF(d5_tst)

#undef UINST_ESAC_DEF
#define UINST_ESAC_DEF(name) k_avr_uinst_##name,

enum {
	UINST_ESAC_DEF_TABLE
};

static inline void uFlashWrite(avr_t* avr, avr_flashaddr_t addr, uint_fast32_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(addr > avr->flashend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of flash, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif

	UFLASH_ADDR_AT(addr) = data;
}

extern inline uint_fast32_t uFlashRead(avr_t* avr, avr_flashaddr_t addr) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(addr > avr->flashend)) {
		AVR_LOG(avr, LOG_ERROR, "FAST-CORE (%s): access at 0x%04x past end of flash, aborting.", __FUNCTION__, addr);
		CRASH();
	}
#endif

	return(UFLASH_ADDR_AT(addr));
}

#if 1
#define U_R0(xu_opcode) (xu_opcode & 0xff)
#define U_R1(xu_opcode) ((xu_opcode >> 8) & 0xff)
#define U_R2(xu_opcode) ((xu_opcode >> 16) & 0xff)
#define U_R3(xu_opcode) ((xu_opcode >> 24) & 0xff)
#else
#define U_R0(xu_opcode) (((uint8_t *)&xu_opcode)[0])
#define U_R1(xu_opcode) (((uint8_t *)&xu_opcode)[1])
#define U_R2(xu_opcode) (((uint8_t *)&xu_opcode)[2])
#define U_R3(xu_opcode) (((uint8_t *)&xu_opcode)[3])
#endif

#if 1
#define U_X16(xu_opcode) (xu_opcode >> 16)
#else
#define U_X16(xu_opcode) (((uint16_t *)&xu_opcode)[1])
#endif

#define U_OPCODE_FETCH(xxu_opcode, addr) xxu_opcode = uFlashRead(avr, addr)
#define U_FETCH_OPCODE(xau_opcode, addr) uint_fast32_t xau_opcode = uFlashRead(avr, addr)
#define U_FETCH_OPCODE2(xbu_opcode, addr) uint_fast32_t xbu_opcode = uFlashReadB(avr, addr)

#define UINST_GET_OP(op, xu_opcode) uint_fast8_t op = U_R0(xu_opcode)
#define UINST_GET_R0(r0, xu_opcode) uint_fast8_t r0 = U_R0(xu_opcode)
#define UINST_GET_R1(r1, xu_opcode) uint_fast8_t r1 = U_R1(xu_opcode)
#define UINST_GET_R2(r2, xu_opcode) uint_fast8_t r2 = U_R2(xu_opcode)
#define UINST_GET_iR2(r2, xu_opcode) int_fast8_t r2 = U_R2(xu_opcode)
#define UINST_GET_R3(r3, xu_opcode) uint_fast8_t r3 = U_R3(xu_opcode)
#define UINST_GET_iR3(r3, xu_opcode) int_fast8_t r3 = U_R3(xu_opcode)
#define UINST_GET_X16(x16, xu_opcode) uint_fast16_t x16 = U_X16(xu_opcode)
#define U_X24(xu_opcode) (xu_opcode >> 8)
#define UINST_GET_X24(x24, xu_opcode) uint_fast32_t x24 = U_X24(xu_opcode)

#define OPCODE(xu_opcode, r1, r2, r3) ((r3<<24)|(r2<<16)|(r1<<8)|(k_avr_uinst_##xu_opcode))
#define OPCODE2(r0, r1, r2, r3) ((r3<<24)|(r2<<16)|(r1<<8)|(r0))

static inline uint_fast8_t INST_DECODE_B3(const uint_fast16_t o) {
	return(o & 0x0007);
}

static inline uint_fast8_t INST_DECODE_D4(const uint_fast16_t o) {
	return((o & 0x00f0) >> 4);
}

static inline uint_fast8_t INST_DECODE_D5(const uint_fast16_t o) {
	return((o & 0x01f0) >> 4);
}

static inline uint_fast8_t INST_DECODE_R4(const uint_fast16_t o) {
	return(o & 0x000f);
}

#define I_OPCODE_FETCH(xxi_opcode, addr) xxi_opcode = _avr_flash_read16le(avr, addr)
#define I_FETCH_OPCODE(xxi_opcode, addr) uint_fast16_t xxi_opcode = _avr_flash_read16le(avr, addr)
#define INST_GET_A5(a5, xi_opcode) uint_fast8_t a5 = ( 32 + ((xi_opcode & 0x00f8) >> 3) )
#define INST_GET_A6(a6, xi_opcode) uint_fast8_t a6 = ( 32 + ( ((xi_opcode & 0x0600) >> 5) | INST_DECODE_R4(xi_opcode) ) )
#define INST_GET_B3a(b3, xi_opcode) uint_fast8_t b3 = INST_DECODE_B3(xi_opcode)
#define INST_GET_B3b(b3, xi_opcode) uint_fast8_t b3 = ((xi_opcode & 0x0070) >> 4)
#define INST_GET_D4(d4, xi_opcode) uint_fast8_t d4 = INST_DECODE_D4(xi_opcode)
#define INST_GET_D5(d5, xi_opcode) uint_fast8_t d5 = INST_DECODE_D5(xi_opcode)
#define INST_GET_D16(d16, xi_opcode) uint_fast8_t d16 = (16 + INST_DECODE_D4(xi_opcode))
#define INST_GET_H4(h4, xi_opcode) uint_fast8_t h4 = (16 + INST_DECODE_D4(xi_opcode))
#define INST_GET_K6(k6, xi_opcode) uint_fast8_t k6 = (((xi_opcode & 0x00c0) >> 2) | INST_DECODE_R4(xi_opcode))
#define INST_GET_K8(k8, xi_opcode) uint_fast8_t k8 = (((xi_opcode & 0x0f00) >> 4) | INST_DECODE_R4(xi_opcode))
#define INST_GET_O7(o7, xi_opcode) int_fast8_t o7 = ((int16_t)((xi_opcode & 0x03f8) << 6) >> 8)
#define INST_GET_O12(o12, xi_opcode) int_fast16_t o12 = ((int16_t)((xi_opcode & 0x0fff) << 4) >> 3)
#define INST_GET_P2(p2, xi_opcode) uint_fast8_t p2 = (24 + ((xi_opcode & 0x0030) >> 3))
#define INST_GET_Q6(q6, xi_opcode) uint_fast8_t q6 = ( ((xi_opcode & 0x2000) >> 8) | ((xi_opcode & 0x0c00) >> 7) | INST_DECODE_B3(xi_opcode) )
#define INST_GET_R4(r4, xi_opcode) uint_fast8_t r4 = INST_DECODE_R4(xi_opcode)
#define INST_GET_R5(r5, xi_opcode) uint_fast8_t r5 = ( ((xi_opcode & 0x0200) >> 5) | INST_DECODE_R4(xi_opcode) )
#define INST_GET_R16(r16, xi_opcode) uint_fast8_t r16 = (16 + INST_DECODE_R4(xi_opcode))

typedef void (pfnInst_t)(avr_t* avr, int_fast32_t * count, uint_fast32_t u_opcode);
#define DEF_INST(name) \
	void _avr_inst_##name(avr_t * avr, int_fast32_t * count, uint16_t i_opcode)
#define DO_INST(name) \
	_avr_inst_##name(avr, count, i_opcode)
#define UINST(name) \
	void _avr_uinst_##name(avr_t * avr, int_fast32_t * count, uint_fast32_t u_opcode)
#define DO_UINST(name) \
	_avr_uinst_##name(avr, count, u_opcode)
#define U_DO_UINST(name) \
	_avr_uinst_##name(avr, count, u_opcode)
#define DO_PFN_UINST(pfn_opcode) \
	pfn(avr, count, pfn_opcode)
#define eUINST(name) \
	extern void _avr_uinst_##name(avr_t* avr, int_fast32_t * count, uint_fast32_t u_opcode)

typedef pfnInst_t * pfnInst_p;


#ifdef FAST_CORE_ITRACE
#define ITRACE(combining) { \
		iSTATE("\t\t\t\t\t\t\t\t%s  (0x%04x [0x%08x]) %s\n", (combining ? "combining" : "         "), \
			i_opcode, u_opcode, __FUNCTION__); \
	}
#else
#define ITRACE(combining)
#endif

#define INST_XLAT(format, name) \
	avr_flashaddr_t __attribute__((__unused__)) inst_pc = avr->pc; \
	avr_flashaddr_t __attribute__((__unused__)) new_pc = 2 + inst_pc; \
	pfnInst_p __attribute__((__unused__)) pfn = _avr_uinst_##format##_##name; \
	uint_fast32_t pfn_opcode = INST_XLAT_##format(avr, k_avr_uinst_##format##_##name, i_opcode); \
	uint_fast32_t u_opcode = pfn_opcode;

#define INST_XLAT_DO_WRITE(format, name) \
	INST_XLAT(format, name); \
	PROFILE(uinst[U_R0(u_opcode)], DO_UINST(format##_##name)); \
	uFlashWrite(avr, inst_pc, u_opcode);

#define INST(name)\
	DEF_INST(name) { \
		avr_flashaddr_t __attribute__((__unused__)) inst_pc = avr->pc; \
		uint_fast32_t u_opcode = OPCODE(name, 0, 0, 0); \
		DO_UINST(name); \
		uFlashWrite(avr, inst_pc, u_opcode); \
		ITRACE(0); \
	}

#define BEGIN_COMBINING \
	int combining = _FAST_CORE_COMBINING; \
	if(combining) { \
		I_FETCH_OPCODE(next_opcode, new_pc);

#define END_COMBINING \
		} PROFILE(uinst[U_R0(pfn_opcode)], DO_PFN_UINST(pfn_opcode)); \
		uFlashWrite(avr, inst_pc, u_opcode); \
		ITRACE(combining); }

#define BEGIN_COMPLEX \
	if(0 != _FAST_CORE_COMBINING) { \

#define END_COMPLEX \
	} PROFILE(uinst[U_R0(pfn_opcode)], DO_PFN_UINST(pfn_opcode)); uFlashWrite(avr, inst_pc, u_opcode); \
	ITRACE(0); }

static uint32_t INST_XLAT_a5m8(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_A5(a5, i_opcode);
	INST_GET_B3a(b3, i_opcode);
	uint8_t mask = (1 << b3);
	return(OPCODE2(r0, a5, mask, 0));
}
#define UINST_GET_a5m8() \
	UINST_GET_R1(io, u_opcode); \
	UINST_GET_R2(mask, u_opcode);	

#define UINSTa5m8(name)  \
	UINST(a5m8_##name)
#define INSTa5m8(name) \
	DEF_INST(a5m8_##name) { \
		INST_XLAT_DO_WRITE(a5m8, name); \
		ITRACE(0); \
	}
#define COMPLEX_INSTa5m8(name) \
	DEF_INST(a5m8_##name) { \
		INST_XLAT(a5m8, name); \
		INST_GET_A5(a5, i_opcode); \
		INST_GET_B3a(b3, i_opcode); \
		uint8_t mask = (1 << b3); \
		BEGIN_COMPLEX

static uint32_t INST_XLAT_b3(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_B3b(b3, i_opcode);
	uint8_t mask = (1 << b3);
	return(OPCODE2(r0, b3, mask, 0));
}

#define UINSTb3(name) \
	UINST(b3_##name)
#define INSTb3(name) \
	DEF_INST(b3_##name) { \
		INST_XLAT_DO_WRITE(b3, name); \
		ITRACE(0); \
	}
#define COMBINING_INSTb3(name) \
	DEF_INST(b3_##name) { \
		INST_XLAT(b3, name); \
		INST_GET_B3b(b3, i_opcode); \
		BEGIN_COMBINING
#define COMPLEX_INSTb3(name) \
	DEF_INST(b3_##name) { \
		INST_XLAT(b3, name); \
		INST_GET_B3b(b3, i_opcode); \
		BEGIN_COMPLEX

static uint32_t INST_XLAT_d4r4(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_D4(d4, i_opcode) << 1;
	INST_GET_R4(r4, i_opcode) << 1;
	return(OPCODE2(r0, d4, r4, 0));
}

#define UINST_GET_d4r4() \
	UINST_GET_R1(d, u_opcode); \
	UINST_GET_R2(r, u_opcode);

#define UINSTd4r4(name) \
	UINST(d4r4_##name)
#define INSTd4r4(name) DEF_INST(d4r4_##name) {\
		INST_XLAT_DO_WRITE(d4r4, name); \
		ITRACE(0); \
	}
	
static uint32_t INST_XLAT_d5(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_D5(d5, i_opcode);
	return(OPCODE2(r0, d5, 0, 0));
}

#define UINSTd5(name) \
	UINST(d5_##name)
#define INSTd5(name) \
	DEF_INST(d5_##name) { \
		INST_XLAT_DO_WRITE(d5, name); \
		ITRACE(0); \
	}
#define COMBINING_INSTd5(name) \
	DEF_INST(d5_##name) { \
		INST_XLAT(d5, name); \
		INST_GET_D5(d5, i_opcode); \
		BEGIN_COMBINING
#define COMPLEX_INSTd5(name) \
	DEF_INST(d5_##name) { \
		INST_XLAT(d5, name); \
		INST_GET_D5(d5, i_opcode); \
		BEGIN_COMPLEX

static uint32_t INST_XLAT_d5a6(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_D5(d5, i_opcode);
	INST_GET_A6(a6, i_opcode);
	return(OPCODE2(r0, d5, a6, 0));
}

#define UINST_GET_d5a6() \
	UINST_GET_R1(d, u_opcode); \
	UINST_GET_R2(a, u_opcode);

#define UINSTd5a6(name) \
	UINST(d5a6_##name)
#define INSTd5a6(name) DEF_INST(d5a6_##name) { \
		INST_XLAT_DO_WRITE(d5a6, name); \
		ITRACE(0); \
	}
#define COMBINING_INSTd5a6(name) \
	DEF_INST(d5a6_##name) { \
		INST_XLAT(d5a6, name); \
		INST_GET_D5(d5, i_opcode); \
		INST_GET_A6(a6, i_opcode); \
		BEGIN_COMBINING

#define UINST_GET_d5a6k8() \
	UINST_GET_d5a6(); \
	UINST_GET_R3(k, u_opcode);

#define UINSTd5a6k8(name) \
	eUINST(d5a6k8_##name)

#define UINST_GET_d5a6m8() \
	UINST_GET_d5a6(); \
	UINST_GET_R3(mask, u_opcode);

#define UINSTd5a6m8(name) \
	UINST(d5a6m8_##name)
#define eUINSTd5a6m8(name) \
	eUINST(d5a6m8_##name)

static uint32_t INST_XLAT_d5b3(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_D5(d5, i_opcode);
	INST_GET_B3a(b3, i_opcode);
	return(OPCODE2(r0, d5, b3, 0));
}


#define UINSTd5b3(name) \
	UINST(d5b3_##name)
#define INSTd5b3(name) \
	DEF_INST(d5b3_##name) {\
		INST_XLAT_DO_WRITE(d5b3, name); \
		ITRACE(0); \
	}

static uint32_t INST_XLAT_d5m8(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_D5(d5, i_opcode);
	INST_GET_B3a(b3, i_opcode);
	uint8_t mask = (1 << b3);
	return(OPCODE2(r0, d5, mask, 0));
}

#define UINST_GET_d5m8() \
	UINST_GET_R1(d, u_opcode); \
	UINST_GET_R2(mask, u_opcode);

#define UINSTd5m8(name) \
	UINST(d5m8_##name)
#define INSTd5m8(name) \
	DEF_INST(d5m8_##name) {\
		INST_XLAT_DO_WRITE(d5m8, name); \
		ITRACE(0); \
	}

static uint32_t INST_XLAT_d5r5(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_D5(d5, i_opcode);
	INST_GET_R5(r5, i_opcode);
	return(OPCODE2(r0, d5, r5, 0));
}

#define UINST_GET_d5r5() \
	UINST_GET_R1(d, u_opcode); \
	UINST_GET_R2(r, u_opcode);

#define UINSTd5r5(name) \
	UINST(d5r5_##name)
#define INSTd5r5(name) \
	DEF_INST(d5r5_##name) { \
		INST_XLAT_DO_WRITE(d5r5, name); \
		ITRACE(0); \
	}
	
#define COMBINING_INSTd5r5(name) \
	DEF_INST(d5r5_##name) { \
		INST_XLAT(d5r5, name); \
		INST_GET_D5(d5, i_opcode); \
		INST_GET_R5(r5, i_opcode); \
		BEGIN_COMBINING
#define COMPLEX_INSTd5r5(name) \
	DEF_INST(d5r5_##name) {\
		INST_XLAT(d5r5, name); \
		INST_GET_D5(d5, i_opcode); \
		INST_GET_R5(r5, i_opcode); \
		BEGIN_COMPLEX

#define UINSTd5r5o7(name) \
	UINST(d5r5o7_##name)

#define eUINSTd5r5o7(name) \
	eUINST(d5r5o7_##name)

static uint32_t INST_XLAT_d5rXYZ(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_D5(d5, i_opcode);
	uint8_t  rXYZ = ((uint8_t []){R_ZL, 0x00, R_YL, R_XL})[(i_opcode & 0x000c)>>2];
	return(OPCODE2(r0, d5, rXYZ, 0));
}

#define UINST_GET_d5rXYZ() \
	UINST_GET_d5r5();

#define UINSTd5rXYZ(name) \
	UINST(d5rXYZ_##name)

#define INSTd5rXYZ(name) \
	DEF_INST(d5rXYZ_##name) { \
		INST_XLAT_DO_WRITE(d5rXYZ, name); \
		ITRACE(0); \
	}
#define COMPLEX_INSTd5rXYZ(name) \
	DEF_INST(d5rXYZ_##name) {\
		INST_XLAT(d5rXYZ, name); \
		INST_GET_D5(d5, i_opcode); \
		uint8_t  r5 = ((uint8_t []){R_ZL, 0x00, R_YL, R_XL})[(i_opcode & 0x000c)>>2]; \
		BEGIN_COMPLEX

static uint32_t INST_XLAT_d5rYZq6(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_D5(d5, i_opcode);
	uint8_t  rYZ = ((i_opcode & 0x0008) ? R_YL : R_ZL);
	INST_GET_Q6(q6, i_opcode);
	return(OPCODE2(r0, d5, rYZ, q6));
}

#define UINST_GET_d5rYZq6() \
	UINST_GET_d5r5(); \
	UINST_GET_R3(q, u_opcode);

#define UINSTd5rYZq6(name) \
	UINST(d5rYZq6_##name)
#define INSTd5rYZq6(name) \
	DEF_INST(d5rYZq6_##name) { \
		INST_XLAT_DO_WRITE(d5rYZq6, name); \
		ITRACE(0); \
	}
#define COMBINING_INSTd5rYZq6(name) \
	DEF_INST(d5rYZq6_##name) { \
		INST_GET_D5(d5, i_opcode); \
		uint8_t  r = ((i_opcode & 0x0008) ? R_YL : R_ZL); \
		INST_GET_Q6(q6, i_opcode); \
		INST_XLAT(d5rYZq6, name); \
		BEGIN_COMBINING

static uint32_t INST_XLAT_d5x16(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_D5(d5, i_opcode);
	I_FETCH_OPCODE(x16, (2 + avr->pc));
	return(OPCODE2(r0, d5, x16, 0));
}

#define UINST_GET_d5x16() \
	UINST_GET_R1(d, u_opcode); \
	UINST_GET_X16(x, u_opcode);

#define UINSTd5x16(name) \
	UINST(d5x16_##name)
#define INSTd5x16(name) \
	DEF_INST(d5x16_##name) { \
		INST_XLAT_DO_WRITE(d5x16, name); \
		ITRACE(0); \
	}
#define COMBINING_INSTd5x16(name) \
	DEF_INST(d5x16_##name) { \
		INST_XLAT(d5x16, name); \
		INST_GET_D5(d5, i_opcode); \
		I_FETCH_OPCODE(x16, new_pc); \
		new_pc += 2; \
		BEGIN_COMBINING

static uint32_t INST_XLAT_d16r16(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_D16(d16, i_opcode);
	INST_GET_R16(r16, i_opcode);
	return(OPCODE2(r0, d16, r16, 0));
}

#define UINSTd16r16(name) \
	UINST(d16r16_##name)
#define INSTd16r16(name) \
	DEF_INST(d16r16_##name) { \
		INST_XLAT_DO_WRITE(d16r16, name); \
		ITRACE(0); \
	}

static uint32_t INST_XLAT_h4k8(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_H4(h4, i_opcode);
	INST_GET_K8(k8, i_opcode);
	return(OPCODE2(r0, h4, k8, 0));
}

#define UINST_GET_h4k8() \
	UINST_GET_R1(h, u_opcode); \
	UINST_GET_R2(k, u_opcode);

#define UINSTh4k8(name) \
	UINST(h4k8_##name)
#define INSTh4k8(name) \
	DEF_INST(h4k8_##name) { \
		INST_XLAT_DO_WRITE(h4k8, name); \
		ITRACE(0); \
	}
#define COMBINING_INSTh4k8(name) \
	DEF_INST(h4k8_##name) { \
		INST_XLAT(h4k8, name); \
		INST_GET_H4(h4, i_opcode); \
		INST_GET_K8(k8, i_opcode); \
		BEGIN_COMBINING

#define UINST_GET_h4k8a6() \
	UINST_GET_R1(h, u_opcode); \
	UINST_GET_R2(k, u_opcode); \
	UINST_GET_R3(a, u_opcode);

#define UINSTh4k8a6(name) \
	UINST(h4k8a6_##name)

#define UINST_GET_h4k8k8() \
	UINST_GET_R1(h, u_opcode); \
	UINST_GET_R2(k1, u_opcode); \
	UINST_GET_R3(k2, u_opcode);

#define UINSTh4k8k8(name) \
	UINST(h4k8k8_##name)

#define UINST_GET_h4k8o7() \
	UINST_GET_R1(h, u_opcode); \
	UINST_GET_R2(k, u_opcode); \
	UINST_GET_iR3(o, u_opcode);

#define UINSTh4k8o7(name) \
	eUINST(h4k8o7_##name)

#define UINST_GET_h4r5k8() \
	UINST_GET_R1(h, u_opcode); \
	UINST_GET_R2(r, u_opcode); \
	UINST_GET_R3(k, u_opcode);

#define UINSTh4r5k8(name) \
	UINST(h4r5k8_##name)
#define eUINSTh4r5k8(name) \
	eUINST(h4r5k8_##name)

#define UINST_GET_h4k16() \
	UINST_GET_R1(h, u_opcode); \
	UINST_GET_X16(k, u_opcode);

#define UINSTh4k16(name) \
	UINST(h4k16_##name)

#define UINSTo7(name) \
	UINST(o7_##name)

static uint32_t INST_XLAT_o7b3(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_O7(o7, i_opcode);
	INST_GET_B3a(b3, i_opcode);
	uint8_t mask = (1 << b3);
	return(OPCODE2(r0, b3, o7, mask));
}

#define UINST_GET_o7b3() \
	UINST_GET_R1(b, u_opcode); \
	UINST_GET_iR2(o, u_opcode);

#define UINSTo7b3(name) \
	UINST(o7b3_##name)
#define INSTo7b3(name) \
	DEF_INST(o7b3_##name) {\
		INST_XLAT_DO_WRITE(o7b3, name); \
		ITRACE(0); \
	}
#define COMPLEX_INSTo7b3(name) \
	DEF_INST(o7b3_##name) {\
		INST_XLAT(o7b3, name); \
		UINST_GET_o7b3(); \
		BEGIN_COMPLEX

static uint32_t INST_XLAT_o12(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_O12(o12, i_opcode);
	return(OPCODE2(r0, 0, o12, 0));
}

#define UINSTo12(name)  \
	UINST(o12_##name)
#define INSTo12(name) \
	DEF_INST(o12_##name) { \
		INST_XLAT_DO_WRITE(o12, name); \
		ITRACE(0); \
	}

static uint32_t INST_XLAT_p2k6(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	INST_GET_P2(p2, i_opcode);
	INST_GET_K6(k6, i_opcode);
	return(OPCODE2(r0, p2, k6, 0));
}

#define UINST_GET_p2k6() \
	UINST_GET_R1(p, u_opcode); \
	UINST_GET_R2(k, u_opcode);

#define UINSTp2k6(name) \
	UINST(p2k6_##name)
#define INSTp2k6(name) \
	DEF_INST(p2k6_##name) { \
		INST_XLAT_DO_WRITE(p2k6, name); \
		ITRACE(0); \
	}

static uint32_t INST_XLAT_x22(avr_t * avr, uint8_t r0, uint16_t i_opcode) {
	uint_fast8_t x6 = ((INST_DECODE_D5(i_opcode) << 1) | (i_opcode & 0x0001));
	I_FETCH_OPCODE(x16, (2 + avr->pc));
	uint_fast32_t x22 = ((x6 << 16) | x16) << 1;
	return((x22 << 8) | r0);
}

#define UINSTx22(name)  \
	UINST(x22_##name)
#define INSTx22(name) \
	DEF_INST(x22_##name) { \
		INST_XLAT_DO_WRITE(x22, name); \
		ITRACE(0); \
	}

#define U_VR(d) _avr_get_r(avr, d)
#define U_RMW_VR(d, pvd) _avr_rmw_r(avr, d, &pvd)
#define U_VR16(d) _avr_get_r16(avr, d)
#define U_VR16le(d) _avr_get_r16le(avr, d)
#define U_RMW_VR16le(d, pvd) _avr_rmw_r16le(avr, d, &pvd)

#define UINST_RMW_VD() AVR_RMW(pvd); uint_fast8_t vd = U_RMW_VR(d, pvd)
#define UINST_GET_VD() uint_fast8_t vd = U_VR(d)
#define UINST_RMW_VH() AVR_RMW(pvh); uint_fast8_t vh = U_RMW_VR(h, pvh)
#define UINST_GET_VH() uint_fast8_t vh = U_VR(h)
#define UINST_GET_VR() uint_fast8_t vr = U_VR(r)

#define U_VA(a) _avr_reg_io_read(avr, count, a)
#define UINST_GET_VA() uint_fast8_t va = U_VA(a)
#define UINST_GET_VIO() uint_fast8_t vio = U_VA(io)

#define UINST_RMW_VD16le() AVR_RMW(pvd); uint_fast16_t vd = U_RMW_VR16le(d, pvd)
#define UINST_GET_VD16le() uint_fast16_t vd = U_VR16le(d)
#define UINST_RMW_VH16le() AVR_RMW(pvh); uint_fast16_t vh = U_RMW_VR16le(h, pvh)
#define UINST_GET_VH16le() uint_fast16_t vh = U_VR16le(h)
#define UINST_RMW_VP16le() AVR_RMW(pvp); uint_fast16_t vp = U_RMW_VR16le(p, pvp)
#define UINST_GET_VP16le() uint_fast16_t vp = U_VR16le(p)
#define UINST_RMW_VR16le() AVR_RMW(pvr); uint_fast16_t vr = U_RMW_VR16le(r, pvr)
#define UINST_GET_VR16le() uint_fast16_t vr = U_VR16le(r)

#ifdef FAST_CORE_COMMON_DATA
#define UINST_GET_VD_VR() \
	UINST_GET_d5r5(); \
	uint8_t* data = avr->data; \
	uint_fast8_t vd = data[d]; \
	uint_fast8_t vr = data[r];
#define UINST_RMW_VD_VR() \
	UINST_GET_d5r5(); \
	uint8_t* data = avr->data; \
	NO_RMW(uint_fast8_t vd = data[d]); \
	RMW(AVR_RMW(pvd); uint_fast8_t vd = avr_rmw8_ptr_set_fetch(&pvd, &data[d])); \
	uint_fast8_t vr = data[r];
#define UINST_GET16le_VD_VR() \
	UINST_GET_d5r5(); \
	uint8_t* data = avr->data; \
	uint_fast16_t vd = _avr_bswap16le(*((uint16_t*)&data[d])); \
	uint_fast16_t vr = _avr_bswap16le(*((uint16_t*)&data[r]));
#define UINST_RMW16le_VD_VR() \
	UINST_GET_d5r5(); \
	uint8_t* data = avr->data; \
	NO_RMW(uint_fast16_t vd = _avr_bswap16le(*((uint16_t*)&data[d]))); \
	RMW(AVR_RMW(pvd); uint_fast16_t vd = _avr_bswap16le(avr_rmw16_ptr_set_fetch(&pvd, (uint16_t*)&data[d]))); \
	uint_fast16_t vr = _avr_bswap16le(*((uint16_t*)&data[r]));
#else
#define UINST_GET_VD_VR() \
	UINST_GET_d5r5(); \
	UINST_GET_VD(); \
	UINST_GET_VR();
#define UINST_RMW_VD_VR() \
	UINST_GET_d5r5(); \
	NO_RMW(UINST_GET_VD()); \
	RMW(UINST_RMW_VD()); \
	UINST_GET_VR();
#define UINST_GET16le_VD_VR() \
	UINST_GET_d5r5(); \
	UINST_GET_VD16le(); \
	UINST_GET_VR16le();
#define UINST_RMW16le_VD_VR() \
	UINST_GET_d5r5(); \
	NO_RMW(UINST_GET_VD16le()); \
	RMW(UINST_RMW_VD16le()); \
	UINST_GET_VR16le();
#endif

#if 1
#define STEP_PC_CYCLES(x, y) CYCLES(y); avr->pc += (x);
#define JMP_PC_CYCLES(x, y) CYCLES(y); avr->pc = (x);
#else
#define STEP_PC_CYCLES(x, y) avr->pc += (x); CYCLES(y);
#define JMP_PC_CYCLES(x, y) avr->pc = (x); CYCLES(y);
#endif

#ifdef CORE_FAST_CORE_DIFF_TRACE
#define CAT_STEP_PC_CYCLES(x, y) \
	STEP_PC_CYCLES(x, y); \
	if(0 >= *count) return;
#else
#define CAT_STEP_PC_CYCLES(x, y) \
	T(STEP_PC_CYCLES(x, y));
#endif

#ifdef FAST_CORE_TAIL_CALL
static pfnInst_p uinst_op_table[256];
#define SPLT_NEXT() \
	if(likely(0 < *count)) { \
		U_OPCODE_FETCH(u_opcode, avr->pc); \
		UINST_GET_OP(u_opcode_op, u_opcode); \
		pfnInst_p pfn = uinst_op_table[u_opcode_op]; \
		return(DO_PFN_UINST(u_opcode)); \
	} else \
		return;
#else
#define SPLT_NEXT() return
#endif

#define UINST_NEXT_PC_CYCLES(next_pc, next_cycles) \
	STEP_PC_CYCLES(next_pc, next_cycles); \
	SPLT_NEXT();
#define UINST_NEXT_JMP_PC_CYCLES(next_pc, next_cycles) \
	JMP_PC_CYCLES(next_pc, next_cycles); \
	SPLT_NEXT();

UINSTd5r5(adc) {
	NO_RMW(UINST_GET_VD_VR());
	RMW(UINST_RMW_VD_VR());
	uint_fast8_t res = vd + vr + avr->sreg[S_C];

	if (r == d) {
		STATE("rol %s[%02x] = %02x\n", avr_regname(d), vd, res);
	} else {
		STATE("addc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
	}

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_add_zns(avr, res, vd, vr);

	SREG();
	
	UINST_NEXT_PC_CYCLES(2, 1);
}
COMPLEX_INSTd5r5(adc)
	if(d5 == r5)
		u_opcode = OPCODE(d5_rol, d5, 0, 0);
END_COMPLEX

UINSTd5r5(add) {
	NO_RMW(UINST_GET_VD_VR());
	RMW(UINST_RMW_VD_VR());
	uint_fast8_t res = vd + vr;

	if (r == d) {
		STATE("lsl %s[%02x] = %02x\n", avr_regname(d), vd, res & 0xff);
	} else {
		STATE("add %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
	}

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_add_zns(avr, res, vd, vr);

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
COMBINING_INSTd5r5(add)
	INST_GET_D5(d5b, next_opcode);
	INST_GET_R5(r5b, next_opcode);

	if( (0x0c00 /* ADD */ == (i_opcode & 0xfc00)) && (0x1c00 /* ADDC */ == (next_opcode & 0xfc00))
			&& ((d5 + 1) == d5b) && ((r5 + 1) == r5b) ) {
		if(d5 == r5)
			u_opcode = OPCODE(d5_lsl_rol, d5, 0, 0);
		else
			u_opcode = OPCODE(d5r5_add_adc, d5, r5, 0);
	} else if (d5 == r5) {
		int count = 1;
		while((7 > count) && (i_opcode == next_opcode)) {
			new_pc += 2;
			count++;
			I_OPCODE_FETCH(next_opcode, new_pc);
		}
		
		if(1 < count) {
			u_opcode = OPCODE(d5r5_lsl_lsl, d5, count, 0);
		} else {
			u_opcode = OPCODE(d5_lsl, d5, 0, 0);
		}
	} else
		combining = 0;
END_COMBINING

UINSTd5r5(add_adc) {
	NO_RMW(UINST_GET16le_VD_VR());
	RMW(UINST_RMW16le_VD_VR());
	uint_fast16_t res = vd + vr;

	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vdl = vd & 0xff; uint8_t vrl = vr & 0xff);
	T(uint8_t res0 = vdl + vrl);
	STATE("add %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdl, avr_regname(r), vrl, res0);
	T(_avr_flags_add_zns(avr, res0, vdl, vrl));
	SREG();
#else
//	STATE("/ add %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdl, avr_regname(r), vrl, res0);
	STATE("add.adc %s:%s[%04x], %s:%s[%04x] = %04x\n", avr_regname(d), avr_regname(d + 1), vd, 
		avr_regname(r), avr_regname(r + 1), vr, res);
#endif

	T(STEP_PC_CYCLES(2, 1));

	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vdh = vd >> 8; uint8_t vrh = vr >> 8);
	T(uint8_t res1 = vdh + vrh + avr->sreg[S_C]);
	STATE("addc %s[%02x], %s[%02x] = %02x\n", avr_regname(d + 1), vdh, avr_regname(r + 1), vrh, res1);
#else
//	STATE("\\ addc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdh, avr_regname(r), vrh, res1);
#endif

	NO_RMW(_avr_set_r16le(avr, d, res));
	RMW(_avr_rmw_write16le(pvd, res));


/*	NOTE: about flag behavior...
	certain flags, despite logic where concerned with the combination of
		add + adc and other combinations only pertain to the data relevant
		to the adc instruction, if handled as and entire quantity produces
		incorrect results...  this is taken into accunt in the 
		_avr_flags_add16_zns16 function...
		
		prior implimentation was:
			_avr_flags_add16(avr, res, vd, vr);
			_avr_flags_zns16(avr, res & 0xff00);
		
		in particular it was found that zero flag was incorrectly set. */
		
	_avr_flags_add16_zns16(avr, res, vd, vr);

	SREG();

	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

UINSTp2k6(adiw) {
	UINST_GET_p2k6();
	NO_RMW(UINST_GET_VP16le());
	RMW(UINST_RMW_VP16le());
	uint_fast16_t res = vp + k;

#ifndef CORE_FAST_CORE_DIFF_TRACE
	STATE("adiw %s:%s[%04x], 0x%02x = 0x%04x\n", avr_regname(p), avr_regname(p+1), vp, k, res);
#else
	STATE("adiw %s:%s[%04x], 0x%02x\n", avr_regname(p), avr_regname(p+1), vp, k);
#endif

	NO_RMW(_avr_set_r16le(avr, p, res));
	RMW(_avr_rmw_write16le(pvp, res));

	avr->sreg[S_V] = (((~vp) & res) >> 15) & 1;
	avr->sreg[S_C] = (((~res) & vp) >> 15) & 1;

	_avr_flags_zns16(avr, res);

	SREG();

	UINST_NEXT_PC_CYCLES(2, 2);
}
INSTp2k6(adiw)

UINSTd5r5(and) {
	NO_RMW(UINST_GET_VD_VR());
	RMW(UINST_RMW_VD_VR());
	uint_fast8_t res = vd & vr;

	if (r == d) {
		STATE("tst %s[%02x]\n", avr_regname(d), vd);
	} else {
		STATE("and %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
	}

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));
	
	_avr_flags_znv0s(avr, res);

	SREG();
	
	UINST_NEXT_PC_CYCLES(2, 1);
}
COMPLEX_INSTd5r5(and)
	if(d5 == r5)
		u_opcode = OPCODE(d5_tst, d5, 0, 0);
END_COMPLEX

UINSTh4k8(andi) {
	UINST_GET_h4k8();
	NO_RMW(UINST_GET_VH());
	RMW(UINST_RMW_VH());
	uint_fast8_t res = vh & k;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("andi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
#else
	STATE("andi %s[%02x], 0x%02x = 0x%02x\n", avr_regname(h), vh, k, res);
#endif

	NO_RMW(_avr_set_r(avr, h, res));
	RMW(_avr_set_rmw(pvh, res));

	_avr_flags_znv0s(avr, res);

	SREG();
	
	UINST_NEXT_PC_CYCLES(2, 1);
}
COMBINING_INSTh4k8(andi)
	INST_GET_H4(h4b, next_opcode);
	INST_GET_D5(d5, next_opcode);
	
	if( (0x7000 == (i_opcode & 0xf000)) && ( 0x7000 == (next_opcode & 0xf000))
			&& ((h4 + 1) == h4b) ) {
		INST_GET_K8(k8b, next_opcode);
		u_opcode = OPCODE(h4k16_andi_andi, h4, k8, k8b);
	} else if( (0x7000 /* ANDI */ == (i_opcode & 0xf000)) && ( 0x2800 == (next_opcode /* OR */ & 0xfc00))
			&& (h4 == d5) ) {
		INST_GET_R5(r5, next_opcode);
		u_opcode = OPCODE(h4r5k8_andi_or, h4, r5, k8);
	} else if( (0x7000 /* ANDI */ == (i_opcode & 0xf000)) && ( 0x6000 == (next_opcode /* ORI */ & 0xf000))
			&& (h4 == h4b) ) {
		INST_GET_K8(k8b, next_opcode);
		u_opcode = OPCODE(h4k8k8_andi_ori, h4, k8, k8b);
	} else

		combining = 0;
END_COMBINING

UINSTh4k16(andi_andi) {
	UINST_GET_h4k16();
	NO_RMW(UINST_GET_VH16le());
	RMW(UINST_RMW_VH16le());
	uint_fast16_t res = vh & k;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("andi %s[%02x], 0x%02x\n", avr_regname(h), vh & 0xff, k & 0xff);
#else
	STATE("andi %s:%s[%04x], 0x%04x\n", avr_regname(h), avr_regname(h + 1), vh, k);
#endif

	T(STEP_PC_CYCLES(2, 1));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("andi %s[%02x], 0x%02x\n", avr_regname(h + 1), vh >> 8, k >> 8);
#endif

	NO_RMW(_avr_set_r16le(avr, h, res));
	RMW(_avr_rmw_write16le(pvh, res));
	
	_avr_flags_znv0s16(avr, res);

	SREG();
	
	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

UINSTh4r5k8(andi_or) {
	UINST_GET_h4r5k8();
	NO_RMW(UINST_GET_VH());
	RMW(UINST_RMW_VH());
	uint_fast8_t andi_res = vh & k;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("andi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
#else
	STATE("/ andi %s[%02x], 0x%02x = 0x%02x\n", avr_regname(h), vh, k, andi_res);
#endif

	T(_avr_flags_znv0s(avr, andi_res));
	SREG();

	T(STEP_PC_CYCLES(2, 1));

	UINST_GET_VR();
	uint_fast8_t res = andi_res | vr;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("or %s[%02x], %s[%02x] = %02x\n", avr_regname(h), andi_res, avr_regname(r), vr, res);
#else
	STATE("\\ or %s[%02x], %s[%02x] = %02x\n", avr_regname(h), andi_res, avr_regname(r), vr, res);
#endif

	NO_RMW(_avr_set_r(avr, h, res));
	RMW(_avr_set_rmw(pvh, res));

	_avr_flags_znv0s(avr, res);

	SREG();

	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

UINSTh4k8k8(andi_ori) {
	UINST_GET_h4k8k8();
	NO_RMW(UINST_GET_VH());
	RMW(UINST_RMW_VH());
	uint_fast8_t andi_res = vh & k1;
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("andi %s[%02x], 0x%02x\n", avr_regname(h), vh, k1);
#else
	STATE("/ andi %s[%02x], 0x%02x = 0x%02x\n", avr_regname(h), vh, k1, andi_res);
#endif

	T(_avr_flags_znv0s(avr, andi_res));
	SREG();

	T(STEP_PC_CYCLES(2, 1));
	uint_fast8_t res = andi_res | k2;
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("ori %s[%02x], 0x%02x\n", avr_regname(h), andi_res, k2);
#else
	STATE("\\ ori %s[%02x], 0x%02x = 0x%02x\n", avr_regname(h), andi_res, k2, res);
#endif

	NO_RMW(_avr_set_r(avr, h, res));
	RMW(_avr_set_rmw(pvh, res));

	_avr_flags_znv0s(avr, res);

	SREG();

	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

UINSTd5(asr) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	
	uint_fast8_t neg = vd & 0x80;
	uint_fast8_t res = (vd >> 1) | neg;
	
	STATE("asr %s[%02x]\n", avr_regname(d), vd);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));
	
	neg >>= 7;
	
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vd & 1;
	avr->sreg[S_N] = neg;
	avr->sreg[S_V] = neg ^ avr->sreg[S_C];
	avr->sreg[S_S] = neg ^ avr->sreg[S_V];

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5(asr)

UINSTb3(bclr) {
	UINST_GET_R1(b, u_opcode);
	avr->sreg[b]=0;

	if(S_I == b)
		*count = 0;
	
	STATE("cl%c\n", _sreg_bit_name[b]);
	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTb3(bclr)

UINSTd5m8(bld) {
	UINST_GET_d5m8();
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	uint_fast8_t res = (vd & ~(mask)) | (avr->sreg[S_T] * (mask));
#else
	uint_fast8_t res = (vd & ~(mask)) | (avr->sreg[S_T] ? (mask) : 0);
#endif

	STATE("bld %s[%02x], 0x%02x = %02x\n", avr_regname(d), vd, mask, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5m8(bld)

UINSTo7(brcc) {
	UINST_GET_iR2(o, u_opcode);

	int branch = (0 == avr->sreg[S_C]);
	avr_flashaddr_t new_pc = 2 + avr->pc;
	
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	avr_flashaddr_t branch_pc = new_pc + (o * branch);
#else
	avr_flashaddr_t branch_pc = new_pc + (branch ? o : 0);
#endif

	STATE("brcc .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}

UINSTo7(brcs) {
	UINST_GET_iR2(o, u_opcode);

	int branch = (0 != avr->sreg[S_C]);
	avr_flashaddr_t new_pc = 2 + avr->pc;
	
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	avr_flashaddr_t branch_pc = new_pc + (o * branch);
#else
	avr_flashaddr_t branch_pc = new_pc + (branch ? o : 0);
#endif

	STATE("brcs .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}

UINSTo7(breq) {
	UINST_GET_iR2(o, u_opcode);

	int branch = (0 != avr->sreg[S_Z]);
	avr_flashaddr_t new_pc = 2 + avr->pc;
	
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	avr_flashaddr_t branch_pc = new_pc + (o * branch);
#else
	avr_flashaddr_t branch_pc = new_pc + (branch ? o : 0);
#endif

	STATE("breq .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}

UINSTo7(brne) {
	UINST_GET_iR2(o, u_opcode);

	int branch = (0 == avr->sreg[S_Z]);
	avr_flashaddr_t new_pc = 2 + avr->pc;
	
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	avr_flashaddr_t branch_pc = new_pc + (o * branch);
#else
	avr_flashaddr_t branch_pc = new_pc + (branch ? o : 0);
#endif

	STATE("brne .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}

UINSTo7(brpl) {
	UINST_GET_iR2(o, u_opcode);

	int branch = (0 == avr->sreg[S_N]);
	avr_flashaddr_t new_pc = 2 + avr->pc;
	
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	avr_flashaddr_t branch_pc = new_pc + (o * branch);
#else
	avr_flashaddr_t branch_pc = new_pc + (branch ? o : 0);
#endif

	STATE("brpl .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}

UINSTo7b3(brxc) {
	UINST_GET_o7b3();

	int branch = (0 == avr->sreg[b]);
	avr_flashaddr_t new_pc = 2 + avr->pc;

#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	avr_flashaddr_t branch_pc = new_pc + (o * branch);
#else
	avr_flashaddr_t branch_pc = new_pc + (branch ? o : 0);
#endif
	const char *names[8] = {
		"brcc", "brne", "brpl", "brvc", NULL, "brhc", "brtc", "brid"
	};

	if (names[b]) {
		STATE("%s .%d [%04x]\t; Will%s branch\n", names[b], o >> 1, new_pc + o, branch ? "":" not");
	} else {
		STATE("brbc%c .%d [%04x]\t; Will%s branch\n", _sreg_bit_name[b], o >> 1, new_pc + o, branch ? "":" not");
	}

	UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}
COMPLEX_INSTo7b3(brxc)
	if(S_C == b)
		u_opcode = OPCODE(o7_brcc, 0, o, 0);
	else if(S_N == b)
		u_opcode = OPCODE(o7_brpl, 0, o, 0);
	else if(S_Z == b)
		u_opcode = OPCODE(o7_brne, 0, o, 0);
END_COMPLEX

UINSTo7b3(brxs) {
	UINST_GET_o7b3();

	int branch = (0 != avr->sreg[b]);
	avr_flashaddr_t new_pc = 2 + avr->pc;

#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	avr_flashaddr_t branch_pc = new_pc + (o * branch);
#else
	avr_flashaddr_t branch_pc = new_pc + (branch ? o : 0);
#endif

	const char *names[8] = {
		"brcs", "breq", "brmi", "brvs", NULL, "brhs", "brts", "brie"
	};
	if (names[b]) {
		STATE("%s .%d [%04x]\t; Will%s branch\n", names[b], o >> 1, new_pc + o, branch ? "":" not");
	} else {
		STATE("brbs%c .%d [%04x]\t; Will%s branch\n", _sreg_bit_name[b], o >> 1, new_pc + o, branch ? "":" not");
	}

	UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch);
}
COMPLEX_INSTo7b3(brxs)
	if(S_C == b)
		u_opcode = OPCODE(o7_brcs, 0, o, 0);
	else if(S_Z == b)
		u_opcode = OPCODE(o7_breq, 0, o, 0);
END_COMPLEX

UINSTb3(bset) {
	UINST_GET_R1(b, u_opcode);
	avr->sreg[b]=1;

	/* 
		avr instruction manual states there shall be one cycle latency
		after issuing an sei instruction. */

	if(S_I == b) {
		*count = 1;
	}

	STATE("se%c\n", _sreg_bit_name[b]);
	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
COMBINING_INSTb3(bset)
	if(0x9588 == next_opcode) {
		u_opcode = OPCODE(sei_sleep, 0, 0, 0);
	} else if(S_I == b3) {
		u_opcode = OPCODE(sei, 0, 0, 0);
		combining = 0;
	} else
		combining = 0;
END_COMBINING

UINSTd5b3(bst) {
	UINST_GET_R1(d, u_opcode);
	UINST_GET_R2(b, u_opcode);
	UINST_GET_VD();
	uint_fast8_t res = (vd >> b) & 1;

	STATE("bst %s[%02x], 0x%02x = %02x\n", avr_regname(d), vd, (1 << b), res);
	
	avr->sreg[S_T] = res;

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5b3(bst)

UINSTx22(call) {
	UINST_GET_X24(x22, u_opcode);

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("call 0x%06x\n", x22 >> 1);
#else
	STATE("call 0x%06x\n", x22);
#endif

	_avr_push16be(avr, count, 2 + (avr->pc >> 1));

	TRACE_JUMP();
	STACK_FRAME_PUSH();

	UINST_NEXT_JMP_PC_CYCLES(x22, 4); // 4 cycles; FIXME 5 on devices with 22 bit PC
}
INSTx22(call)

UINSTa5m8(cbi) {
	UINST_GET_a5m8();
	UINST_GET_VIO();
	uint_fast8_t res = vio & ~(mask);

	STATE("cbi %s[%04x], 0x%02x = %02x\n", avr_regname(io), vio, mask, res);

	_avr_reg_io_write(avr, count, io, res);

	UINST_NEXT_PC_CYCLES(2, 2);
}
COMPLEX_INSTa5m8(cbi)
	if((R_SREG == a5) && (S_I == mask)) {
		u_opcode = OPCODE(cli, 0, 0, 0);
	}
END_COMPLEX

UINST(cli) {
	T(uint_fast8_t io = R_SREG);
	uint_fast8_t mask = S_I;

	T(UINST_GET_VIO());
	T(uint_fast8_t res = vio & ~(mask));
	
	STATE("cbi %s[%04x], 0x%02x = %02x\n", avr_regname(io), vio, mask, res);

	avr->sreg[mask]=0;
	
	*count = 0;
	CLI(avr);
	
	UINST_NEXT_PC_CYCLES(2, 2);
}

UINSTd5(clr) {
	UINST_GET_R1(d, u_opcode);
	T(UINST_GET_VD());
	STATE("clr %s[%02x]\n", avr_regname(d), vd);

	_avr_set_r(avr, d, 0);

	avr->sreg[S_N] = 0;
	avr->sreg[S_S] = 0;
	avr->sreg[S_V] = 0;
	avr->sreg[S_Z] = 1;
	
	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}

UINSTd5(com) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = 0xff - vd;

	STATE("com %s[%02x] = %02x\n", avr_regname(d), vd, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	avr->sreg[S_C] = 1;

	_avr_flags_znv0s(avr, res);

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5(com)

UINSTd5r5(cp) {
	UINST_GET_VD_VR();
	uint_fast8_t res = vd - vr;

	STATE("cp %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_flags_sub_zns(avr, res, vd, vr);

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
COMBINING_INSTd5r5(cp)
	INST_GET_D5(d5b, next_opcode);
	INST_GET_R5(r5b, next_opcode);

	if( (0x1400 /* CP.lh */ == (i_opcode & 0xfc00)) && (0x0400 /* CPC.lh */ == (next_opcode & 0xfc00))
			&& ((d5 + 1) == d5b) && ((r5 + 1) == r5b) ) {
		I_FETCH_OPCODE(next_next_opcode, 2 + new_pc);
		if(0xf401 == (next_next_opcode & 0xfc07)) {
			INST_GET_O7(o7, next_next_opcode);
			u_opcode = OPCODE(d5r5o7_cp_cpc_brne, d5, r5, o7);
		} else {
			u_opcode = OPCODE(d5r5_cp_cpc, d5, r5, 0);
		}
	} else
		combining = 0;
END_COMBINING

UINSTd5r5(cp_cpc) {
	UINST_GET16le_VD_VR();
	uint_fast16_t res = vd - vr;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vdl = vd & 0xff; uint8_t vrl = vr & 0xff);
	T(uint8_t res0 = vdl  - vrl);
	STATE("cp %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdl, avr_regname(r), vrl, res0);
	T(_avr_flags_sub_zns(avr, res0, vdl, vrl));
	T(SREG());
#else
	STATE("cp.cpc %s:%s[%04x], %s:%s[%04x] = %04x\n", avr_regname(d), avr_regname(d + 1), vd, 
		avr_regname(r), avr_regname(r + 1), vr, res);
#endif

	T(STEP_PC_CYCLES(2, 1));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vdh = (vd >> 8) & 0xff; uint8_t vrh = (vr >> 8) & 0xff);
	T(uint8_t res1 = vdh  - vrh);
	STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(d + 1), vdh, avr_regname(r + 1), vrh, res1);
#endif

	_avr_flags_sub16_zns16(avr, res, vd, vr);

	SREG();

	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

eUINSTd5r5o7(cp_cpc_brne) {
	UINST_GET16le_VD_VR();
	uint_fast16_t res = vd - vr;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vdl = vd & 0xff; uint8_t vrl = vr & 0xff);
	T(uint8_t res0 = vdl  - vrl);
	STATE("cp %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdl, avr_regname(r), vrl, res0);
	T(_avr_flags_sub_zns(avr, res0, vdl, vrl));
	T(SREG());
#else
	STATE("cp.cpc %s:%s[%04x], %s:%s[%04x] = %04x\n", avr_regname(d), avr_regname(d + 1), vd, 
		avr_regname(r), avr_regname(r + 1), vr, res);
#endif

	T(STEP_PC_CYCLES(2, 1));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vdh = (vd >> 8) & 0xff; uint8_t vrh = (vr >> 8) & 0xff);
	T(uint8_t res1 = vdh  - vrh);
	STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(d + 1), vdh, avr_regname(r + 1), vrh, res1);
#endif

	_avr_flags_sub16_zns16(avr, res, vd, vr);

	SREG();

	T(STEP_PC_CYCLES(2, 1));
	UINST_GET_iR3(o, u_opcode);

	int branch = (0 != res);
		
	T(avr_flashaddr_t new_pc = 2 + avr->pc);
	NO_T(avr_flashaddr_t new_pc = 2 + 2 + 2 + avr->pc);
	
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	avr_flashaddr_t branch_pc = new_pc + (o * branch);
#else
	avr_flashaddr_t branch_pc = new_pc + (branch ? o : 0);
#endif

	STATE("brne .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	T(UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch));
	NO_T(UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + 1 + 1 + branch));
}

UINSTd5r5(cpc) {
	UINST_GET_VD_VR();
	uint_fast8_t res = vd - vr - avr->sreg[S_C];

	STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_flags_sub_Rzns(avr, res, vd, vr);

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5r5(cpc)

UINSTh4k8(cpi) {
	UINST_GET_h4k8();
	UINST_GET_VH();
	uint_fast8_t res = vh - k;

#ifndef CORE_FAST_CORE_DIFF_TRACE
	STATE("cpi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);
#else
	STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
#endif

	_avr_flags_sub_zns(avr, res, vh, k);

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
COMBINING_INSTh4k8(cpi)
	INST_GET_D5(d5, next_opcode);

#ifdef FAST_CORE_USE_CPI_BRXX
	if( (0x3000 /* CPI */ == (i_opcode & 0xf000)) && (0xf000 /* BRX(S/C) */ == (next_opcode & 0xf800)) ) {
		INST_GET_O7(o7, next_opcode);
		switch(next_opcode & 0x0407) {
			case	S_C:
				u_opcode = OPCODE(h4k8o7_cpi_brcs, h4, k8, o7);
				break;
			case	S_Z:
				u_opcode = OPCODE(h4k8o7_cpi_breq, h4, k8, o7);
				break;
			case	(0x400 | S_C):
				u_opcode = OPCODE(h4k8o7_cpi_brcc, h4, k8, o7);
				break;
			case	(0x400 | S_Z):
				u_opcode = OPCODE(h4k8o7_cpi_brne, h4, k8, o7);
				break;
			default:
				combining = 0;
				break;
		}
	} else 
#endif
		if( (0x3000 /* CPI.l */ == (i_opcode & 0xf000)) && (0x0400 /* CPC.h */ == (next_opcode & 0xfc00))
			&& ((h4 + 1) == d5) ) {
		INST_GET_R5(r5, next_opcode);
		u_opcode = OPCODE(h4r5k8_cpi_cpc, h4, r5, k8);
	} else
		combining = 0;
END_COMBINING

#define DO_CPI_BRXX_BRANCH(branch_op_string) \
	avr_uinst_do_cpi_brxx_branch(avr, count, u_opcode, o, branch, branch_op_string)
static void avr_uinst_do_cpi_brxx_branch(avr_t * avr, int_fast32_t * count, uint_fast32_t u_opcode,
	uint_fast16_t o, int branch, const char *branch_op_string);

#if defined(FAST_CORE_USE_CPI_BRXX) || defined(FAST_CORE_CPI_BRXX_CODE)
UINSTh4k8o7(cpi_brcc) {
	UINST_GET_h4k8o7();
	UINST_GET_VH();
	uint_fast8_t res = vh - k;

#ifndef CORE_FAST_CORE_DIFF_TRACE
	STATE("cpi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);
#else
	STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
#endif

	_avr_flags_sub_zns(avr, res, vh, k);

	int branch = (0 == avr->sreg[S_C]);

	SREG();

	T(STEP_PC_CYCLES(2, 1));

#ifdef FAST_CORE_CPI_BRXX_COMMON_BRANCH_CODE
	DO_CPI_BRXX_BRANCH("brcc");
#else
	T(avr_flashaddr_t new_pc = 2 + avr->pc);
	NO_T(avr_flashaddr_t new_pc = 2 + 2 + avr->pc);

#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	avr_flashaddr_t branch_pc = new_pc + (o * branch);
#else
	avr_flashaddr_t branch_pc = new_pc + (branch ? o : 0);
#endif

	STATE("brcc .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	T(UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch));
	NO_T(UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + 1 + branch));
#endif
}

UINSTh4k8o7(cpi_brcs) {
	UINST_GET_h4k8o7();
	UINST_GET_VH();
	uint_fast8_t res = vh - k;
	
#ifndef CORE_FAST_CORE_DIFF_TRACE
	STATE("cpi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);
#else
	STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
#endif

	_avr_flags_sub_zns(avr, res, vh, k);

	int branch = (vh < k);

	SREG();

	T(STEP_PC_CYCLES(2, 1));

#ifdef FAST_CORE_CPI_BRXX_COMMON_BRANCH_CODE
	DO_CPI_BRXX_BRANCH("brcs");
#else
	T(avr_flashaddr_t new_pc = 2 + avr->pc);
	NO_T(avr_flashaddr_t new_pc = 2 + 2 + avr->pc);

#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	avr_flashaddr_t branch_pc = new_pc + (o * branch);
#else
	avr_flashaddr_t branch_pc = new_pc + (branch ? o : 0);
#endif

	STATE("brcs .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	T(UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch));
	NO_T(UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + 1 + branch));
#endif
}

static void avr_uinst_do_cpi_brxx_branch(avr_t * avr, int_fast32_t * count,
		uint_fast32_t u_opcode, uint_fast16_t o, int branch,
			const char *branch_op_string) {
	T(avr_flashaddr_t new_pc = 2 + avr->pc);
	NO_T(avr_flashaddr_t new_pc = 2 + 2 + avr->pc);

#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	avr_flashaddr_t branch_pc = new_pc + (o * branch);
#else
	avr_flashaddr_t branch_pc = new_pc + (branch ? o : 0);
#endif

	STATE("%s .%d [%04x]\t; Will%s branch\n", branch_op_string, o >> 1,
		new_pc + o, branch ? "":" not");

	T(UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch));
	NO_T(UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + 1 + branch));

}

UINSTh4k8o7(cpi_breq) {
	UINST_GET_h4k8o7();
	UINST_GET_VH();
	uint_fast8_t res = vh - k;
	
#ifndef CORE_FAST_CORE_DIFF_TRACE
	STATE("cpi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);
#else
	STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
#endif

	_avr_flags_sub_zns(avr, res, vh, k);

	int branch = (vh == k);

	SREG();

	T(STEP_PC_CYCLES(2, 1));

#ifdef FAST_CORE_CPI_BRXX_COMMON_BRANCH_CODE
	DO_CPI_BRXX_BRANCH("breq");
#else
	T(avr_flashaddr_t new_pc = 2 + avr->pc);
	NO_T(avr_flashaddr_t new_pc = 2 + 2 + avr->pc);

#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	avr_flashaddr_t branch_pc = new_pc + (o * branch);
#else
	avr_flashaddr_t branch_pc = new_pc + (branch ? o : 0);
#endif

	STATE("breq .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	T(UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch));
	NO_T(UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + 1 + branch));
#endif
}

UINSTh4k8o7(cpi_brne) {
	UINST_GET_h4k8o7();
	UINST_GET_VH();
	uint_fast8_t res = vh - k;
	int branch = (vh != k);
	
#ifndef CORE_FAST_CORE_DIFF_TRACE
	STATE("cpi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);
#else
	STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
#endif

	_avr_flags_sub_zns(avr, res, vh, k);

	SREG();

	T(STEP_PC_CYCLES(2, 1));

#ifdef FAST_CORE_CPI_BRXX_COMMON_BRANCH_CODE
	DO_CPI_BRXX_BRANCH("brne");
#else
	T(avr_flashaddr_t new_pc = 2 + avr->pc);
	NO_T(avr_flashaddr_t new_pc = 2 + 2 + avr->pc);

#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	avr_flashaddr_t branch_pc = new_pc + (o * branch);
#else
	avr_flashaddr_t branch_pc = new_pc + (branch ? o : 0);
#endif

	STATE("brne .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc + o, branch ? "":" not");

	T(UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + branch));
	NO_T(UINST_NEXT_JMP_PC_CYCLES(branch_pc, 1 + 1 + branch));
#endif
}
#endif

UINSTh4r5k8(cpi_cpc) {
	UINST_GET_h4r5k8();
	UINST_GET_VH16le();
	UINST_GET_VR();
	
	uint_fast16_t vrk = (vr << 8) | k;
	uint_fast16_t res = vh - vrk;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vhl = vh & 0xff);
	T(uint8_t res0 = vhl  - k);
	STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vhl, k);
	T(_avr_flags_sub_zns(avr, res0, vhl, k));
	T(SREG());
#else
	STATE("cpi.cpc %s:%s[%04x], 0x%04x = %04x\n", avr_regname(h), avr_regname(h + 1), vh, 
		vrk, res);
#endif

	T(STEP_PC_CYCLES(2, 1));
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vhh = (vh >> 8) & 0xff);
	T(uint8_t res1 = vhh  - vr - avr->sreg[S_C]);
	STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(h + 1), vhh, avr_regname(r), vr, res1);
#endif

	_avr_flags_sub16_zns16(avr, res, vh, vrk);

	SREG();

	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

UINSTd5r5(16_cpse) {
	UINST_GET_VD_VR();
	uint_fast8_t skip = vd == vr;

	STATE("cpse %s[%02x], %s[%02x]\t; Will%s skip\n", avr_regname(d), vd, avr_regname(r), vr, skip ? "":" not");

#ifdef FAST_CORE_SKIP_SHIFT
	UINST_NEXT_PC_CYCLES(2 << skip, 1 + skip);
#else
	if (skip) {
		UINST_NEXT_PC_CYCLES(4, 2);
	}
	UINST_NEXT_PC_CYCLES(2, 1);
#endif
}
INSTd5r5(16_cpse)

UINSTd5r5(32_cpse) {
	UINST_GET_VD_VR();
	uint_fast8_t skip = (vd == vr);
	
	STATE("cpse %s[%02x], %s[%02x]\t; Will%s skip\n", avr_regname(d), vd, avr_regname(r), vr, skip ? "":" not");

#ifdef FAST_CORE_32_SKIP_SHIFT
	int skip_count = skip ? 3 : 1;
	UINST_NEXT_PC_CYCLES(skip_count << 1, skip_count);
#else
	if (skip) {
		UINST_NEXT_PC_CYCLES(6, 3);
	}
	UINST_NEXT_PC_CYCLES(2, 1);
#endif
}
INSTd5r5(32_cpse)

UINSTd5(dec) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = vd - 1;

	STATE("dec %s[%02x] = %02x\n", avr_regname(d), vd, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	avr->sreg[S_V] = res == 0x7f;

	_avr_flags_zns(avr, res);

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5(dec)

UINST(eicall) {
	uint_fast32_t z = (_avr_get_r16le(avr, R_ZL) | avr->data[avr->eind] << 16) << 1;

	STATE("eicall Z[%04x]\n", z);

	_avr_push16be(avr, count, 1 + (avr->pc >> 1));

	TRACE_JUMP();

	UINST_NEXT_JMP_PC_CYCLES(z, 4);  // 4 cycles except 3 cycles on XMEGAs
}
INST(eicall)

UINST(eijmp) {
	uint_fast32_t z = (_avr_get_r16le(avr, R_ZL) | avr->data[avr->eind] << 16) << 1;

	STATE("eijmp Z[%04x]\n", z);

	TRACE_JUMP();
	
	UINST_NEXT_JMP_PC_CYCLES(z, 2); // ??? 3 cycles ???
}
INST(eijmp)

UINSTd5r5(eor) {
	NO_RMW(UINST_GET_VD_VR());
	RMW(UINST_RMW_VD_VR());
	uint_fast8_t res = vd ^ vr;

	if (r==d) {
		STATE("clr %s[%02x]\n", avr_regname(d), vd);
	} else {
		STATE("eor %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
	}

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_znv0s(avr, res);

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
COMPLEX_INSTd5r5(eor)
	if(d5 == r5) {
		u_opcode = OPCODE(d5_clr, d5, 0, 0);
	}
END_COMPLEX

UINST(icall) {
	uint_fast16_t z = _avr_get_r16le(avr, R_ZL) << 1;

	STATE("icall Z[%04x]\n", z);

	_avr_push16be(avr, count, 1 + (avr->pc >> 1));

	TRACE_JUMP();

	UINST_NEXT_JMP_PC_CYCLES(z, 3);
}
INST(icall)

UINST(ijmp) {
	uint_fast16_t z = _avr_get_r16le(avr, R_ZL) << 1;

	STATE("ijmp Z[%04x]\n", z);

	TRACE_JUMP();

	UINST_NEXT_JMP_PC_CYCLES(z, 2);
}
INST(ijmp)

UINSTd5a6(in) {
	UINST_GET_d5a6();

#ifdef CORE_FAST_CORE_DIFF_TRACE
	/* CORE TRACE BUG ??? */
	STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), U_VR(a));
	UINST_GET_VA();
#else
	UINST_GET_VA();
	STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);
#endif

	_avr_set_r(avr, d, va);

	UINST_NEXT_PC_CYCLES(2, 1);
}
COMBINING_INSTd5a6(in)
	INST_GET_D5(d5b, next_opcode);
	INST_GET_H4(h4, next_opcode);

	if( (0xb000 /* IN */ == (i_opcode & 0xf800)) && (0xfe00 /* SBRS */ == (next_opcode & 0xfe00))
			&& (d5 == d5b) ) {
		INST_GET_B3a(b3, next_opcode);
		u_opcode = OPCODE(d5a6m8_in_sbrs, d5, a6, (1 << b3));
	} else if( (0xb000 /* IN */ == (i_opcode & 0xf800)) && (0x920f /* PUSH */ == (0xfe0f & next_opcode))
			&& (d5 == d5b) ) {
		u_opcode = OPCODE(d5a6_in_push, d5, a6, 0);
	} else if( (0xb000 /* IN */ == (i_opcode & 0xf800)) && 
		( (0x7000 /* ANDI */ == (next_opcode & 0xf000)) || (0x6000 /* ORI */ == (next_opcode & 0xf000)))
			&& (d5 == h4)) {
		I_FETCH_OPCODE(next_next_opcode, 2 + new_pc);
		INST_GET_D5(d5c, next_next_opcode);
		INST_GET_A6(a6b, next_next_opcode);

		if( (0xb800 /* OUT */ == (next_next_opcode & 0xf800)) && (d5 == d5c) && (a6 == a6b)) {
			INST_GET_K8(k8, next_opcode);
			if( (0x7000 /* ANDI */ == (next_opcode & 0xf000)) ) {
				u_opcode = OPCODE(d5a6k8_in_andi_out, d5, a6, k8);
			} else if( (0x6000 /* ORI */ == (next_opcode & 0xf000)) ) {
				u_opcode = OPCODE(d5a6k8_in_ori_out, d5, a6, k8);
			}
		} else
			combining = 0;
	} else
		combining = 0;
END_COMBINING

UINSTd5a6k8(in_andi_out) {
	UINST_GET_d5a6k8();
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	/* CORE TRACE BUG ??? */
	STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), U_VR(a));
	UINST_GET_VA();
#else
	UINST_GET_VA();
	STATE("/ in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);
#endif

	T(STEP_PC_CYCLES(2, 1));

	/* andi h4, k8 */

	uint_fast8_t res = va & k;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("andi %s[%02x], 0x%02x\n", avr_regname(d), va, k);
#else
	STATE("| andi %s[%02x], 0x%02x = 0x%02x\n", avr_regname(d), va, k, res);
#endif

	_avr_set_r(avr, d, res);
	_avr_flags_znv0s(avr, res);

	SREG();

	T(STEP_PC_CYCLES(2, 1));

	/* out d5, a6 */

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), res);
#else
	STATE("\\ out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), res);
#endif

	_avr_reg_io_write(avr, count, a, res);

	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2 + 2, 1 + 1 + 1));
}

UINSTd5a6k8(in_ori_out) {
	UINST_GET_d5a6k8();
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	/* CORE TRACE BUG ??? */
	STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), U_VR(a));
	UINST_GET_VA();
#else
	UINST_GET_VA();
	STATE("/ in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);
#endif

	T(STEP_PC_CYCLES(2, 1));
	
	/* ori h4, k8 */

	uint_fast8_t res = va | k;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("ori %s[%02x], 0x%02x\n", avr_regname(d), va, k);
#else
	STATE("| ori %s[%02x], 0x%02x\n", avr_regname(d), va, k);
#endif

	_avr_set_r(avr, d, res);
	_avr_flags_znv0s(avr, res);

	SREG();

	T(STEP_PC_CYCLES(2, 1));

	/* out d5, a6 */

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), res);
#else
	STATE("\\ out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), res);
#endif

	_avr_reg_io_write(avr, count, a, res);

	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2 + 2, 1 + 1 + 1));
}

UINSTd5a6(in_push) {
	UINST_GET_d5a6();
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	/* CORE TRACE BUG ??? */
	STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), U_VR(a));
	UINST_GET_VA();
#else
	UINST_GET_VA();
	STATE("/ in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);
#endif

	_avr_set_r(avr, d, va);
	
	T(STEP_PC_CYCLES(2, 1));

	_avr_push8(avr, count, va);

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d), va, _avr_sp_get(avr));
#else
	STACK_STATE("\\ push %s[%02x] (@%04x)\n", avr_regname(d), va, _avr_sp_get(avr));
#endif

	T(UINST_NEXT_PC_CYCLES(2, 2));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 1 + 2));
}

UINSTd5a6m8(in_sbrs) {
	UINST_GET_d5a6m8();

#ifdef CORE_FAST_CORE_DIFF_TRACE
	/* CORE TRACE BUG ??? */
	STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), U_VR(a));
	UINST_GET_VA();
#else
	UINST_GET_VA();
	STATE("/ in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);
#endif

	_avr_set_r(avr, d, va);

	T(STEP_PC_CYCLES(2, 1));

	int	branch = (0 != (va & (mask)));
	T(avr_flashaddr_t branch_pc = 2 + avr->pc);
	NO_T(avr_flashaddr_t branch_pc = 2 + 2 + avr->pc);

	T(int cycles = 1);
	NO_T(int cycles = 1 + 1);	
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("sbrs %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), va, mask, branch ? "":" not");
#else
	STATE("\\ sbrs %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), va, mask, branch ? "":" not");
#endif

	if (branch) {
		T(int shift = _avr_is_instruction_32_bits(avr, 2 + branch_pc));
		NO_T(int shift = _avr_is_instruction_32_bits(avr, 2 + branch_pc));
#ifdef FAST_CORE_SKIP_SHIFT
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
	
	UINST_NEXT_JMP_PC_CYCLES(branch_pc, cycles);
}

UINSTd5(inc) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = vd + 1;

	STATE("inc %s[%02x] = %02x\n", avr_regname(d), vd, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	avr->sreg[S_V] = res == 0x80;

	_avr_flags_zns(avr, res);

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5(inc)

UINSTx22(jmp) {
	UINST_GET_X24(x22, u_opcode);

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("jmp 0x%06x\n", x22 >> 1);
#else
	STATE("jmp 0x%06x\n", x22);
#endif

	TRACE_JUMP();
	
	UINST_NEXT_JMP_PC_CYCLES(x22, 3);
}
INSTx22(jmp)

UINSTd5rXYZ(ld_no_op) {
	UINST_GET_d5rXYZ();
	UINST_GET_VR16le();

	uint_fast8_t ivr = _avr_get_ram(avr, count, vr);

	_avr_set_r(avr, d, ivr);
	
	STATE("ld %s, %c[%04x]\n", avr_regname(d), *avr_regname(r), vr);

	UINST_NEXT_PC_CYCLES(2, 2);
}
INSTd5rXYZ(ld_no_op)

UINSTd5rXYZ(ld_pre_dec) {
	UINST_GET_d5rXYZ();
	NO_RMW(UINST_GET_VR16le());
	RMW(UINST_RMW_VR16le());
	uint_fast8_t ivr;

	vr--;
	_avr_set_r(avr, d, ivr = _avr_get_ram(avr, count, vr));
	
	STATE("ld %s, --%c[%04x]\n", avr_regname(d), *avr_regname(r), vr);

	NO_RMW(_avr_set_r16le(avr, r, vr));
	RMW(_avr_rmw_write16le(pvr, vr));

	UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles (1 for tinyavr, except with inc/dec 2)
}
INSTd5rXYZ(ld_pre_dec)

UINSTd5rXYZ(ld_post_inc) {
	UINST_GET_d5rXYZ();
	NO_RMW(UINST_GET_VR16le());
	RMW(UINST_RMW_VR16le());
	uint_fast8_t ivr;

	_avr_set_r(avr, d, ivr = _avr_get_ram(avr, count, vr));
	vr++;
	
	STATE("ld %s, %c[%04x]++\n", avr_regname(d), *avr_regname(r), vr);

	NO_RMW(_avr_set_r16le(avr, r, vr));
	RMW(_avr_rmw_write16le(pvr, vr));

	UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles (1 for tinyavr, except with inc/dec 2)
}
INSTd5rXYZ(ld_post_inc)

UINSTd5rYZq6(ldd) {
	UINST_GET_d5rYZq6();
	UINST_GET_VR16le() + q;
	uint_fast8_t ivr;

	_avr_set_r(avr, d, ivr = _avr_get_ram(avr, count, vr));

	STATE("ld %s, (%c+%d[%04x])=[%02x]\n", avr_regname(d), *avr_regname(r), q, vr, ivr);

	UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles, 3 for tinyavr
}
COMBINING_INSTd5rYZq6(ldd)
	INST_GET_D5(d5b, next_opcode);
	INST_GET_Q6(q6b, next_opcode);

	if( ((i_opcode /* LDD.l.h */ & 0xd208) == (next_opcode /* LDD.h.l */ & 0xd208))
			&& (d5 == (d5b + 1)) && (q6 == (q6b - 1)) ) {
		u_opcode = OPCODE(d5rYZq6_ldd_ldd, d5, r, q6);
	} else
		combining=0;
END_COMBINING

UINSTd5rYZq6(ldd_ldd) {
	UINST_GET_d5rYZq6();
	UINST_GET_VR16le() + q;

	uint16_t ivr;
	
	if(likely(0xff < vr)) {
		ivr = _avr_data_read16be(avr, vr);
	} else {
		ivr = _avr_get_ram(avr, count, vr) << 8;
		ivr |= _avr_get_ram(avr, count, vr + 1);
	}
	
	_avr_set_r16le(avr, d - 1, ivr);

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("ld %s, (%c+%d[%04x])=[%02x]\n", avr_regname(d), *avr_regname(r), q, vr, ivr >> 8);
	T(STEP_PC_CYCLES(2, 2));
	STATE("ld %s, (%c+%d[%04x])=[%02x]\n", avr_regname(d), *avr_regname(r), q + 1, vr + 1, ivr & 0xff);
	UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles, 3 for tinyavr
#else
	STATE("ld.w %s:%s, (%s+%d:%d[%04x:%04x])=[%04x]\n", 
		avr_regname(d), avr_regname(d - 1), 
		avr_regname(r), q, q +1, vr, vr + 1, 
		ivr);
	UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2); // 2 cycles, 3 for tinyavr
#endif
}

UINSTh4k8(ldi) {
	UINST_GET_h4k8();
	STATE("ldi %s, 0x%02x\n", avr_regname(h), k);

	_avr_set_r(avr, h, k);
	
	UINST_NEXT_PC_CYCLES(2, 1);
}
COMBINING_INSTh4k8(ldi)
	INST_GET_H4(h4b, next_opcode);
	INST_GET_D5(d5, next_opcode);
	
	if( (0xe000 /* LDI.l */ == (i_opcode & 0xf000)) && (0xe000 /* LDI.h */ == (next_opcode & 0xf000))
			&& ((h4 + 1) == h4b) ) {
		INST_GET_K8(k8b, next_opcode);
		u_opcode = OPCODE(h4k16_ldi_ldi, h4, k8, k8b);
	} else if( (0xe000 /* LDI.h */ == (i_opcode & 0xf000)) && (0xe000 /* LDI.l */ == (next_opcode & 0xf000))
			&& (h4 == (h4b + 1)) ) {
		INST_GET_K8(k8b, next_opcode);
		u_opcode = OPCODE(h4k16_ldi_ldi, h4b, k8b, k8);
	} else if( (0xe000 /* LDI */ == (i_opcode & 0xf000)) && (0xb800 /* OUT */ == (next_opcode & 0xf800))
			&& (h4 == d5) ) {
		INST_GET_A6(a6, next_opcode);
		u_opcode = OPCODE(h4k8a6_ldi_out, h4, k8, a6);
	} else

		combining = 0;
END_COMBINING

UINSTh4k16(ldi_ldi) {
	UINST_GET_h4k16();

#ifdef CORE_FAST_CORE_DIFF_TRACE
	//  For tracing purposes, there is no easier way to get around this...
	T(I_FETCH_OPCODE(i_opcode_a, avr->pc));
	T(INST_GET_H4(h4a, i_opcode_a));
	T(INST_GET_K8(k8a, i_opcode_a));
	STATE("ldi %s, 0x%02x\n", avr_regname(h4a), k8a);
#else
	STATE("ldi.w %s:%s, 0x%04x\n", avr_regname(h), avr_regname(h+1), k);
#endif

	T(STEP_PC_CYCLES(2, 1));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(I_FETCH_OPCODE(i_opcode_b, avr->pc));
	T(INST_GET_H4(h4b, i_opcode_b));
	T(INST_GET_K8(k8b, i_opcode_b));
	STATE("ldi %s, 0x%02x\n", avr_regname(h4b), k8b);
#endif

	_avr_set_r16le(avr, h, k);

	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

UINSTh4k8a6(ldi_out) {
	UINST_GET_h4k8a6();

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("ldi %s, 0x%02x\n", avr_regname(h), k);
#else
	STATE("/ ldi %s, 0x%02x\n", avr_regname(h), k);
#endif
	_avr_set_r(avr, h, k);

	T(STEP_PC_CYCLES(2, 1));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(h), k);
#else
	STATE("\\ out %s, %s[%02x]\n", avr_regname(a), avr_regname(h), k);
#endif

	_avr_reg_io_write(avr, count, a, k);

	T(UINST_NEXT_PC_CYCLES(2, 1));	
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));	
}

UINSTd5x16(lds) {
	UINST_GET_d5x16();
	
	uint_fast8_t vd = _avr_get_ram(avr, count, x);
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_get_r(avr, d), x);
#else
	STATE("lds %s, 0x%04x[%02x]\n", avr_regname(d), x, vd);
#endif
	_avr_set_r(avr, d, vd);

	UINST_NEXT_PC_CYCLES(4, 2);	
}
COMBINING_INSTd5x16(lds)
	INST_GET_D5(d5b, next_opcode);
	INST_GET_R5(r5, next_opcode);
	I_FETCH_OPCODE(x16b, 2 + new_pc);

	if( (0x9000 /* LDS.l */ == (0xfe0f & i_opcode)) && (0x9000 /* LDS.h */ == (0xfe0f & next_opcode))
			&& ((d5 + 1) == d5b) && ((x16 + 1) == x16b) ) {
		if(0xff < x16)
			u_opcode = OPCODE(d5x16_lds_lds_no_io, d5, x16, 0);
		else u_opcode = OPCODE(d5x16_lds_lds, d5, x16, 0);
	} else if( (0x9000 /* LDS */ == (0xfe0f & i_opcode)) && (0x2000 /* TST */ == (0xfc00 & next_opcode))
			&& (d5 == d5b) && (d5 == r5) ) {
		if(0xff < x16) {
			u_opcode = OPCODE(d5x16_lds_no_io_tst, d5, x16, 0);
		} else
			u_opcode = OPCODE(d5x16_lds_tst, d5, x16, 0);
	} else {
		if(0xff < x16)
			u_opcode = OPCODE(d5x16_lds_no_io, d5, x16, 0);
	
		combining = 0;
	}
END_COMBINING

UINSTd5x16(lds_no_io) {
	UINST_GET_d5x16();

	uint_fast8_t vd = _avr_data_read(avr, x);
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	/* CORE BUG -- TRACE */
	STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_get_r(avr, d), x);
#else
	STATE("lds %s, 0x%04x[%02x]\n", avr_regname(d), x, vd);
#endif

	_avr_set_r(avr, d, vd);

	UINST_NEXT_PC_CYCLES(4, 2);	
}

UINSTd5x16(lds_lds) {
	UINST_GET_d5x16();

	/* lds low:high, sts high:low ... replicate order incase in the instance io is accessed. */

	uint_fast8_t vxl = _avr_get_ram(avr, count, x);

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_get_r(avr, d), x);
#else
	STATE("/ lds %s, 0x%04x[%02x]\n", avr_regname(d), x, vxl);
#endif

	T(STEP_PC_CYCLES(4, 2));

	uint_fast8_t vxh = _avr_get_ram(avr, count, x + 1);

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("lds %s[%02x], 0x%04x\n", avr_regname(d + 1), _avr_get_r(avr, d + 1), x + 1);
#else
	STATE("\\ lds %s, 0x%04x[%02x]\n", avr_regname(d + 1), x + 1, vxh);
#endif

	_avr_set_r16le(avr, d, (vxh << 8) | vxl);

	T(UINST_NEXT_PC_CYCLES(4, 2));
	NO_T(UINST_NEXT_PC_CYCLES(4 + 4, 2 + 2));
}

UINSTd5x16(lds_lds_no_io) {
	UINST_GET_d5x16();

	/* lds low:high, sts high:low ... replicate order incase in the instance io is accessed. */

	T(uint_fast16_t vx = _avr_data_read16le(avr, x));
	NO_T(uint_fast16_t vx = _avr_data_read16(avr, x));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_get_r(avr, d), x);
#else
	STATE("/ lds %s, 0x%04x[%02x]\n", avr_regname(d), x, vx & 0xff);
#endif

	T(STEP_PC_CYCLES(4, 2));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("lds %s[%02x], 0x%04x\n", avr_regname(d + 1), _avr_get_r(avr, d), x + 1);
#else
	STATE("\\ lds %s, 0x%04x[%02x]\n", avr_regname(d + 1), x + 1, vx > 8);
#endif

	T(_avr_set_r16le(avr, d, vx));
	NO_T(_avr_set_r16(avr, d, vx));

	T(UINST_NEXT_PC_CYCLES(4, 2));
	NO_T(UINST_NEXT_PC_CYCLES(4 + 4, 2 + 2));
}

UINSTd5x16(lds_no_io_tst) {
	UINST_GET_d5x16();

	uint_fast8_t vd = _avr_data_read(avr, x);

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_get_r(avr, d), x);
#else
	STATE("/ lds %s, 0x%04x[%02x]\n", avr_regname(d), x, vd);
#endif

	_avr_set_r(avr, d, vd);

	T(STEP_PC_CYCLES(4, 2)); // 2 cycles

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("tst %s[%02x]\n", avr_regname(d), vd);
#else
	STATE("\\ tst %s[%02x]\n", avr_regname(d), vd);
#endif
	_avr_flags_znv0s(avr, vd);

	SREG();
	
	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(4 + 2, 2 + 1));
}

UINSTd5x16(lds_tst) {
	UINST_GET_d5x16();
	
	uint_fast8_t vd = _avr_get_ram(avr, count, x);

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_get_r(avr, d), x);
#else
	STATE("/ lds %s, 0x%04x[%02x]\n", avr_regname(d), x, vd);
#endif

	_avr_set_r(avr, d, vd);

	T(STEP_PC_CYCLES(4, 2)); // 2 cycles

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("tst %s[%02x]\n", avr_regname(d), vd);
#else
	STATE("\\ tst %s[%02x]\n", avr_regname(d), vd);
#endif
	_avr_flags_znv0s(avr, vd);

	SREG();
	
	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(4 + 2, 2 + 1));
}

UINSTd5(lpm_z) {
	UINST_GET_R1(d, u_opcode);
	uint_fast16_t z = _avr_get_r16le(avr, R_ZL);

#ifdef CORE_FAST_CORE_DIFF_TRACE
	/* CORE TRACE BUG -- LPM will always indicate as Z+ */
	STATE("lpm %s, (Z[%04x]+)\n", avr_regname(d), z);
#else
	STATE("lpm %s, (Z[%04x])\n", avr_regname(d), z);
#endif

	_avr_set_r(avr, d, avr->flash[z]);
	
	UINST_NEXT_PC_CYCLES(2, 3); // 3 cycles
}
INSTd5(lpm_z)

UINSTd5(lpm_z_post_inc) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(uint_fast16_t z = _avr_get_r16le(avr, R_ZL));
	RMW(AVR_RMW(pvz); uint_fast16_t z = _avr_rmw_r16le(avr, R_ZL, &pvz));

	STATE("lpm %s, (Z[%04x]+)\n", avr_regname(d), z);

	_avr_set_r(avr, d, avr->flash[z]);

	z++;

	NO_RMW(_avr_set_r16le(avr, R_ZL, z));
	RMW(_avr_rmw_write16le(pvz, z));

	UINST_NEXT_PC_CYCLES(2, 3); // 3 cycles
}
COMBINING_INSTd5(lpm_z_post_inc)
	INST_GET_D5(d5b, next_opcode);

	if( (0x9005 /* LPM_Z+ */ == (0xfe0e & i_opcode)) && (0x920c /* ST+ */ == (0xfe0e & next_opcode))
			&& (d5 == d5b) ) {
		uint_fast8_t regs[4] = {R_ZL, 0x00, R_YL, R_XL};
		uint_fast8_t r = regs[(next_opcode & 0x000c)>>2];
		uint_fast8_t opr = next_opcode & 0x0003;
		if(opr == 1)
			u_opcode = OPCODE(d5rXYZ_lpm_z_post_inc_st_post_inc, d5, r, 0);
		else
			combining = 0;
	 } else if( ((0x9e0c /* LPM_Z+.l */ & i_opcode) == (0x9e0c /* LPM_Z+.h */ & next_opcode))
			&& ((d5 + 1) == d5b) ) {
		u_opcode = OPCODE(d5_lpm16_z_post_inc, d5, 0, 0);
	 } else
		combining = 0;
END_COMBINING

UINSTd5rXYZ(lpm_z_post_inc_st_post_inc) {
	UINST_GET_d5rXYZ();
	NO_RMW(uint_fast16_t z = _avr_get_r16le(avr, R_ZL));
	RMW(AVR_RMW(pvz); uint_fast16_t z = _avr_rmw_r16le(avr, R_ZL, &pvz));
	uint_fast8_t vd = avr->flash[z];

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("lpm %s, (Z[%04x]+)\n", avr_regname(d), z);
#else
	STATE("/ lpm %s, (Z[%04x]+)\n", avr_regname(d), z);
#endif

	_avr_set_r(avr, d, vd);
	
	z++;
	
	NO_RMW(_avr_set_r16le(avr, R_ZL, z));
	RMW(_avr_rmw_write16le(pvz, z));

	T(STEP_PC_CYCLES(2, 3)); // 3 cycles

	NO_RMW(UINST_GET_VR16le());
	RMW(UINST_RMW_VR16le());

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("st %c[%04x]++, %s[0x%02x]\n", *avr_regname(r), vr,
		avr_regname(d), vd);
#else
	STATE("\\ st %c[%04x]++, %s[0x%02x]\n", *avr_regname(r), vr,
		avr_regname(d), vd);
#endif

	_avr_set_ram(avr, count, vr, vd);
	vr++;
	
	NO_RMW(_avr_set_r16le(avr, r, vr));
	RMW(_avr_rmw_write16le(pvr, vr));

	
	T(UINST_NEXT_PC_CYCLES(2, 2)); // 2 cycles, except tinyavr
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 3 + 2)); 
}

UINSTd5(lpm16_z_post_inc) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(uint_fast16_t z = _avr_get_r16le(avr, R_ZL));
	RMW(AVR_RMW(pvz); uint_fast16_t z = _avr_rmw_r16le(avr, R_ZL, &pvz));
	uint_fast16_t vd = _avr_fetch16(avr->flash, z);
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("lpm %s, (Z[%04x]+)\n", avr_regname(d), z);
#else
	STATE("lpm.w %s:%s, (Z[%04x:%04x]+) = 0x%04x\n", avr_regname(d), avr_regname(d + 1), z, z + 1, vd);
#endif

	T(STEP_PC_CYCLES(2, 3));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("lpm %s, (Z[%04x]+)\n", avr_regname(d+1), z+1);
#endif

	_avr_set_r16(avr, d, vd);
	
	z += 2;
	
	NO_RMW(_avr_set_r16le(avr, R_ZL, z));
	RMW(_avr_rmw_write16le(pvz, z));

	T(UINST_NEXT_PC_CYCLES(2, 3));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 3 + 3));
}

UINSTd5(lsl) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = vd << 1;

	STATE("lsl %s[%02x] = %02x\n", avr_regname(d), vd, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_znvs(avr, res);
	avr->sreg[S_H] = (vd >> 3) & 1;
	avr->sreg[S_C] = (vd >> 7) & 1;

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}

UINSTd5r5(lsl_lsl) {
	UINST_GET_d5r5();
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = vd << r;

	STATE("lsl %s[%02x] = %02x\n", avr_regname(d), vd, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_znvs(avr, res);

	uint_fast8_t flags_vd = vd << (r - 1);
	avr->sreg[S_H] = (flags_vd >> 3) & 1;
	avr->sreg[S_C] = (flags_vd >> 7) & 1;

	SREG();

	/* 2 bytes/instruction, 1 cycle/instruction */ 
	UINST_NEXT_PC_CYCLES(r << 1, r);
}

UINSTd5(lsl_rol) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(UINST_GET_VD16le());
	RMW(UINST_RMW_VD16le());
	uint_fast16_t res = vd << 1;

#ifdef FAST_CORE_DIFF_TRACE
	T(uint8_t vdl = vd & 0xff; uint8_t res0 = res & 0xff);
	STATE("lsl %s[%02x] = %02x\n", avr_regname(d), vdl, res0);
	T(_avr_flags_add_zns(avr, res0, vdl, vdl));
	SREG();
#else
	STATE("lsr.w %s:%s[%04x] = [%04x]\n", avr_regname(d), avr_regname(d + 1), vd, res);
#endif

	T(STEP_PC_CYCLES(2, 1));
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("rol %s[%02x]\n", avr_regname(d), vd >> 8);
#endif

	NO_RMW(_avr_set_r16le(avr, d, res));
	RMW(_avr_rmw_write16le(pvd, res));

	/* NOTE:
		where it concerns the emulation of the combination of lsl + rol,
		setting of flags only pertains to the high order byte quantity
		produced by result of the rol instruction. */
	
	_avr_flags_znvs(avr, res >> 8);
	avr->sreg[S_H] = (vd >> 11) & 1;
	avr->sreg[S_C] = (vd >> 15) & 1;

	SREG();

	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

UINSTd5(lsr) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = vd >> 1;

	STATE("lsr %s[%02x]\n", avr_regname(d), vd);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_zcn0vs(avr, res, vd);

	SREG();
	
	UINST_NEXT_PC_CYCLES(2, 1);
}
COMBINING_INSTd5(lsr)
	INST_GET_D5(__attribute__((__unused__))d5b, next_opcode);

	if( (0x9406 /* LSR */ == (0xfe0f & i_opcode)) && (0x9407 /* ROR */ == (0xfe0f & next_opcode))
			&& (d5 == (d5b + 1)) ) {
		u_opcode = OPCODE(d5_lsr_ror, d5b, 0, 0);
	} else if(i_opcode == next_opcode) {
		int count = 1;
		do {
			new_pc += 2;
			count++;
			I_OPCODE_FETCH(next_opcode, new_pc);
		} while((7 > count) && (i_opcode == next_opcode));

 		u_opcode = OPCODE(d5r5_lsr_lsr, d5, count, 0);
	} else
		combining = 0;
END_COMBINING

UINSTd5r5(lsr_lsr) {
	UINST_GET_d5r5();
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = vd >> r;

	STATE("lsr %s[%02x]\n", avr_regname(d), vd);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_zcn0vs(avr, res, (vd >> (r - 1)));

	SREG();
	
	/* 2 bytes/instruction, 1 cycle/instruction */ 
	UINST_NEXT_PC_CYCLES(r << 1, r);
}

UINSTd5(lsr_ror) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(UINST_GET_VD16le());
	RMW(UINST_RMW_VD16le());
	uint_fast16_t res = vd >> 1;
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vdh = vd >> 8);
	T(uint8_t res0 = vdh >> 1);

	STATE("lsr %s[%02x]\n", avr_regname(d + 1), vdh);
	T(_avr_flags_zcn0vs(avr, res0, vdh));
	SREG();
#else
	STATE("lsr.w %s:%s[%04x] = [%04x]\n", avr_regname(d), avr_regname(d + 1), vd, res);
#endif

	T(STEP_PC_CYCLES(2, 1));
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vdl = vd & 0xff);
	STATE("ror %s[%02x]\n", avr_regname(d), vdl);
#endif

	NO_RMW(_avr_set_r16le(avr, d, res));
	RMW(_avr_rmw_write16le(pvd, res));

	_avr_flags_zcnvs16(avr, res, vd);

	SREG();
	
	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

UINSTd5r5(mov) {
	UINST_GET_d5r5();
	_avr_mov_r(avr, d, r);

	T(UINST_GET_VR());
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("mov %s, %s[%02x] = %02x\n", avr_regname(d), avr_regname(r), vr, vr);
#else
	STATE("mov %s, %s[%02x]\n", avr_regname(d), avr_regname(r), vr);
#endif

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5r5(mov)

UINSTd4r4(movw) {
	UINST_GET_d4r4();
	_avr_mov_r16(avr, d, r);

	T(UINST_GET_VR16le());
	STATE("movw %s:%s, %s:%s[%04x]\n", avr_regname(d), avr_regname(d+1), avr_regname(r), avr_regname(r+1), vr);

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd4r4(movw)

UINSTd5r5(mul) {
	UINST_GET_VD_VR();
	uint_least16_t res = vd * vr;

	STATE("mul %s[%02x], %s[%02x] = %04x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_set_r16le(avr, 0, res);

	_avr_flags_zc16(avr, res);

	SREG();

	UINST_NEXT_PC_CYCLES(2, 2);
}
INSTd5r5(mul)

UINSTd16r16(muls) {
	UINST_GET_d5r5();
	int_least8_t vd = _avr_get_r(avr, d);
	int_least8_t vr = _avr_get_r(avr, r);
	int_least16_t res = vr * vd;

	STATE("muls %s[%d], %s[%02x] = %d\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_set_r16le(avr, 0, res);

	_avr_flags_zc16(avr, res);

	SREG();
	UINST_NEXT_PC_CYCLES(2, 2);
}
INSTd16r16(muls)

UINSTd5(neg) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = 0x00 - vd;

	STATE("neg %s[%02x] = %02x\n", avr_regname(d), vd, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	avr->sreg[S_H] = ((res | vd) >> 3) & 1;
	avr->sreg[S_V] = res == 0x80;
	avr->sreg[S_C] = res != 0;

	_avr_flags_zns(avr, res);

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5(neg)

UINST(nop) {
	STATE("nop\n");
	UINST_NEXT_PC_CYCLES(2, 1);
}
INST(nop)

UINSTd5r5(or) {
	NO_RMW(UINST_GET_VD_VR());
	RMW(UINST_RMW_VD_VR());
	uint_fast8_t res = vd | vr;

	STATE("or %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_znv0s(avr, res);

	SREG();
	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5r5(or)

UINSTh4k8(ori) {
	UINST_GET_h4k8();
	NO_RMW(UINST_GET_VH());
	RMW(UINST_RMW_VH());
	uint_fast8_t res = vh | k;

	STATE("ori %s[%02x], 0x%02x\n", avr_regname(h), vh, k);

	NO_RMW(_avr_set_r(avr, h, res));
	RMW(_avr_set_rmw(pvh, res));

	_avr_flags_znv0s(avr, res);

	SREG();
	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTh4k8(ori)

UINSTd5a6(out) {
	UINST_GET_d5a6();
	UINST_GET_VD();

	STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), vd);

	_avr_reg_io_write(avr, count, a, vd);
	
	UINST_NEXT_PC_CYCLES(2, 1);
}
COMBINING_INSTd5a6(out)
	if((0xb800 /* OUT */ == (0xf800 & i_opcode)) && (R_SPH == a6)) {
		INST_GET_A6(__attribute__((__unused__))a6b, next_opcode);
		if((0xb800 /* OUT */ == (0xf800 & next_opcode)) && (R_SREG == a6b)) {
			I_FETCH_OPCODE(next_next_opcode, 2 + new_pc);
			INST_GET_D5(__attribute__((__unused__))d5b, next_opcode);
			INST_GET_A6(__attribute__((__unused__))a6c, next_opcode);
			if((0xb800 /* OUT */ == (0xf800 & next_next_opcode)) && (R_SPL == a6c)) {
				INST_GET_D5(__attribute__((__unused__))d5c, next_opcode);
				u_opcode = OPCODE(d5r5_out_sph_sreg_spl, d5, d5b, d5c);
			} else combining = 0;
		} else combining = 0;
	} else combining = 0;
END_COMBINING

UINSTd5r5(out_sph_sreg_spl) {
	UINST_GET_VD_VR();

	/* R_SPH */
	STATE("out %s, %s[%02x]\n", avr_regname(R_SPH), avr_regname(d), vd);

	_avr_reg_io_write(avr, count, R_SPH, vd);

	T(STEP_PC_CYCLES(2, 1));
	
	/* R_SREG */
	STATE("out %s, %s[%02x]\n", avr_regname(R_SREG), avr_regname(r), vr);

	_avr_reg_io_write(avr, count, R_SREG, vr);

	T(STEP_PC_CYCLES(2, 1));
	
	/* R_SPL */
	UINST_GET_R3(d2, u_opcode);
	uint_fast8_t vd2 = U_VR(d2);
	STATE("out %s, %s[%02x]\n", avr_regname(R_SPL), avr_regname(d2), vd2);

	_avr_reg_io_write(avr, count, R_SPL, vd2);

	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2 + 2, 1 + 1 + 1));
}

UINSTd5(pop) {
	UINST_GET_R1(d, u_opcode);
	uint_fast8_t vd = _avr_pop8(avr, count);
	_avr_set_r(avr, d, vd);

	STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d), _avr_sp_get(avr), vd);
	
	UINST_NEXT_PC_CYCLES(2, 2);
}
COMBINING_INSTd5(pop)
	INST_GET_D5(__attribute__((__unused__))d5b, next_opcode);

	if( (0x900f /* POP.h*/ == (0xfe0f & i_opcode)) && (0x900f /* POP.l */ == (0xfe0f & next_opcode))
			&& (d5 == (d5b + 1)) ) {
		u_opcode = OPCODE(d5_pop_pop16be, d5b, 0, 0);
	} else if( (0x900f /* POP.l */ == (0xfe0f & i_opcode)) && (0x900f /* POP.h */ == (0xfe0f & next_opcode))
			&& ((d5 + 1) == d5b) ) {
		u_opcode = OPCODE(d5_pop_pop16le, d5, 0, 0);
	} else if( (0x900f /* POP */ == (0xfe0f & i_opcode)) && (0xb800 /* OUT */ == (0xf800 & next_opcode))
			&& (d5 == d5b) ) {
		INST_GET_A6(a6, next_opcode);
		u_opcode = OPCODE(d5a6_pop_out, d5, a6, 0);
	} else
		combining = 0;
END_COMBINING

UINSTd5a6(pop_out) {
	UINST_GET_d5a6();
	uint_fast8_t vd = _avr_pop8(avr, count);
	_avr_set_r(avr, d, vd);


#ifdef CORE_FAST_CORE_DIFF_TRACE
	STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d), _avr_sp_get(avr), vd);
#else
	STACK_STATE("/ pop %s (@%04x)[%02x]\n", avr_regname(d), _avr_sp_get(avr), vd);
#endif

	T(STEP_PC_CYCLES(2, 2));
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), vd);
#else
	STATE("\\ out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), vd);
#endif

	_avr_reg_io_write(avr, count, a, vd);
	
	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 2 + 1));
}

UINSTd5(pop_pop16be) {
	UINST_GET_R1(d, u_opcode);
	uint_fast16_t vd = _avr_pop16be(avr, count);
	T(uint_fast16_t sp = _avr_sp_get(avr));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d + 1), sp - 1, vd >> 8);
#else
	STACK_STATE("/ pop %s (@%04x)[%02x]\n", avr_regname(d + 1), sp - 1, vd >> 8);
#endif

	T(STEP_PC_CYCLES(2, 2));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d), sp, vd & 0xff);
#else
	STACK_STATE("\\ pop %s (@%04x)[%02x]\n", avr_regname(d), sp, vd & 0xff);
#endif

	_avr_set_r16le(avr, d, vd);

	T(UINST_NEXT_PC_CYCLES(2, 2));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2));
}

UINSTd5(pop_pop16le) {
	UINST_GET_R1(d, u_opcode);
	uint_fast16_t vd = _avr_pop16le(avr, count);
	T(uint_fast16_t sp = _avr_sp_get(avr));


#ifdef CORE_FAST_CORE_DIFF_TRACE
	STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d), sp - 1, vd & 0xff);
#else
	STACK_STATE("/ pop %s (@%04x)[%02x]\n", avr_regname(d), sp - 1, vd & 0xff);
#endif

	T(STEP_PC_CYCLES(2, 2));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d + 1), sp, vd >> 8);
#else
	STACK_STATE("\\ pop %s (@%04x)[%02x]\n", avr_regname(d + 1), sp, vd >> 8);
#endif

	_avr_set_r16le(avr, d, vd);

	T(UINST_NEXT_PC_CYCLES(2, 2));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2));
}

UINSTd5(push) {
	UINST_GET_R1(d, u_opcode);
	UINST_GET_VD();
	_avr_push8(avr, count, vd);

	STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d), vd, _avr_sp_get(avr));

	UINST_NEXT_PC_CYCLES(2, 2);
}

COMBINING_INSTd5(push)
	INST_GET_D5(__attribute__((__unused__))d5b, next_opcode);

	if( (0x920f /* PUSH.l */ == (0xfe0f & i_opcode)) && (0x920f /* PUSH.h */ == (0xfe0f & next_opcode))
			&& ((d5 + 1) == d5b) ) {
		u_opcode = OPCODE(d5_push_push16be, d5, 0, 0);
	} else if( (0x920f /* PUSH.h */ == (0xfe0f & i_opcode)) && (0x920f /* PUSH.l */ == (0xfe0f & next_opcode))
			&& (d5 == (d5b + 1)) ) {
		u_opcode = OPCODE(d5_push_push16le, d5b, 0, 0);
	} else
		combining = 0;
END_COMBINING

UINSTd5(push_push16be) {
	UINST_GET_R1(d, u_opcode);
	UINST_GET_VD16le();
	_avr_push16be(avr, count, vd);
	T(uint_fast16_t sp = _avr_sp_get(avr));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d), vd & 0xff, sp + 1);
	STEP_PC_CYCLES(2, 2);
	STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d + 1), vd >> 8, sp);
	UINST_NEXT_PC_CYCLES(2, 2);
#else
	STACK_STATE("push.w %s:%s[%04x] (@%04x)\n", avr_regname(d+1), avr_regname(d), vd, sp);
	UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2)
#endif
}

UINSTd5(push_push16le) {
	UINST_GET_R1(d, u_opcode);
	UINST_GET_VD16le();
	_avr_push16le(avr, count, vd);
	T(uint_fast16_t sp = _avr_sp_get(avr));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d + 1), vd >> 8, sp + 1);
	STEP_PC_CYCLES(2, 2);
	STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d), vd & 0xff, sp);
	UINST_NEXT_PC_CYCLES(2, 2);
#else
	STACK_STATE("push.w %s:%s[%04x] (@%04x)\n", avr_regname(d+1), avr_regname(d), vd, sp);
	UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2)
#endif
}

UINSTo12(rcall) {
	UINST_GET_X16(o, u_opcode);
	avr_flashaddr_t new_pc = 2 + avr->pc;
	avr_flashaddr_t branch_pc = new_pc + (int16_t)o;

	STATE("rcall .%d [%04x]\n", (int16_t)o >> 1, branch_pc);

	_avr_push16be(avr, count, new_pc >> 1);

	// 'rcall .1' is used as a cheap "push 16 bits of room on the stack"
	if (o != 0) {
		TRACE_JUMP();
		STACK_FRAME_PUSH();
	}

	UINST_NEXT_JMP_PC_CYCLES(branch_pc, 3);
}
INSTo12(rcall)

UINST(ret) {
	STATE("ret\n");

	TRACE_JUMP();
	STACK_FRAME_POP();
	
	UINST_NEXT_JMP_PC_CYCLES(_avr_pop16be(avr, count) << 1, 4);
}
INST(ret)

UINST(reti) {
	*count = 0;
	avr->sreg[S_I] = 1;

	STATE("reti\n");

	TRACE_JUMP();
	STACK_FRAME_POP();

	UINST_NEXT_JMP_PC_CYCLES(_avr_pop16be(avr, count) << 1, 4);
}
INST(reti)

UINSTo12(rjmp) {
	UINST_GET_X16(o, u_opcode);
	avr_flashaddr_t new_pc = 2 + avr->pc;
	avr_flashaddr_t	branch_pc = new_pc + (int16_t)o;

	STATE("rjmp .%d [%04x]\n", (int16_t)o >> 1, branch_pc);

	TRACE_JUMP();

	UINST_NEXT_JMP_PC_CYCLES(branch_pc, 2);
}
INSTo12(rjmp)

UINSTd5(rol) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = (vd << 1) + avr->sreg[S_C];

	STATE("rol %s[%02x] = %02x\n", avr_regname(d), vd, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_add_zns(avr, res, vd, vd);

	SREG();
	
	UINST_NEXT_PC_CYCLES(2, 1);
}

UINSTd5(ror) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	uint_fast8_t res = (avr->sreg[S_C] * 0x80) | vd >> 1;
#else
	uint_fast8_t res = (avr->sreg[S_C] ? 0x80 : 0) | vd >> 1;
#endif

	STATE("ror %s[%02x]\n", avr_regname(d), vd);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_zcnvs(avr, res, vd);

	SREG();
	
	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5(ror)

UINSTd5r5(sbc) {
	NO_RMW(UINST_GET_VD_VR());
	RMW(UINST_RMW_VD_VR());
	uint_fast8_t res = vd - vr - avr->sreg[S_C];

	STATE("sbc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_sub_Rzns(avr, res, vd, vr);

	SREG();
	
	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5r5(sbc)

UINSTh4k8(sbci) {
	UINST_GET_h4k8();
	UINST_GET_VH();
	uint_fast8_t res = vh - k - avr->sreg[S_C];

	STATE("sbci %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);

	_avr_set_r(avr, h, res);

#ifdef CORE_FAST_CORE_DIFF_TRACE
	// CORE BUG -- standard core does not calculate H and V flags.
	avr->sreg[S_C] = (k + avr->sreg[S_C]) > vh;
	_avr_flags_Rzns(avr, res);
#else
	_avr_flags_sub_Rzns(avr, res, vh, k);
#endif

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTh4k8(sbci)

UINSTa5m8(sbi) {
	UINST_GET_a5m8();
	UINST_GET_VIO();
	uint_fast8_t res = vio | mask;

	STATE("sbi %s[%04x], 0x%02x = %02x\n", avr_regname(io), vio, mask, res);

	_avr_reg_io_write(avr, count, io, res);

	UINST_NEXT_PC_CYCLES(2, 2);
}
INSTa5m8(sbi)

UINSTa5m8(16_sbic) {
	UINST_GET_a5m8();
	UINST_GET_VIO();
	uint_fast8_t skip = (0 == (vio & mask));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("sbic %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), vio, mask, skip ? "":" not");
#else
	STATE("sbic %s[%04x], 0x%02x\t; Will%s skip\n", avr_regname(io), vio, mask, skip ? "":" not");
#endif

#ifdef FAST_CORE_SKIP_SHIFT
	UINST_NEXT_PC_CYCLES(2 << skip, 1 + skip);
#else
	if (skip) {
		UINST_NEXT_PC_CYCLES(4, 2);
	}
	UINST_NEXT_PC_CYCLES(2, 1);
#endif
}
INSTa5m8(16_sbic)

UINSTa5m8(32_sbic) {
	UINST_GET_a5m8();
	UINST_GET_VIO();
	uint_fast8_t skip = (0 == (vio & mask));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("sbic %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), vio, mask, skip ? "":" not");
#else
	STATE("sbic %s[%04x], 0x%02x\t; Will%s skip\n", avr_regname(io), vio, mask, skip ? "":" not");
#endif

#ifdef FAST_CORE_32_SKIP_SHIFT
	int skip_count = skip ? 3 : 1;
	UINST_NEXT_PC_CYCLES(skip_count << 1, skip_count);
#else
	if (skip) {
		UINST_NEXT_PC_CYCLES(6, 3);
	}
	UINST_NEXT_PC_CYCLES(2, 1);
#endif
}
INSTa5m8(32_sbic)

UINSTa5m8(16_sbis) {
	UINST_GET_a5m8();
	UINST_GET_VIO();
	uint_fast8_t skip = (0 != (vio & mask));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("sbis %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), vio, mask, skip ? "":" not");
#else
	STATE("sbis %s[%04x], 0x%02x\t; Will%s skip\n", avr_regname(io), vio, mask, skip ? "":" not");
#endif

#ifdef FAST_CORE_SKIP_SHIFT
	UINST_NEXT_PC_CYCLES(2 << skip, 1 + skip);
#else
	if (skip) {
		UINST_NEXT_PC_CYCLES(4, 2);
	}
	UINST_NEXT_PC_CYCLES(2, 1);
#endif
}
INSTa5m8(16_sbis)

UINSTa5m8(32_sbis) {
	UINST_GET_a5m8();
	UINST_GET_VIO();
	uint_fast8_t skip = (0 != (vio & mask));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("sbis %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), vio, mask, skip ? "":" not");
#else
	STATE("sbis %s[%04x], 0x%02x\t; Will%s skip\n", avr_regname(io), vio, mask, skip ? "":" not");
#endif

#ifdef FAST_CORE_32_SKIP_SHIFT
	int skip_count = skip ? 3 : 1;
	UINST_NEXT_PC_CYCLES(skip_count << 1, skip_count);
#else
	if (skip) {
		UINST_NEXT_PC_CYCLES(6, 3);
	}
	UINST_NEXT_PC_CYCLES(2, 1);
#endif
}
INSTa5m8(32_sbis)

UINSTp2k6(sbiw) {
	UINST_GET_p2k6();
	NO_RMW(UINST_GET_VP16le());
	RMW(UINST_RMW_VP16le());
	uint_fast16_t res = vp - k;

	STATE("sbiw %s:%s[%04x], 0x%02x\n", avr_regname(p), avr_regname(p+1), vp, k);

	NO_RMW(_avr_set_r16le(avr, p, res));
	RMW(_avr_rmw_write16le(pvp, res));

	avr->sreg[S_V] = ((vp & (~res)) >> 15) & 1;
	avr->sreg[S_C] = ((res & (~vp)) >> 15) & 1;

	_avr_flags_zns16(avr, res);

	SREG();

	UINST_NEXT_PC_CYCLES(2, 2);
}
INSTp2k6(sbiw)

UINSTd5m8(16_sbrc) {
	UINST_GET_d5m8();
	UINST_GET_VD();
	int	skip = (0 == (vd & (mask)));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("sbrc %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), vd, mask, skip ? "":" not");
#else
	STATE("sbrc %s[%02x], 0x%02x\t; Will%s skip\n", avr_regname(d), vd, mask, skip ? "":" not");
#endif

#ifdef FAST_CORE_SKIP_SHIFT
	UINST_NEXT_PC_CYCLES(2 << skip, 1 + skip);
#else
	if (skip) {
		UINST_NEXT_PC_CYCLES(4, 2);
	}
	UINST_NEXT_PC_CYCLES(2, 1);
#endif
}
INSTd5m8(16_sbrc)

UINSTd5m8(32_sbrc) {
	UINST_GET_d5m8();
	UINST_GET_VD();
	int	skip = (0 == (vd & (mask)));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("sbrc %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), vd, mask, skip ? "":" not");
#else
	STATE("sbrc %s[%02x], 0x%02x\t; Will%s skip\n", avr_regname(d), vd, mask, skip ? "":" not");
#endif

#ifdef FAST_CORE_32_SKIP_SHIFT
	int skip_count = skip ? 3 : 1;
	UINST_NEXT_PC_CYCLES(skip_count << 1, skip_count);
#else
	if (skip) {
		UINST_NEXT_PC_CYCLES(6, 3);
	}
	UINST_NEXT_PC_CYCLES(2, 1);
#endif
}
INSTd5m8(32_sbrc)

UINSTd5m8(16_sbrs) {
	UINST_GET_d5m8();
	UINST_GET_VD();
	int	skip = (0 != (vd & (mask)));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("sbrs %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), vd, mask, skip ? "":" not");
#else
	STATE("sbrs %s[%02x], 0x%02x\t; Will%s skip\n", avr_regname(d), vd, mask, skip ? "":" not");
#endif

#ifdef FAST_CORE_SKIP_SHIFT
	UINST_NEXT_PC_CYCLES(2 << skip, 1 + skip);
#else
	if (skip) {
		UINST_NEXT_PC_CYCLES(4, 2);
	}
	UINST_NEXT_PC_CYCLES(2, 1);
#endif
}
INSTd5m8(16_sbrs)

UINSTd5m8(32_sbrs) {
	UINST_GET_d5m8();
	UINST_GET_VD();
	int	skip = (0 != (vd & (mask)));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("sbrs %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), vd, mask, skip ? "":" not");
#else
	STATE("sbrs %s[%02x], 0x%02x\t; Will%s skip\n", avr_regname(d), vd, mask, skip ? "":" not");
#endif

#ifdef FAST_CORE_32_SKIP_SHIFT
	int skip_count = skip ? 3 : 1;
	UINST_NEXT_PC_CYCLES(skip_count << 1, skip_count);
#else
	if (skip) {
		UINST_NEXT_PC_CYCLES(6, 3);
	}
	UINST_NEXT_PC_CYCLES(2, 1);
#endif
}
INSTd5m8(32_sbrs)

UINST(sei) {
	/* 
		avr instruction manual states there shall be one cycle latency
		after issuing an sei instruction. */

	*count = 1;
	avr->sreg[S_I]=1;

	STATE("sei\n");
	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}

UINST(sei_sleep) {
	avr->sreg[S_I]=1;

	STATE("sei\n");
	SREG();

	T(STEP_PC_CYCLES(2, 1));
	
	STATE("sleep\n");
	/* Don't sleep if there are interrupts about to be serviced.
	 * Without this check, it was possible to incorrectly enter a state
	 * in which the cpu was sleeping and interrupts were disabled. For more
	 * details, see the commit message. */

	if(!avr_has_pending_interrupts(avr)) {
		avr->state = cpu_Sleeping;
#ifdef FAST_CORE_SLEEP_PREPROCESS_TIMERS_AND_INTERRUPTS
		CYCLES(1);
		T(STEP_PC_CYCLES(2, *count));
		NO_T(STEP_PC_CYCLES(2 + 2, *count));
		_avr_cycle_timer_process(avr);
		_avr_service_interrupts(avr);
	}
#else
		*count = 0;
	}

	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
#endif	
}

UINST(sleep) {
	STATE("sleep\n");
	/* Don't sleep if there are interrupts about to be serviced.
	 * Without this check, it was possible to incorrectly enter a state
	 * in which the cpu was sleeping and interrupts were disabled. For more
	 * details, see the commit message. */
	if(!avr_has_pending_interrupts(avr) || !avr->sreg[S_I]) {
		avr->state = cpu_Sleeping;
#ifdef FAST_CORE_SLEEP_PREPROCESS_TIMERS_AND_INTERRUPTS
		CYCLES(1);
		STEP_PC_CYCLES(2, *count);
		_avr_cycle_timer_process(avr);
		_avr_service_interrupts(avr);
	}
#else
		*count = 0;
	}

	UINST_NEXT_PC_CYCLES(2, 1);
#endif	
}
INST(sleep)

UINSTd5rXYZ(st_no_op) {
	UINST_GET_d5rXYZ();
	UINST_GET_VD();
	UINST_GET_VR16le();

	STATE("st %c[%04x], %s[%02x] \n", *avr_regname(r), vr, avr_regname(d), vd);

	_avr_set_ram(avr, count, vr, vd);
	
	UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles, except tinyavr
}
INSTd5rXYZ(st_no_op)

UINSTd5rXYZ(st_pre_dec) {
	UINST_GET_d5rXYZ();
	UINST_GET_VD();
	NO_RMW(UINST_GET_VR16le());
	RMW(UINST_RMW_VR16le());

	STATE("st --%c[%04x], %s[%02x] \n", *avr_regname(r), vr, avr_regname(d), vd);

	vr--;
	_avr_set_ram(avr, count, vr, vd);

	NO_RMW(_avr_set_r16le(avr, r, vr));
	RMW(_avr_rmw_write16le(pvr, vr));

	UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles, except tinyavr
}
INSTd5rXYZ(st_pre_dec)

UINSTd5rXYZ(st_post_inc) {
	UINST_GET_d5rXYZ();
	UINST_GET_VD();
	NO_RMW(UINST_GET_VR16le());
	RMW(UINST_RMW_VR16le());

	STATE("st %c[%04x]++, %s[%02x] \n", *avr_regname(r), vr, avr_regname(d), vd);

	_avr_set_ram(avr, count, vr, vd);
	vr++;
	
	NO_RMW(_avr_set_r16le(avr, r, vr));
	RMW(_avr_rmw_write16le(pvr, vr));

	UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles, except tinyavr
}
INSTd5rXYZ(st_post_inc)

UINSTd5rYZq6(std) {
	UINST_GET_d5rYZq6();
	UINST_GET_VD();
	UINST_GET_VR16le() + q;

	STATE("st (%c+%d[%04x]), %s[%02x]\n", *avr_regname(r), q, vr, avr_regname(d), vd);

	_avr_set_ram(avr, count, vr, vd);

	UINST_NEXT_PC_CYCLES(2, 2); // 2 cycles, except tinyavr
}
COMBINING_INSTd5rYZq6(std)
	INST_GET_D5(__attribute__((__unused__)) d5b, next_opcode);
	INST_GET_Q6(__attribute__((__unused__)) q6b, next_opcode);

	if( ((i_opcode /* STD.h.h */ & 0xd208) == (next_opcode /* STD.l.l */& 0xd208))
			&& (d5 == (d5b + 1)) && (q6 == (q6b + 1)) ) {
		u_opcode = OPCODE(d5rYZq6_std_std_hhll, d5b, r, q6b);
	} else	if( ((i_opcode /* STD.l.h */ & 0xd208) == (next_opcode /* STD.h.l */ & 0xd208))
			&& ((d5 + 1) == d5b) && (q6 == (q6b + 1)) ) {
		u_opcode = OPCODE(d5rYZq6_std_std_hllh, d5, r, q6b);
	} else
		combining=0;
END_COMBINING

UINSTd5rYZq6(std_std_hhll) {
	UINST_GET_d5rYZq6();
	UINST_GET_VD16le();
	UINST_GET_VR16le() + q;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("st (%c+%d[%04x]), %s[%02x]\n", *avr_regname(r), q + 1, vr + 1, avr_regname(d), vd >> 8);
#else
	STATE("st (%c+%d:%d[%04x:%04x]), %s:%s[%04x]\n", *avr_regname(r), q, q + 1, vr, vr + 1,
		avr_regname(d), avr_regname(d + 1), vd);
#endif

	T(UINST_NEXT_PC_CYCLES(2, 2)); // 2 cycles, except tinyavr

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("st (%c+%d[%04x]), %s[%02x]\n", *avr_regname(r), q, vr, avr_regname(d), vd & 0xff);
#endif

	if(likely(0xff < vr)) {
		_avr_data_write16le(avr, vr, vd);
	} else {
		_avr_set_ram(avr, count, vr + 1, vd >> 8);
		_avr_set_ram(avr, count, vr, vd & 0xff);
	}

	T(STEP_PC_CYCLES(2, 2)); // 2 cycles, except tinyavr
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2));
}

UINSTd5rYZq6(std_std_hllh) {
	UINST_GET_d5rYZq6();
	UINST_GET_VD16le();
	UINST_GET_VR16le() + q;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("st (%c+%d[%04x]), %s[%02x]\n", *avr_regname(r), q + 1, vr + 1, avr_regname(d), vd & 0xff);
#else
	STATE("st (%c+%d:%d[%04x:%04x]), %s:%s[%04x]\n", *avr_regname(r), q + 1, q , vr + 1, vr,
		avr_regname(d), avr_regname(d + 1), vd);
#endif

	T(UINST_NEXT_PC_CYCLES(2, 2)); // 2 cycles, except tinyavr

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("st (%c+%d[%04x]), %s[%02x]\n", *avr_regname(r), q, vr, avr_regname(d), vd >> 8);
#endif

	if(likely(0xff < vr)) {
		_avr_data_write16be(avr, vr, vd);
	} else {
		_avr_set_ram(avr, count, vr + 1, vd & 0xff);
		_avr_set_ram(avr, count, vr , vd >> 8);
	}

	T(STEP_PC_CYCLES(2, 2)); // 2 cycles, except tinyavr
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 2 + 2));
}

UINSTd5x16(sts) {
	UINST_GET_d5x16();
	UINST_GET_VD();

	STATE("sts 0x%04x, %s[%02x]\n", x, avr_regname(d), vd);

	_avr_set_ram(avr, count, x, vd);
		
	UINST_NEXT_PC_CYCLES(4, 2);
}
COMBINING_INSTd5x16(sts)
	INST_GET_D5(d5b, next_opcode);
	I_FETCH_OPCODE(x16b, 2 + new_pc);

	if( (0x9200 /* STS.h */ == (0xfe0f & i_opcode)) && (0x9200 /* STS.l */ == (0xfe0f & next_opcode))
			&& (d5 == (d5b + 1)) && (x16 == (x16b + 1)) ) {
		if(0xff < x16b)
			u_opcode = OPCODE(d5x16_sts_sts_no_io, d5b, x16b, 0);
		else
			u_opcode = OPCODE(d5x16_sts_sts, d5b, x16b, 0);
	} else {
		if(0xff < x16)
			u_opcode = OPCODE(d5x16_sts_no_io, d5, x16, 0);
		
		combining = 0;
	}
END_COMBINING

UINSTd5x16(sts_no_io) {
	UINST_GET_d5x16();
	UINST_GET_VD();

	STATE("sts 0x%04x, %s[%02x]\n", x, avr_regname(d), vd);

	_avr_data_write(avr, x, vd);
		
	UINST_NEXT_PC_CYCLES(4, 2);
}

UINSTd5x16(sts_sts) {
	UINST_GET_d5x16();
	UINST_GET_VD16le();

	/* lds low:high, sts high:low ... replicate order incase in the instance io is accessed. */

	uint_fast8_t vdh = vd >> 8;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("sts 0x%04x, %s[%02x]\n", x + 1, avr_regname(d), vdh);
#else
	STATE("sts.w 0x%04x:0x%04x, %s:%s[%04x]\n", x, x + 1, avr_regname(d), avr_regname(d + 1), _avr_get_r16(avr, d));
#endif

	_avr_set_ram(avr, count, x + 1, vdh);

	T(STEP_PC_CYCLES(4, 2));

	uint_fast8_t vdl = vd & 0xff;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("sts 0x%04x, %s[%02x]\n", x, avr_regname(d), vdl);
#endif

	_avr_set_ram(avr, count, x, vdl);

	T(STEP_PC_CYCLES(4, 2));
	NO_T(UINST_NEXT_PC_CYCLES(4 + 4, 2 + 2));
}

UINSTd5x16(sts_sts_no_io) {
	UINST_GET_d5x16();
	
	/* lds low:high, sts high:low ...
		normally, replicate order incase in the instance io is accessed. */

	T(UINST_GET_VD16le());
	NO_T(UINST_GET_VD());
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint_fast8_t vdh = vd >> 8);
	STATE("sts 0x%04x, %s[%02x]\n", x + 1, avr_regname(d), vdh);
#else
	STATE("sts.w 0x%04x:0x%04x, %s:%s[%04x]\n", x, x + 1, avr_regname(d), avr_regname(d + 1), vd);
#endif

	T(STEP_PC_CYCLES(4, 2));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint_fast8_t vdl = vd & 0xff);
	STATE("sts 0x%04x, %s[%02x]\n", x, avr_regname(d), vdl);
#endif

	T(_avr_data_write16le(avr, x, vd));
	NO_T(_avr_data_write16(avr, x, vd));

	T(STEP_PC_CYCLES(4, 2));
	NO_T(UINST_NEXT_PC_CYCLES(4 + 4, 2 + 2));
}

UINSTd5r5(sub) {
	NO_RMW(UINST_GET_VD_VR());
	RMW(UINST_RMW_VD_VR());
	uint_fast8_t res = vd - vr;

	STATE("sub %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_sub_zns(avr, res, vd, vr);

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5r5(sub)

UINSTh4k8(subi) {
	UINST_GET_h4k8();
	NO_RMW(UINST_GET_VH());
	RMW(UINST_RMW_VH());
	uint_fast8_t res = vh - k;

	STATE("subi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);

	NO_RMW(_avr_set_r(avr, h, res));
	RMW(_avr_set_rmw(pvh, res));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	// CORE BUG -- standard core does not calculate H and V flags.
	avr->sreg[S_C] = k > vh;
	_avr_flags_zns(avr, res);
#else
	_avr_flags_sub_zns(avr, res, vh, k);
#endif

	SREG();

	UINST_NEXT_PC_CYCLES(2, 1);
}
COMBINING_INSTh4k8(subi)
	INST_GET_H4(__attribute__((__unused__)) h4b, next_opcode);

	if( (0x5000 /* SUBI.l */ == (i_opcode & 0xf000)) && (0x4000 /* SBCI.h */ == (next_opcode & 0xf000))
			&& ((h4 + 1) == h4b) ) {
		INST_GET_K8(__attribute__((__unused__)) k8b, next_opcode);
		u_opcode = OPCODE(h4k16_subi_sbci, h4, k8, k8b);
	} else
		combining = 0;
END_COMBINING

UINSTh4k16(subi_sbci) {
	UINST_GET_h4k16();
	NO_RMW(UINST_GET_VH16le());
	RMW(UINST_RMW_VH16le());
	uint_fast16_t res = vh - k;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vhl = vh & 0xff; uint8_t vkl = k & 0xff);
	T(uint8_t res0 = vhl - vkl);
	STATE("subi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vhl, vkl, res0);

#ifdef CORE_FAST_CORE_DIFF_TRACE
	// CORE BUG -- standard core does not calculate H and V flags.
	T(avr->sreg[S_C] = vkl > vhl);
	T(_avr_flags_zns(avr, res0));
#else
	T(_avr_flags_sub_zns(avr, res0, vhl, vkl));
#endif

	SREG();

	T(STEP_PC_CYCLES(2, 1));

	T(uint8_t vhh = vh >> 8; uint8_t vkh = k >> 8);
	T(uint8_t res1 = vhh - vkh - avr->sreg[S_C]);
	STATE("sbci %s[%02x], 0x%02x = %02x\n", avr_regname(h + 1), vhh, vkh, res1);
#else
	STATE("subi.sbci %s:%s[%04x], 0x%04x = %04x\n", avr_regname(h), avr_regname(h + 1), vh, k, res);
#endif

	NO_RMW(_avr_set_r16le(avr, h, res));
	RMW(_avr_rmw_write16le(pvh, res));

#ifdef CORE_FAST_CORE_DIFF_TRACE
	// CORE BUG -- standard core does not calculate H and V flags.
	avr->sreg[S_C] = k > vh;
	_avr_flags_zns16(avr, res);
#else
	_avr_flags_sub16_zns16(avr, res, vh, k);
#endif

	SREG();

	T(UINST_NEXT_PC_CYCLES(2, 1));
	NO_T(UINST_NEXT_PC_CYCLES(2 + 2, 1 + 1));
}

UINSTd5(swap) {
	UINST_GET_R1(d, u_opcode);
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = (vd >> 4) | (vd << 4);

	STATE("swap %s[%02x] = %02x\n", avr_regname(d), vd, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	UINST_NEXT_PC_CYCLES(2, 1);
}
INSTd5(swap)

UINSTd5(tst) {
	UINST_GET_R1(d, u_opcode);
	UINST_GET_VD();

	STATE("tst %s[%02x]\n", avr_regname(d), vd);

	_avr_flags_znv0s(avr, vd);

	SREG();
	
	UINST_NEXT_PC_CYCLES(2, 1);
}

/*
 * Called when an invalid opcode is decoded
 */
static void _avr_invalid_opcode(avr_t * avr)
{
#if CONFIG_SIMAVR_TRACE
	printf( FONT_RED "*** %04x: %-25s Invalid Opcode SP=%04x O=%04x \n" FONT_DEFAULT,
			avr->pc, avr_symbol_name_for_address(avr, avr->pc),
			_avr_sp_get(avr), _avr_flash_read16le(avr, avr->pc));
#else
	AVR_LOG(avr, LOG_ERROR, FONT_RED "CORE: *** %04x: Invalid Opcode SP=%04x O=%04x \n" FONT_DEFAULT,
			avr->pc, _avr_sp_get(avr), _avr_flash_read16le(avr, avr->pc));
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
static int avr_decode_one(avr_t * avr, int_fast32_t * count)
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

	I_FETCH_OPCODE(i_opcode, avr->pc);

#ifdef FAST_CORE_DECODE_TRAP
	U_FETCH_OPCODE(u_opcode, avr->pc);
	if(unlikely(u_opcode)) {
		xSTATE("opcode trap, not handled: 0x%08x [0x%04x]\n", u_opcode, i_opcode);
	}
#endif

	switch (i_opcode & 0xf000) {
		case 0x0000: {
			switch (i_opcode) {
				case 0x0000: {	// NOP
					DO_INST(nop);
				}	break;
				default: {
					switch (i_opcode & 0xfc00) {
						case 0x0400: {	// CPC compare with carry 0000 01rd dddd rrrr
							DO_INST(d5r5_cpc);
						}	break;
						case 0x0c00: {	// ADD without carry 0000 11 rd dddd rrrr
							DO_INST(d5r5_add);
						}	break;
						case 0x0800: {	// SBC subtract with carry 0000 10rd dddd rrrr
							DO_INST(d5r5_sbc);
						}	break;
						default:
							switch (i_opcode & 0xff00) {
								case 0x0100: {	// MOVW – Copy Register Word 0000 0001 dddd rrrr
									DO_INST(d4r4_movw);
								}	break;
								case 0x0200: {	// MULS – Multiply Signed 0000 0010 dddd rrrr
									DO_INST(d16r16_muls);
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
									_avr_set_r16le(avr, 0, res);
									avr->sreg[S_C] = c;
									avr->sreg[S_Z] = res == 0;
									SREG();
									STEP_PC_CYCLES(2, 2);
								}	break;
								default: _avr_invalid_opcode(avr);
							}
					}
				}
			}
		}	break;

		case 0x1000: {
			switch (i_opcode & 0xfc00) {
				case 0x1800: {	// SUB without carry 0000 10 rd dddd rrrr
					DO_INST(d5r5_sub);
				}	break;
				case 0x1000: {	// CPSE Compare, skip if equal 0000 00 rd dddd rrrr
					if(_avr_is_instruction_32_bits(avr, new_pc))
						DO_INST(d5r5_32_cpse);
					else
						DO_INST(d5r5_16_cpse);
				}	break;
				case 0x1400: {	// CP Compare 0000 01 rd dddd rrrr
					DO_INST(d5r5_cp);
				}	break;
				case 0x1c00: {	// ADD with carry 0001 11 rd dddd rrrr
					DO_INST(d5r5_adc);
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0x2000: {
			switch (i_opcode & 0xfc00) {
				case 0x2000: {	// AND	0010 00rd dddd rrrr
					DO_INST(d5r5_and);
				}	break;
				case 0x2400: {	// EOR	0010 01rd dddd rrrr
					DO_INST(d5r5_eor);
				}	break;
				case 0x2800: {	// OR Logical OR	0010 10rd dddd rrrr
					DO_INST(d5r5_or);
				}	break;
				case 0x2c00: {	// MOV	0010 11rd dddd rrrr
					DO_INST(d5r5_mov);
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0x3000: {	// CPI 0011 KKKK dddd KKKK
			DO_INST(h4k8_cpi);
		}	break;

		case 0x4000: {	// SBCI Subtract Immediate With Carry 0101 10 kkkk dddd kkkk
			DO_INST(h4k8_sbci);
		}	break;

		case 0x5000: {	// SUB Subtract Immediate 0101 10 kkkk dddd kkkk
			DO_INST(h4k8_subi);
		}	break;

		case 0x6000: {	// ORI aka SBR	Logical AND with Immediate	0110 kkkk dddd kkkk
			DO_INST(h4k8_ori);
		}	break;

		case 0x7000: {	// ANDI	Logical AND with Immediate	0111 kkkk dddd kkkk
			DO_INST(h4k8_andi);
		}	break;

		case 0xa000:
		case 0x8000: {
			switch (i_opcode & 0xd200) {
				case 0x8000:
				case 0xa000: {	// LD (LDD) – Load Indirect using Y/Z 11q0 qq0r rrrr 0qqq
						DO_INST(d5rYZq6_ldd);
				}	break;
				case 0x8200:
				case 0xa200: {	// ST (STD) – Store Indirect using Y/Z 10q0 qqsr rrrr iqqq
						DO_INST(d5rYZq6_std);
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0x9000: {
			switch (i_opcode) {
				case 0x9588: { // SLEEP
					DO_INST(sleep);
				}	break;
				case 0x9598: { // BREAK
					STATE("break\n");
					if (avr->gdb) {
						// if gdb is on, we break here as in here
						// and we do so until gdb restores the instruction
						// that was here before
						avr->state = cpu_StepDone;
						STEP_PC_CYCLES(2, 3);
					}
				}	break;
				case 0x95a8: { // WDR
					STATE("wdr\n");
					avr_ioctl(avr, AVR_IOCTL_WATCHDOG_RESET, 0);
					STEP_PC_CYCLES(2, 1);
				}	break;
				case 0x95e8: { // SPM
					STATE("spm\n");
					avr_ioctl(avr, AVR_IOCTL_FLASH_SPM, 0);
					STEP_PC_CYCLES(2, 1);
					*count = 0;
				}	break;
				case 0x9409: {   // IJMP Indirect jump 					1001 0100 0000 1001
					DO_INST(ijmp);
				}	break;
				case 0x9419: {  // EIJMP Indirect jump 					1001 0100 0001 1001   bit 4 is "indirect"
					if (!avr->eind) {
						_avr_invalid_opcode(avr);
						STEP_PC_CYCLES(2, 1);
					} else
						DO_INST(eijmp);
				}	break;
				case 0x9509: {  // ICALL Indirect Call to Subroutine		1001 0101 0000 1001
					DO_INST(icall);
				}	break;
				case 0x9519: { // EICALL Indirect Call to Subroutine	1001 0101 0001 1001   bit 8 is "push pc"
					if (!avr->eind) {
						_avr_invalid_opcode(avr);
						STEP_PC_CYCLES(2, 1);
					} else
						DO_INST(eicall);
				}	break;
				case 0x9518: {	// RETI
					DO_INST(reti);
				}	break;
				case 0x9508: {	// RET
					DO_INST(ret);
				}	break;
				case 0x95c8: {	// LPM Load Program Memory R0 <- (Z) 1001 0101 1100 1000
					i_opcode = 0x9004;
					DO_INST(d5_lpm_z);
				}	break;
				case 0x9408:case 0x9418:case 0x9428:case 0x9438:case 0x9448:case 0x9458:case 0x9468:
				case 0x9478: // BSET 1001 0100 0ddd 1000
				{	DO_INST(b3_bset);
				}	break;
				case 0x9488:case 0x9498:case 0x94a8:case 0x94b8:case 0x94c8:case 0x94d8:case 0x94e8:
				case 0x94f8:	// bit 7 is 'clear vs set'
				{	// BCLR 1001 0100 1ddd 1000
					DO_INST(b3_bclr);
				}	break;
				default:  {
					switch (i_opcode & 0xfe0f) {
						case 0x9000: {	// LDS Load Direct from Data Space, 32 bits
							DO_INST(d5x16_lds);
						}	break;
						case 0x9005: {	// LPM Load Program Memory 1001 000d dddd 01oo
							DO_INST(d5_lpm_z_post_inc);
						}	break;
						case 0x9004: {	// LPM Load Program Memory 1001 000d dddd 01oo
							DO_INST(d5_lpm_z);
						}	break;
						case 0x9006:
						case 0x9007: {	// ELPM Extended Load Program Memory 1001 000d dddd 01oo
							if (!avr->rampz)
								_avr_invalid_opcode(avr);
							uint32_t z = _avr_get_r16le(avr, R_ZL) | (avr->data[avr->rampz] << 16);

							uint_fast8_t r = (i_opcode >> 4) & 0x1f;
							int op = i_opcode & 3;
							STATE("elpm %s, (Z[%02x:%04x]%s)\n", avr_regname(r), z >> 16, z&0xffff, i_opcode?"+":"");
							_avr_set_r(avr, r, avr->flash[z]);
							if (op == 3) {
								z++;
								_avr_set_r(avr, avr->rampz, z >> 16);
								_avr_set_r16le(avr, R_ZL, (z&0xffff));
							}
							STEP_PC_CYCLES(2, 3); // 3 cycles
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
							DO_INST(d5rXYZ_ld_no_op);
						}	break;
						case 0x920c: {	// ST Store Indirect Data Space 1001 001r rrrr iioo
							DO_INST(d5rXYZ_st_no_op);
						}	break;
						case 0x9001:
						case 0x9009:
						case 0x900d: {	// LD Load Indirect from Data  1001 00sr rrrr iioo
							DO_INST(d5rXYZ_ld_post_inc);
						}	break;
						case 0x9002:
						case 0x900a:
						case 0x900e: {	// LD Load Indirect from Data 1001 00sr rrrr iioo
							DO_INST(d5rXYZ_ld_pre_dec);
						}	break;
						case 0x9201:
						case 0x9209:
						case 0x920d:  {	// ST Store Indirect Data space 1001 000r rrrr iioo
							DO_INST(d5rXYZ_st_post_inc);
						}	break;
						case 0x9202:
						case 0x920a:
						case 0x920e: {	// ST Store Indirect Data Space 1001 001r rrrr iioo
							DO_INST(d5rXYZ_st_pre_dec);
						}	break;
						case 0x9200: {	// STS ! Store Direct to Data Space, 32 bits
							DO_INST(d5x16_sts);
						}	break;
						case 0x900f: {	// POP 1001 000d dddd 1111
							DO_INST(d5_pop);
						}	break;
						case 0x920f: {	// PUSH 1001 001d dddd 1111
							DO_INST(d5_push);
						}	break;
						case 0x9400: {	// COM – One’s Complement
							DO_INST(d5_com);
						}	break;
						case 0x9401: {	// NEG – Two’s Complement
							DO_INST(d5_neg);
						}	break;
						case 0x9402: {	// SWAP – Swap Nibbles
							DO_INST(d5_swap);
						}	break;
						case 0x9403: {	// INC – Increment
							DO_INST(d5_inc);
						}	break;
						case 0x9405: {	// ASR – Arithmetic Shift Right 1001 010d dddd 0101
							DO_INST(d5_asr);
						}	break;
						case 0x9406: {	// LSR 1001 010d dddd 0110
							DO_INST(d5_lsr);
						}	break;
						case 0x9407: {	// ROR 1001 010d dddd 0111
							DO_INST(d5_ror);
						}	break;
						case 0x940a: {	// DEC – Decrement
							DO_INST(d5_dec);
						}	break;
						case 0x940c:
						case 0x940d: {	// JMP Long Call to sub, 32 bits
							DO_INST(x22_jmp);
						}	break;
						case 0x940e:
						case 0x940f: {	// CALL Long Call to sub, 32 bits
							DO_INST(x22_call);
						}	break;

						default: {
							switch (i_opcode & 0xff00) {
								case 0x9600: {	// ADIW - Add Immediate to Word 1001 0110 KKdd KKKK
									DO_INST(p2k6_adiw);
								}	break;
								case 0x9700: {	// SBIW - Subtract Immediate from Word 1001 0110 KKdd KKKK
									DO_INST(p2k6_sbiw);
								}	break;
								case 0x9800: {	// CBI - Clear Bit in I/O Register 1001 1000 AAAA Abbb
									DO_INST(a5m8_cbi);
								}	break;
								case 0x9900: {	// SBIC - Skip if Bit in I/O Register is Cleared 1001 0111 AAAA Abbb
									if(_avr_is_instruction_32_bits(avr, new_pc))
										DO_INST(a5m8_32_sbic);
									else
										DO_INST(a5m8_16_sbic);
								}	break;
								case 0x9a00: {	// SBI - Set Bit in I/O Register 1001 1000 AAAA Abbb
									DO_INST(a5m8_sbi);
								}	break;
								case 0x9b00: {	// SBIS - Skip if Bit in I/O Register is Set 1001 1011 AAAA Abbb
									if(_avr_is_instruction_32_bits(avr, new_pc))
										DO_INST(a5m8_32_sbis);
									else
										DO_INST(a5m8_16_sbis);
								}	break;
								default:
									switch (i_opcode & 0xfc00) {
										case 0x9c00: {	// MUL - Multiply Unsigned 1001 11rd dddd rrrr
											DO_INST(d5r5_mul);
										}	break;
										default: _avr_invalid_opcode(avr);
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
					DO_INST(d5a6_out);
				}	break;
				case 0xb000: {	// IN Rd,A 1011 0AAr rrrr AAAA
					DO_INST(d5a6_in);
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;
		case 0xc000: {	// RJMP 1100 kkkk kkkk kkkk
			DO_INST(o12_rjmp);
		}	break;
		case 0xd000: {
			// RCALL 1100 kkkk kkkk kkkk
			DO_INST(o12_rcall);
		}	break;
		case 0xe000: {	// LDI Rd, K 1110 KKKK RRRR KKKK -- aka SER (LDI r, 0xff)
			DO_INST(h4k8_ldi);
		}	break;
		case 0xf000: {
			switch (i_opcode & 0xfe00) {
				case 0xf000:
				case 0xf200: {
					DO_INST(o7b3_brxs);
				}	break;
				case 0xf400:
				case 0xf600: {
					DO_INST(o7b3_brxc);
				}	break;
				case 0xf800:
				case 0xf900: {	// BLD – Bit Store from T into a Bit in Register 1111 100r rrrr 0bbb
					DO_INST(d5m8_bld);
				}	break;
				case 0xfa00:
				case 0xfb00:{	// BST – Bit Store into T from bit in Register 1111 100r rrrr 0bbb
					DO_INST(d5b3_bst);
				}	break;
				case 0xfc00: {	// SBRC – Skip if Bit in Register is Clear 1111 11sr rrrr 0bbb
					if(_avr_is_instruction_32_bits(avr, new_pc))
						DO_INST(d5m8_32_sbrc);
					else
						DO_INST(d5m8_16_sbrc);
				}	break;
				case 0xfe00: {	// SBRS – Skip if Bit in Register is Set 1111 11sr rrrr 0bbb
					if(_avr_is_instruction_32_bits(avr, new_pc))
						DO_INST(d5m8_32_sbrs);
					else
						DO_INST(d5m8_16_sbrs);
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;
		default: _avr_invalid_opcode(avr);
	}
	return(*count);
}

UINST(avr_decode_one) {
	avr_decode_one(avr, count);
}

#ifdef FAST_CORE_UINST_PROFILING
#define STR(name) #name
#undef UINST_ESAC_DEF
#define UINST_ESAC_DEF(name) STR(_avr_uinst_##name),
extern const char *uinst_op_profile_names[256] = {
	UINST_ESAC_DEF_TABLE
};
#endif

#ifdef FAST_CORE_TAIL_CALL
#undef UINST_ESAC_DEF
#define UINST_ESAC_DEF(name) _avr_uinst_##name,
static pfnInst_p uinst_op_table[256] = {
	UINST_ESAC_DEF_TABLE
};
#endif

#undef UINST_ESAC_DEF
#define UINST_ESAC_DEF(name) case k_avr_uinst_##name: PROFILE_IPS(); PROFILE(uinst[u_opcode_op], U_DO_UINST(name)); break;

static int _avr_fast_core_run_one(avr_t* avr, int_fast32_t * count) {
	if(0 < *count) {

	U_FETCH_OPCODE(u_opcode, avr->pc);
	UINST_GET_OP(u_opcode_op, u_opcode);
	PROFILE_ISEQ();

#ifdef FAST_CORE_TAIL_CALL
	pfnInst_p pfn = uinst_op_table[u_opcode_op];
		DO_PFN_UINST(u_opcode);
#else
	switch(u_opcode_op) {
		UINST_ESAC_DEF_TABLE;
		default:
			PROFILE_IPS();
			return(avr_decode_one(avr, count));
	}
#endif
	}
	return(*count);
}

extern void avr_fast_core_run_many(avr_t* avr) {
	PROFILE_START(core_loop);
	int_fast32_t count = avr_cycle_timer_process(avr);
	
	if(0 == avr->sreg[S_I]) {
/* no interrupt free run */
		if(likely(cpu_Running == avr->state)) {
			while(0 < _avr_fast_core_run_one(avr, &count))
				;

			if (avr->sreg[S_I] && !avr->i_shadow) {
				avr->interrupts.pending_wait++;
				goto interrupts_enabled;
			}

interrupts_disabled:
			_avr_cycle_timer_process(avr);
		} else
			if(cpu_Sleeping == avr->state) {
			if (avr->log)
				AVR_LOG(avr, LOG_TRACE, "simavr: sleeping with interrupts off, quitting gracefully\n");
			avr->state = cpu_Done;
			return;
		}
	} else {
/* slow(er) run with interrupt check */
		if(likely(cpu_Running == avr->state)) {
			while(0 < _avr_fast_core_run_one(avr, &count))
				;

			if(!avr->sreg[S_I] && avr->i_shadow) {
				goto interrupts_disabled;
			}
		}
		
interrupts_enabled:
		avr->i_shadow = avr->sreg[S_I];
			
		count = _avr_cycle_timer_process(avr);
		
		if(cpu_Sleeping == avr->state) {
			avr->sleep(avr, count);
			avr->cycle += (1 + count);
		}

		_avr_service_interrupts(avr);
	}
	PROFILE_STOP(core_loop);
}

extern void avr_fast_core_init(avr_t* avr) {
	/* avr program memory is 16 bits wide, byte addressed. */
	uint32_t flashsize = (avr->flashend + 1); // 2

	uint32_t uflashsize = flashsize << kUFlashSizeShift;

#if defined(FAST_CORE_IO_DISPTACH_TABLES) && !defined(FAST_CORE_USE_GLOBAL_FLASH_ACCESS)
	uflashsize += sizeof(fast_core_data_t);
#endif

	avr->flash = realloc(avr->flash, flashsize + uflashsize);
	assert(0 != avr->flash);
	
	memset(&avr->flash[flashsize], 0, uflashsize);

	PROFILE_INIT();

	PROFILE_ISEQ_FLUSH(avr_decode_one);
	PROFILE_ISEQ_FLUSH(o7b3_brxc);
	PROFILE_ISEQ_FLUSH(o7b3_brxs);
	PROFILE_ISEQ_FLUSH(x22_call);
	PROFILE_ISEQ_FLUSH(o12_rjmp);
	PROFILE_ISEQ_FLUSH(d5m8_16_sbrc);
	PROFILE_ISEQ_FLUSH(d5m8_32_sbrc);
	PROFILE_ISEQ_FLUSH(d5m8_16_sbrs);
	PROFILE_ISEQ_FLUSH(d5m8_32_sbrs);
	
#ifdef FAST_CORE_USE_GLOBAL_FLASH_ACCESS
	_uflash = AVR_FLASH_UFLASH;
#else
#ifdef FAST_CORE_IO_DISPTACH_TABLES
	AVR_FLASH_CORE_DATA->_uflash = AVR_FLASH_UFLASH + sizeof(fast_core_data_t);
#endif
#endif
}

extern avr_flashaddr_t avr_fast_core_run_one(avr_t * avr) {
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

