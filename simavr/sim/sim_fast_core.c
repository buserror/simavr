#ifndef CONFIG_SIMAVR_FAST_CORE
/* PROJECT_LOCAL_FAST_CORE
	may be set if using fast core in locally in project...  be sure to check 
	options below. */
//#define PROJECT_LOCAL_FAST_CORE
#endif


#ifdef PROJECT_LOCAL_FAST_CORE
/* if using the core compiled in the project, set these as needed. */
/* FAST_CORE_USE_GLOBAL_FLASH_ACCESS
	may be set if using in project (not in simavr library)... uses global 
	for uflash access */
#define FAST_CORE_USE_GLOBAL_FLASH_ACCESS
#ifndef FAST_CORE_USE_GLOBAL_FLASH_ACCESS
/* CONFIG_SIMAVR_FAST_CORE_PIGGYBACKED
	piggybacking, if not using global access has the benefit of letting simavr do
	our cleanup...  however may likely incurr a speed penalty, but necessary
	if not using global access...  at this time adding members to avr_t
	causes memory errors...  I have not at this time been able to track down
	the culprit. */
#define CONFIG_SIMAVR_FAST_CORE_PIGGYBACKED
#endif
#endif

#if defined(CONFIG_SIMAVR_FAST_CORE) || defined(STAND_ALONE_FAST_CORE)

#include <stdio.h>	// printf
#include <ctype.h>	// toupper
#include <stdlib.h>	// abort
#include <endian.h>	// byteorder macro stuff
#include <signal.h>	// raise
#include <string.h>	// memset
#include <assert.h>	// assert

#include "sim_avr.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "avr_flash.h"
#include "avr_watchdog.h"

/* FAST_CORE_COMBINING
	common instruction sequences are combined as well as allowing 16 bit access tricks. */
/* FAST_CORE_FAST_INTERRUPTS
	slight changes in interrupt handling behavior. */
/* FAST_CORE_BRANCH_HINTS
	via likely() and unlikely() provide the compiler (and possibly passed 
	onto the processor) hints to help reduce pipeline stalls due to 
	misspredicted branches. USE WITH CARE! :) */
/* FAST_CORE_SKIP_SHIFT
	use shifts vs comparisons and branching where shifting is less expensive
	then branching. Some processors have specialized instructions to handle
	such cases and may be faster disabled. */
/* FAST_CORE_READ_MODIFY_WRITE
	reduces redundancy inherent in register access...  and cuts back on 
	some unecessary checks. */
/* FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	for processors with fast multiply, helps reduce branches in comparisons 
	some processors may have specialized instructions making this slower */

#define FAST_CORE_COMBINING
//#define FAST_CORE_FAST_INTERRUPTS
#define FAST_CORE_BRANCH_HINTS
#define FAST_CORE_SKIP_SHIFT
#define FAST_CORE_READ_MODIFY_WRITE
#define FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY

/* fast_core_cycle_count_t
 ->	used in fast_core_run_many from avr_cycle_process
	avr_cycle_count_t uses an uint64_t datatype, however the call under normal 
	circumsances will pass data fiting withn an int32_t and likely straight 
	ints could be used here...  using 64 bit data type here seems unnessary 
	and requires extra processing on 32 bit processors...  and not all 
	implementations may handle operations on 64 bit data types properly 
	(as seems to be part of my case). */
typedef int_fast32_t fast_core_cycle_count_t;
//typedef avr_cycle_count_t fast_core_cycle_count_t;

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

#ifdef FAST_CORE_USE_GLOBAL_FLASH_ACCESS
uint32_t* _uflash = NULL;
#endif

#ifdef FAST_CORE_COMBINING
const int _FAST_CORE_COMBINING = 1;
#else
const int _FAST_CORE_COMBINING = 0;
#endif

#ifdef FAST_CORE_READ_MODIFY_WRITE
#define RMW(w) w
#define NO_RMW(w) 
#else
#define RMW(w) 
#define NO_RMW(w) w
#endif

#define CYCLES(x) { if(1==x) cycle[0]++; else cycle[0]+=(x); }
#define STEP_PC() avr->pc = new_pc[0]
#define NEW_PC(x) new_pc[0] += (x)

#define _avr_sp_get _avr_fast_core_sp_get
#define _avr_sp_set _avr_fast_core_sp_set

#ifdef FAST_CORE_BRANCH_HINTS
#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)
#else
#define likely(x) x
#define unlikely(x) x
#endif

#define xSTATE(_f, args...) { \
	printf("%06x: " _f, avr->pc, ## args);\
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

extern void avr_core_watch_write(avr_t *avr, uint16_t addr, uint8_t v);
extern uint8_t avr_core_watch_read(avr_t *avr, uint16_t addr);

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
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	return(avr->data[addr]);
}

static inline uint_fast8_t _avr_data_rmw(avr_t* avr, uint_fast16_t addr, uint8_t** ptr_reg) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(addr > avr->ramend)) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	*ptr_reg = &avr->data[addr];
	return(**ptr_reg);
}

static inline void _avr_data_write(avr_t* avr, uint_fast16_t addr, uint_fast8_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(addr > avr->ramend)) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	avr->data[addr]=data;
}

static inline void _avr_set_rmw(uint8_t* ptr_reg, uint_fast8_t data) {
	*ptr_reg = data;
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

static inline uint_fast16_t _avr_rmw_fetch16(void* p, uint_fast16_t addr, uint16_t** ptr_data) {
	*ptr_data = ((uint16_t*)&((uint8_t *)p)[addr]);
	return(**ptr_data);
}

static inline void _avr_store16(void*p, uint_fast16_t addr, uint_fast16_t data) {
	*((uint16_t *)&((uint8_t *)p)[addr])=data;
}

static inline void _avr_data_mov(avr_t* avr, uint_fast16_t dst, uint_fast16_t src) {
	avr->data[dst] = avr->data[src];
}

static inline void _avr_data_mov16(avr_t* avr, uint_fast16_t dst, uint_fast16_t src) {
	uint8_t* ptr_src = &avr->data[src];
	uint8_t* ptr_dst = &avr->data[dst];

	*ptr_dst++ = *ptr_src++;
	*ptr_dst = *ptr_src;
}

static inline uint_fast16_t _avr_data_read16(avr_t* avr, uint_fast16_t addr) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	return(_avr_fetch16(avr->data, addr));
}

static inline void _avr_data_write16(avr_t* avr, uint_fast16_t addr, uint_fast16_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	_avr_store16(avr->data, addr, data);
}

static inline uint_fast16_t _avr_data_read16be(avr_t* avr, uint_fast16_t addr) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	return(_avr_bswap16be(_avr_fetch16(avr->data, addr)));
}

static inline uint_fast16_t _avr_data_read16le(avr_t* avr, uint_fast16_t addr) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	return(_avr_bswap16le(_avr_fetch16(avr->data, addr)));
}

static inline uint_fast16_t _avr_data_rmw16le(avr_t* avr, uint_fast16_t addr, uint16_t** ptr_reg) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	return(_avr_bswap16le(_avr_rmw_fetch16(avr->data, addr, ptr_reg)));
}

static inline void _avr_data_write16be(avr_t* avr, uint_fast16_t addr, uint_fast16_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	_avr_store16(avr->data, addr, _avr_bswap16be(data));
}

static inline void _avr_data_write16le(avr_t* avr, uint_fast16_t addr, uint_fast16_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely((addr + 1) > avr->ramend)) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	_avr_store16(avr->data, addr, _avr_bswap16le(data));
}

static inline void _avr_rmw_write16le(uint16_t* ptr_data, uint_fast16_t data) {
	ptr_data[0] = _avr_bswap16le(data);
}

/*
 * Stack pointer access
 */
static inline uint_fast16_t _avr_sp_get(avr_t * avr)
{
	return(_avr_data_read16le(avr, R_SPL));
}

static inline uint_fast16_t _avr_rmw_sp(avr_t * avr, uint16_t **ptr_sp) {
	return(_avr_data_rmw16le(avr, R_SPL, ptr_sp));
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

static inline uint_fast8_t _avr_rmw_r(avr_t* avr, uint_fast8_t reg, uint8_t** reg_ptr) {
	return(_avr_data_rmw(avr, reg, reg_ptr));
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

static inline uint_fast16_t _avr_rmw_r16le(avr_t* avr, uint_fast8_t addr, uint16_t** reg_ptr) {
	return(_avr_data_rmw16le(avr, addr, reg_ptr));
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

#ifdef NO_COLOR
	#define FONT_RED		
	#define FONT_DEFAULT	
#else
	#define FONT_RED		"\e[31m"
	#define FONT_DEFAULT	"\e[0m"
#endif

/*
 * Handle "touching" registers, marking them changed.
 * This is used only for debugging purposes to be able to
 * print the effects of each instructions on registers
 */
#if CONFIG_SIMAVR_TRACE

#define T(w) w

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
#define REG_TOUCH(a, r)
#define STATE(_f, args...) xSTATE(_f, ## args)
#define SREG() xSREG()
#else
#define REG_TOUCH(a, r)
#ifdef FAST_CORE_ITRACE
extern inline uint32_t uFlashRead(avr_t* avr, avr_flashaddr_t addr);
#define T(w) w
#define STATE(_f, args...) { if(0==uFlashRead(avr, avr->pc)) xSTATE(_f, ## args) }
#else
#define T(w)
#define STATE(_f, args...)
#endif
#define SREG()
#endif
#endif

static inline void SEI(avr_t * avr) {
	avr->interrupts.pending_wait+=2;
	avr->i_shadow = avr->sreg[S_I];
}

static inline void CLI(avr_t * avr) {
	avr->interrupts.pending_wait=0;
	avr->i_shadow = avr->sreg[S_I];
}

#ifndef FAST_CORE_FAST_INTERRUPTS
#define avr_service_timers_and_interrupts(avr, cycle)
#else
static void avr_service_timers_and_interrupts(avr_t * avr, int* cycle) {
		avr_cycle_timer_pool_t * pool = &avr->cycle_timers;
		if(pool->count) {
			avr_cycle_timer_slot_t  timer = pool->timer[pool->count-1];
			avr_cycle_count_t when = timer.when;
			if (when < (avr->cycle + cycle[0])) {
				avr->cycle += cycle[0];
				cycle[0] = 0;
				avr_cycle_timer_process(avr);
			}
		}

		if(avr->sreg[S_I]) {
			if(avr_has_pending_interrupts(avr)) {
				avr_service_interrupts(avr);
			}
		}
}
#endif

static void _avr_reg_io_write(avr_t * avr, int* cycle, uint_fast16_t addr, uint_fast8_t v) {
	if(unlikely(addr > avr->ramend)) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}

	REG_TOUCH(avr, addr);

	if (addr == R_SREG) {
		// unsplit the SREG
		SET_SREG_FROM(avr, v);
#ifdef FAST_CORE_FAST_INTERRUPTS
		if(avr->sreg[S_I] && !avr->i_shadow) {
			SEI(avr);
		} else if(!avr->sreg[S_I] && avr->i_shadow) {
			CLI(avr);
		}
#endif
		SREG();
	} else if (addr > 31) {
		uint8_t io = AVR_DATA_TO_IO(addr);
		if (avr->io[io].w.c)
			avr->io[io].w.c(avr, addr, v, avr->io[io].w.param);
		else
			_avr_data_write(avr, addr, v);

		if (avr->io[io].irq) {
			avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
			for (int i = 0; i < 8; i++)
				avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);				
		}

		avr_service_timers_and_interrupts(avr, cycle);
	} else
		_avr_data_write(avr, addr, v);
}

/*
 * Set any address to a value; split between registers and SRAM
 */
static void _avr_set_ram(avr_t * avr, int* cycle, uint_fast16_t addr, uint_fast8_t v)
{
	if (likely(addr >= 256 && addr <= avr->ramend)) {
		_avr_data_write(avr, addr, v);
	} else
		_avr_reg_io_write(avr, cycle, addr, v);
}

static uint_fast8_t _avr_reg_io_read(avr_t * avr, int* cycle, uint_fast16_t addr) {
	if(unlikely(addr > avr->ramend)) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}

	if (addr == R_SREG) {
		/*
		 * SREG is special it's reconstructed when read
		 * while the core itself uses the "shortcut" array
		 */
		uint8_t sreg;
		READ_SREG_INTO(avr, sreg);
		_avr_data_write(avr, R_SREG, sreg);
		return(sreg);
	} else if (addr > 31) {
		uint8_t io = AVR_DATA_TO_IO(addr);
	
		if (avr->io[io].r.c)
			_avr_data_write(avr, addr, avr->io[io].r.c(avr, addr, avr->io[io].r.param));

		if (avr->io[io].irq) {
			uint8_t v = _avr_data_read(avr, addr);
			avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
			for (int i = 0; i < 8; i++)
				avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);				
		}

		avr_service_timers_and_interrupts(avr, cycle);
	}

	return(_avr_data_read(avr, addr));
}


/*
 * Get a value from SRAM.
 */
static uint_fast8_t _avr_get_ram(avr_t * avr, int* cycle, uint_fast16_t addr)
{
	if(likely(addr >= 256 && addr <= avr->ramend))
		return(_avr_data_read(avr, addr));

	return(_avr_reg_io_read(avr, cycle, addr));
}

/*
 * Stack push accessors. Push/pop 8 and 16 bits
 */
static inline void _avr_push8(avr_t * avr, int* cycle, uint_fast8_t v)
{
	NO_RMW(uint_fast16_t sp = _avr_sp_get(avr))
	RMW(uint16_t* ptr_sp; uint_fast16_t sp = _avr_rmw_sp(avr, &ptr_sp));

	TSTACK(printf("%s @0x%04x[0x%04x]\n", __FUNCTION__, sp, v));
	_avr_set_ram(avr, cycle, sp, v);

	NO_RMW(_avr_sp_set(avr, sp-1));
	RMW(_avr_rmw_write16le(ptr_sp, sp - 1));
}

static inline uint_fast8_t _avr_pop8(avr_t * avr, int* cycle)
{
	NO_RMW(uint_fast16_t sp = _avr_sp_get(avr) + 1);
	RMW(uint16_t* ptr_sp; uint_fast16_t sp = _avr_rmw_sp(avr, &ptr_sp) + 1);

	uint_fast8_t res = _avr_get_ram(avr, cycle, sp);
	TSTACK(printf("%s @0x%04x[0x%04x]\n", __FUNCTION__, sp, res));

	NO_RMW(_avr_sp_set(avr, sp));
	RMW(_avr_rmw_write16le(ptr_sp, sp));
	return res;
}

#ifdef FAST_CORE_COMBINING
static inline void _avr_push16xx(avr_t * avr, uint_fast16_t v) {
	NO_RMW(uint_fast16_t sp = _avr_sp_get(avr));
	RMW(uint16_t* ptr_sp; uint_fast16_t sp = _avr_rmw_sp(avr, &ptr_sp));

#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(256 > sp)) {
		printf("%s: stack pointer at 0x%04x below end of io space, aborting.", __FUNCTION__, sp);
		abort();
	}
#endif

	if(likely(sp <= avr->ramend)) {
		_avr_data_write16(avr, sp - 1, v);
		NO_RMW(_avr_sp_set(avr, sp - 2));
		RMW(_avr_rmw_write16le(ptr_sp, sp - 2));
	} else {
		printf("%s: stack pointer at 0x%04x, above ramend... aborting.", __FUNCTION__, sp);
		abort();
	}
}
#endif

static inline void _avr_push16be(avr_t * avr, int* cycle, uint_fast16_t v) {
#ifdef FAST_CORE_COMBINING
	TSTACK(uint_fast16_t sp = _avr_sp_get(avr));
	STACK("push.w ([%02x]@%04x):([%02x]@%04x)\n", 
		v >> 8, sp - 1, v & 0xff, sp);
	_avr_push16xx(avr, _avr_bswap16be(v));
#else
	_avr_push8(avr, cycle, v);
	_avr_push8(avr, cycle, v >> 8);
#endif
}

static inline void _avr_push16le(avr_t * avr, int* cycle, uint_fast16_t v) {
#ifdef FAST_CORE_COMBINING
	TSTACK(uint_fast16_t sp = _avr_sp_get(avr));
	STACK("push.w ([%02x]@%04x):([%02x]@%04x)\n", 
		v & 0xff, sp - 1, v >> 8, sp);
	_avr_push16xx(avr, _avr_bswap16le(v));
#else
	_avr_push8(avr, cycle, v >> 8);
	_avr_push8(avr, cycle, v);
#endif
}

#ifdef FAST_CORE_COMBINING
static inline uint_fast16_t _avr_pop16xx(avr_t * avr) {
	NO_RMW(uint_fast16_t sp = _avr_sp_get(avr) + 2);
	RMW(uint16_t* ptr_sp; uint_fast16_t sp = _avr_rmw_sp(avr, &ptr_sp) + 2);

#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(256 > sp)) {
		printf("%s: stack pointer at 0x%04x, below end of io space... aborting.", __FUNCTION__, sp);
		abort();
	}
#endif

	if(likely(sp < avr->ramend)) {
		uint_fast16_t data = _avr_data_read16(avr, sp - 1);
		NO_RMW(_avr_sp_set(avr, sp));
		RMW(_avr_rmw_write16le(ptr_sp, sp));
		return(data);
	} else {
		printf("%s: stack pointer at 0x%04x, above ramend... aborting.", __FUNCTION__, sp);
		abort();
	}

	return(0);
}
#endif

static inline uint_fast16_t _avr_pop16be(avr_t * avr, int* cycle) {
	uint_fast16_t data;
#ifdef FAST_CORE_COMBINING
	TSTACK(uint_fast16_t sp = _avr_sp_get(avr));
	data = _avr_bswap16be(_avr_pop16xx(avr));
	STACK("pop.w ([%02x]@%04x):([%02x]@%04x)\n", 
		data >> 8, sp + 1, data & 0xff, sp + 2);
#else
	data = _avr_pop8(avr, cycle) << 8;
	data |= _avr_pop8(avr, cycle);
#endif	
	return(data);
}

static inline uint_fast16_t _avr_pop16le(avr_t * avr, int* cycle) {
	uint_fast16_t data;
#ifdef FAST_CORE_COMBINING
	TSTACK(uint_fast16_t sp = _avr_sp_get(avr));
	data = _avr_bswap16le(_avr_pop16xx(avr));
	STACK("pop.w ([%02x]@%04x):([%02x]@%04x)\n",
		data & 0xff, sp + 1, data >> 8, sp + 2);
#else
	data = _avr_pop8(avr, cycle);
	data |= (_avr_pop8(avr, cycle) << 8);
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

static inline void _avr_flags_zcn0vs(avr_t* avr, const uint_fast8_t res, const uint_fast8_t vr) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vr & 1;
	avr->sreg[S_N] = 0;
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

static inline void _avr_flags_add16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t rd, const uint_fast16_t rr) {
	uint_fast16_t xvec = (rd ^ rr);
	uint_fast16_t ovec = (rr ^ res) & ~xvec;
	uint_fast16_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 11) & 1;
	avr->sreg[S_C] = (rxor >> 15) & 1;

	avr->sreg[S_V] = (ovec >> 7) & 1;
}

static inline void _avr_flags_sub(avr_t* avr, const uint_fast8_t res, const uint_fast8_t rd, const uint_fast8_t rr) {
	uint_fast8_t xvec = (rd ^ rr);
	uint_fast8_t ovec = (rd ^ res) & xvec;
	uint_fast8_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 3) & 1;
	avr->sreg[S_C] = (rxor >> 7) & 1;

	avr->sreg[S_V] = (ovec >> 7) & 1;
}

static inline void _avr_flags_sub16(avr_t* avr, const uint_fast16_t res, const uint_fast16_t rd, const uint_fast16_t rr) {
	uint_fast16_t xvec = (rd ^ rr);
	uint_fast16_t ovec = (rd ^ res) & xvec;
	uint_fast16_t rxor = xvec ^ ovec ^ res;

	avr->sreg[S_H] = (rxor >> 11) & 1;
	avr->sreg[S_C] = (rxor >> 15) & 1;

	avr->sreg[S_V] = (ovec >> 7) & 1;
}

enum {
	k_avr_uinst_do_not_define = 0x00,
	k_avr_uinst_d5r5_add = 0x01,
	k_avr_uinst_d5Wr5W_add_adc,
	k_avr_uinst_d5r5_adc,
	k_avr_uinst_p2k6_adiw,
	k_avr_uinst_d5r5_and,
	k_avr_uinst_h4k8_andi,
	k_avr_uinst_h4k16_andi_andi,
	k_avr_uinst_h4r5k8_andi_or,
	k_avr_uinst_h4k8k8_andi_ori,
	k_avr_uinst_d5_asr,
	k_avr_uinst_b3_bclr,
	k_avr_uinst_d5b3_bld,
	k_avr_uinst_o7_brcs,
	k_avr_uinst_o7_brne,
	k_avr_uinst_o7b3_brxc,
	k_avr_uinst_o7b3_brxs,
	k_avr_uinst_b3_bset,
	k_avr_uinst_d5b3_bst,
	k_avr_uinst_x22_call,
	k_avr_uinst_a5b3_cbi,
	k_avr_uinst_d5_clr,
	k_avr_uinst_d5_com,
	k_avr_uinst_d5r5_cp,
	k_avr_uinst_d5Wr5W_cp_cpc,
	k_avr_uinst_d5r5_cpc,
	k_avr_uinst_h4k8_cpi,
	k_avr_uinst_h4r5k8_cpi_cpc,
	k_avr_uinst_d5r5_cpse,
	k_avr_uinst_d5_dec,
	k_avr_uinst_eicall,
	k_avr_uinst_eijmp,
	k_avr_uinst_d5r5_eor,
	k_avr_uinst_icall,
	k_avr_uinst_ijmp,
	k_avr_uinst_d5a6_in,
	k_avr_uinst_d5a6_in_push,
	k_avr_uinst_d5a6m8_in_sbrs,
	k_avr_uinst_d5_inc,
	k_avr_uinst_x22_jmp,
	k_avr_uinst_d5rXYZop_ld,
	k_avr_uinst_d5rXYZq6_ldd,
	k_avr_uinst_d5rXYZq6_ldd_ldd,
	k_avr_uinst_h4k8_ldi,
	k_avr_uinst_h4k16_ldi_ldi,
	k_avr_uinst_h4k8a6_ldi_out,
	k_avr_uinst_d5x16_lds,
	k_avr_uinst_d5x16_lds_lds,
	k_avr_uinst_d5x16_lds_tst,
	k_avr_uinst_d5_lpm_z0,
	k_avr_uinst_d5rXYZop_lpm_z0_st,
	k_avr_uinst_d5_lpm_z1,
	k_avr_uinst_d5_lpm_z1_lpm_z1,
	k_avr_uinst_d5_lsr,
	k_avr_uinst_d5_lsr_ror,
	k_avr_uinst_d5r5_mov,
	k_avr_uinst_d4r4_movw,
	k_avr_uinst_d5r5_mul,
	k_avr_uinst_d16r16_muls,
	k_avr_uinst_d5_neg,
	k_avr_uinst_nop,
	k_avr_uinst_d5r5_or,
	k_avr_uinst_h4k8_ori,
	k_avr_uinst_d5a6_out,
	k_avr_uinst_d5_pop,
	k_avr_uinst_d5a6_pop_out,
	k_avr_uinst_d5_pop_pop16be,
	k_avr_uinst_d5_pop_pop16le,
	k_avr_uinst_d5_push,
	k_avr_uinst_d5_push_push16be,
	k_avr_uinst_d5_push_push16le,
	k_avr_uinst_ret,
	k_avr_uinst_reti,
	k_avr_uinst_d5_ror,
	k_avr_uinst_o12_rcall,
	k_avr_uinst_o12_rjmp,
	k_avr_uinst_a5b3_sbi,
	k_avr_uinst_a5b3_sbic,
	k_avr_uinst_a5b3_sbis,
	k_avr_uinst_p2k6_sbiw,
	k_avr_uinst_d5r5_sbc,
	k_avr_uinst_h4k8_sbci,
	k_avr_uinst_d5b3_sbrc,
	k_avr_uinst_d5b3_sbrs,
	k_avr_uinst_sleep,
	k_avr_uinst_d5rXYZop_st,
	k_avr_uinst_d5rXYZq6_std,
	k_avr_uinst_d5rXYZq6_std_std_hhll,
	k_avr_uinst_d5rXYZq6_std_std_hllh,
	k_avr_uinst_d5x16_sts,
	k_avr_uinst_d5x16_sts_sts,
	k_avr_uinst_d5r5_sub,
	k_avr_uinst_h4k8_subi,
	k_avr_uinst_h4k16_subi_sbci,
	k_avr_uinst_d5_swap,
	k_avr_uinst_d5_tst,
};

static inline void uFlashWrite(avr_t* avr, avr_flashaddr_t addr, uint_fast32_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(addr > avr->flashend)) {
		printf("%s: access at 0x%04x past end of flash, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif

#ifdef FAST_CORE_USE_GLOBAL_FLASH_ACCESS
	_uflash[addr] = data;
#else
#ifndef CONFIG_SIMAVR_FAST_CORE_PIGGYBACKED
	avr->uflash[addr] = data;
#else
	uint32_t	*uflash = ((uint32_t*)&((uint8_t *)avr->flash)[avr->flashend + 1]);
	uflash[addr] = data;
#endif
#endif
}

extern inline uint_fast32_t uFlashRead(avr_t* avr, avr_flashaddr_t addr) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(unlikely(addr > avr->flashend)) {
		printf("%s: access at 0x%04x past end of flash, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif

#ifdef FAST_CORE_USE_GLOBAL_FLASH_ACCESS
	return(_uflash[addr]);
#else
#ifndef CONFIG_SIMAVR_FAST_CORE_PIGGYBACKED
	return(avr->uflash[addr]);
#else
	uint32_t	*uflash = ((uint32_t*)&((uint8_t *)avr->flash)[avr->flashend + 1]);
	return(uflash[addr]);
#endif
#endif
}

#define U_FETCH_OPCODE(u_opcode, addr) uint_fast32_t u_opcode = uFlashRead(avr, addr)
#define UINST_GET_OP(op) uint_fast8_t op = (opcode & 0xff)
#define UINST_GET_R1(r1) uint_fast8_t r1 = ((opcode >> 8) & 0xff)
#define UINST_GET_R2(r2) uint_fast8_t r2 = ((opcode >> 16) & 0xff)
#define UINST_GET_R3(r3) uint_fast8_t r3 = ((opcode >> 24) & 0xff)
#define UINST_GET_X16(x16) uint_fast16_t x16 = (opcode >> 16)
#define UINST_GET_X24(x24) uint_fast32_t x24 = (opcode >> 8)

#define OPCODE(opcode, r1, r2, r3) ((r3<<24)|(r2<<16)|(r1<<8)|(k_avr_uinst_##opcode))

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

#define I_FETCH_OPCODE(opcode, addr) uint_fast16_t opcode = _avr_flash_read16le(avr, addr)
#define INST_GET_A5(a5, opcode) uint_fast8_t a5 = ( 32 + ((opcode & 0x00f8) >> 3) )
#define INST_GET_A6(a6, opcode) uint_fast8_t a6 = ( 32 + ( ((opcode & 0x0600) >> 5) | INST_DECODE_R4(opcode) ) )
#define INST_GET_B3a(b3, opcode) uint_fast8_t b3 = INST_DECODE_B3(opcode)
#define INST_GET_B3b(b3, opcode) uint_fast8_t b3 = ((opcode & 0x0070) >> 4)
#define INST_GET_D4(d4, opcode) uint_fast8_t d4 = INST_DECODE_D4(opcode)
#define INST_GET_D5(d5, opcode) uint_fast8_t d5 = INST_DECODE_D5(opcode)
#define INST_GET_D16(d16, opcode) uint_fast8_t d16 = (16 + INST_DECODE_D4(opcode))
#define INST_GET_H4(h4, opcode) uint_fast8_t h4 = (16 + INST_DECODE_D4(opcode))
#define INST_GET_K6(k6, opcode) uint_fast8_t k6 = (((opcode & 0x00c0) >> 2) | INST_DECODE_R4(opcode))
#define INST_GET_K8(k8, opcode) uint_fast8_t k8 = (((opcode & 0x0f00) >> 4) | INST_DECODE_R4(opcode))
#define INST_GET_O7(o7, opcode) int_fast8_t o7 = ((int16_t)((opcode & 0x03f8) << 6) >> 8)
#define INST_GET_O12(o12, opcode) int_fast16_t o12 = ((int16_t)((opcode & 0x0fff) << 4) >> 3)
#define INST_GET_P2(p2, opcode) uint_fast8_t p2 = (24 + ((opcode & 0x0030) >> 3))
#define INST_GET_Q6(q6, opcode)	uint_fast8_t q6 = ( ((opcode & 0x2000) >> 8) | ((opcode & 0x0c00) >> 7) | INST_DECODE_B3(opcode) )
#define INST_GET_R4(r4, opcode) uint_fast8_t r4 = INST_DECODE_R4(opcode)
#define INST_GET_R5(r5, opcode) uint_fast8_t r5 = ( ((opcode & 0x0200) >> 5) | INST_DECODE_R4(opcode) )
#define INST_GET_R16(r16, opcode) uint_fast8_t r16 = (16 + INST_DECODE_R4(opcode))

#define DEF_INST(name) \
	static inline void _avr_inst_##name(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle, uint_fast16_t opcode)
#define DEF_INSTarg(name, args...) \
	static inline void _avr_inst_##name(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle, uint_fast16_t opcode, ##args)

#define DO_INST(name) \
	_avr_inst_##name(avr, new_pc, cycle, opcode)
#define DO_INSTarg(name, args...) \
	_avr_inst_##name(avr, new_pc, cycle, opcode, ##args)

#ifdef FAST_CORE_ITRACE
#define ITRACE(combining) { \
		printf("\t\t\t\t\t\t\t\t%s  (0x%04x [u0x%08x]) %s\n", (combining ? "combining" : "         "), \
			opcode, uFlashRead(avr, avr->pc), __FUNCTION__); \
	}
#else
#define ITRACE(combining)
#endif

#define UINST(name) \
	static inline void _avr_uinst_##name(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle, uint_fast32_t opcode)
#define U_DO_UINST(name) \
	_avr_uinst_##name(avr, new_pc, cycle, opcode)
#define DO_UINST(name) \
	_avr_uinst_##name(avr, new_pc, cycle, opcode)
#define INST(name)\
	DEF_INST(name) { \
		uint_fast32_t u_opcode = OPCODE(name, 0, 0, 0); \
		DO_UINST(name); \
		uFlashWrite(avr, avr->pc, u_opcode); \
		ITRACE(0); \
	}

#define UINSTarg(name, args...) \
	static inline void _avr_uinst_##name(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle, uint_fast32_t opcode, ##args)
#define DO_UINSTarg(name, args...) \
	_avr_uinst_##name(avr, new_pc, cycle, opcode, ##args)

#define BEGIN_COMBINING \
	int combining = _FAST_CORE_COMBINING; \
	if(combining) { \
		I_FETCH_OPCODE(next_opcode, new_pc[0]);

#define END_COMBINING \
		uFlashWrite(avr, avr->pc, u_opcode); \
		ITRACE(combining); \
	} }

#define BEGIN_COMPLEX \
	if(0 != _FAST_CORE_COMBINING) { \

#define END_COMPLEX \
	} \
	uFlashWrite(avr, avr->pc, u_opcode); \
	ITRACE(0); \
	}

#define CALL_UINST(name) \
	UINST(call_##name) { \
		DO_UINST(name); \
	}

#define UINSTa5b3(name)  \
	UINSTarg(a5b3_##name, const uint_fast8_t io, const uint_fast8_t b, const uint_fast8_t mask)
#define CALL_UINSTa5b3(name)\
	UINST(call_a5b3_##name) { \
		UINST_GET_R1(a5); \
		UINST_GET_R2(b3); \
	/*	UINST_GET_R3(mask);*/\
		DO_UINSTarg(a5b3_##name, a5, b3, 0); \
	}
#define INSTa5b3(name) \
	DEF_INST(a5b3_##name) { \
		INST_GET_A5(a5, opcode); \
		INST_GET_B3a(b3, opcode); \
	\
		DO_UINSTarg(a5b3_##name, a5, b3, (1 << b3)); \
		uFlashWrite(avr, avr->pc, OPCODE(a5b3_##name, a5, b3, (1 << b3))); \
		ITRACE(0); \
	}

#define UINSTa5m8(name)  \
	UINSTarg(a5b3_##name, const uint_fast8_t io, const uint_fast8_t b, const uint_fast8_t mask)
#define CALL_UINSTa5m8(name)\
	UINST(call_a5b3_##name) { \
		UINST_GET_R1(a5); \
	/*	UINST_GET_R2(b3);*/\
		UINST_GET_R3(mask); \
		DO_UINSTarg(a5b3_##name, a5, 0, mask); \
	}

#define UINSTb3(name) \
	UINSTarg(b3_##name, const uint_fast8_t b, const uint_fast8_t mask)
#define CALL_UINSTb3(name) \
	UINST(call_b3_##name) { \
		UINST_GET_R1(b3); \
		DO_UINSTarg(b3_##name, b3, 0); \
	}
#define INSTb3(name) \
	DEF_INST(b3_##name) { \
		INST_GET_B3b(b3, opcode); \
		DO_UINSTarg(b3_##name, b3, (1 << b3)); \
		uFlashWrite(avr, avr->pc, OPCODE(b3_##name, b3, (1 << b3), 0)); \
		ITRACE(0); \
	}

#define UINSTm8(name) \
	UINSTarg(b3_##name, const uint_fast8_t b, const uint_fast8_t mask)
#define CALL_UINSTm8(name) \
	UINST(call_b3_##name) { \
	/*	UINST_GET_R1(b3);*/\
		UINST_GET_R2(mask); \
		DO_UINSTarg(b3_##name, 0, mask); \
	}

#define UINSTd4r4(name) \
	UINSTarg(d4r4_##name, const uint_fast8_t d, const uint_fast8_t r)
#define CALL_UINSTd4r4(name)\
	UINST(call_d4r4_##name) { \
		UINST_GET_R1(d4); \
		UINST_GET_R2(r4); \
		DO_UINSTarg(d4r4_##name, d4, r4); \
	}
#define INSTd4r4(name) DEF_INST(d4r4_##name) {\
		INST_GET_D4(d4, opcode) << 1; \
		INST_GET_R4(r4, opcode) << 1; \
	\
		DO_UINSTarg(d4r4_##name, d4, r4); \
		uFlashWrite(avr, avr->pc, OPCODE(d4r4_##name, d4, r4, 0)); \
		ITRACE(0); \
	}

#define UINSTd5(name) \
	UINSTarg(d5_##name, const uint_fast8_t d)
#define CALL_UINSTd5(name) \
	UINST(call_d5_##name) { \
		UINST_GET_R1(d5); \
		DO_UINSTarg(d5_##name, d5); \
	}
#define INSTd5(name) \
	DEF_INST(d5_##name) { \
		INST_GET_D5(d5, opcode); \
		DO_UINSTarg(d5_##name, d5); \
		uFlashWrite(avr, avr->pc, OPCODE(d5_##name, d5, 0, 0)); \
		ITRACE(0); \
	}
#define COMBINING_INSTd5(name) \
	DEF_INST(d5_##name) { \
		INST_GET_D5(d5, opcode); \
		DO_UINSTarg(d5_##name, d5); \
		uint_fast32_t u_opcode = OPCODE(d5_##name, d5, 0, 0); \
		BEGIN_COMBINING

#define UINSTd5a6(name) \
	UINSTarg(d5a6_##name, const uint_fast8_t d, const uint_fast8_t a)
#define CALL_UINSTd5a6(name)\
	UINST(call_d5a6_##name) { \
		UINST_GET_R1(d5); \
		UINST_GET_R2(a6); \
		DO_UINSTarg(d5a6_##name, d5, a6); \
	}
#define INSTd5a6(name) DEF_INST(d5a6_##name) { \
		INST_GET_D5(d5, opcode); \
		INST_GET_A6(a6, opcode); \
	\
		DO_UINSTarg(d5a6_##name, d5, a6); \
		uFlashWrite(avr, avr->pc, OPCODE(d5a6_##name, d5, a6, 0)); \
		ITRACE(0); \
	}
#define COMBINING_INSTd5a6(name) \
	DEF_INST(d5a6_##name) { \
		INST_GET_D5(d5, opcode); \
		INST_GET_A6(a6, opcode); \
		DO_UINSTarg(d5a6_##name, d5, a6); \
		uint_fast32_t u_opcode = OPCODE(d5a6_##name, d5, a6, 0); \
		BEGIN_COMBINING

#define UINSTd5a6b3(name) \
	UINSTarg(d5a6b3_##name, const uint_fast8_t d, const uint_fast8_t a, const uint_fast8_t b)
#define CALL_UINSTd5a6b3(name)\
	UINST(call_d5a6b3_##name) { \
		UINST_GET_R1(d5); \
		UINST_GET_R2(a6); \
		UINST_GET_R3(b3); \
		DO_UINSTarg(d5a6b3_##name, d5, a6, b3); \
	}

#define UINSTd5a6m8(name) \
	UINSTarg(d5a6m8_##name, const uint_fast8_t d, const uint_fast8_t a, const uint_fast8_t mask)
#define CALL_UINSTd5a6m8(name)\
	UINST(call_d5a6m8_##name) { \
		UINST_GET_R1(d5); \
		UINST_GET_R2(a6); \
		UINST_GET_R3(mask); \
		DO_UINSTarg(d5a6m8_##name, d5, a6, mask); \
	}

#define UINSTd5b3(name) \
	UINSTarg(d5b3_##name, const uint_fast8_t d, const uint_fast8_t b, const uint_fast8_t mask)
#define CALL_UINSTd5b3(name)\
	UINST(call_d5b3_##name) { \
		UINST_GET_R1(d5); \
		UINST_GET_R2(b3); \
	/*	UINST_GET_R3(mask); */\
		DO_UINSTarg(d5b3_##name, d5, b3, 0); \
	}
#define INSTd5b3(name) \
	DEF_INST(d5b3_##name) {\
		INST_GET_D5(d5, opcode); \
		INST_GET_B3a(b3, opcode); \
	\
		DO_UINSTarg(d5b3_##name, d5, b3, (1 << b3)); \
		uFlashWrite(avr, avr->pc, OPCODE(d5b3_##name, d5, b3, (1 << b3))); \
		ITRACE(0); \
	}

#define UINSTd5m8(name) \
	UINSTarg(d5b3_##name, const uint_fast8_t d, const uint_fast8_t b, const uint_fast8_t mask)
#define CALL_UINSTd5m8(name)\
	UINST(call_d5b3_##name) { \
		UINST_GET_R1(d5); \
	/*	UINST_GET_R2(b3);*/\
		UINST_GET_R3(mask); \
		DO_UINSTarg(d5b3_##name, d5, 0, mask); \
	}

#define UINSTd5r5(name) \
	UINSTarg(d5r5_##name, const uint_fast8_t d, const uint_fast8_t r)
#define CALL_UINSTd5r5(name)\
	UINST(call_d5r5_##name) { \
		UINST_GET_R1(d5); \
		UINST_GET_R2(r5); \
		DO_UINSTarg(d5r5_##name, d5, r5); \
	}
#define INSTd5r5(name) \
	DEF_INST(d5r5_##name) { \
		INST_GET_D5(d5, opcode); \
		INST_GET_R5(r5, opcode); \
	\
		DO_UINSTarg(d5r5_##name, d5, r5); \
		uFlashWrite(avr, avr->pc, OPCODE(d5r5_##name, d5, r5, 0)); \
		ITRACE(0); \
	}

#define COMBINING_INSTd5r5(name) \
	DEF_INST(d5r5_##name) { \
		INST_GET_D5(d5, opcode); \
		INST_GET_R5(r5, opcode); \
		DO_UINSTarg(d5r5_##name, d5, r5); \
		uint_fast32_t u_opcode = OPCODE(d5r5_##name, d5, r5, 0); \
		BEGIN_COMBINING
#define COMPLEX_INSTd5r5(name) \
	DEF_INST(d5r5_##name) {\
		INST_GET_D5(d5, opcode); \
		INST_GET_R5(r5, opcode); \
	\
		DO_UINSTarg(d5r5_##name, d5, r5); \
		uint_fast32_t u_opcode = OPCODE(d5r5_##name, d5, r5, 0); \
		BEGIN_COMPLEX


#define UINSTd5rXYZop(name) \
	UINSTarg(d5rXYZop_##name, const uint_fast8_t d, const uint_fast8_t r, const uint_fast8_t op)
#define CALL_UINSTd5rXYZop(name)\
	UINST(call_d5rXYZop_##name) { \
		UINST_GET_R1(d5); \
		UINST_GET_R2(r); \
		UINST_GET_R3(opr); \
		DO_UINSTarg(d5rXYZop_##name, d5, r, opr); \
	}

#define INSTd5rXYZop(name) \
	DEF_INST(d5rXYZop_##name) { \
		INST_GET_D5(d5, opcode); \
		uint_fast8_t r = ((uint_fast8_t []){R_ZL, 0x00, R_YL, R_XL})[(opcode & 0x000c)>>2]; \
		uint_fast8_t opr = opcode & 0x003; \
	\
		DO_UINSTarg(d5rXYZop_##name, d5, r, opr); \
		uFlashWrite(avr, avr->pc, OPCODE(d5rXYZop_##name, d5, r, opr)); \
		ITRACE(0); \
	}

#define UINSTd5rXYZq6(name) \
	UINSTarg(d5rXYZq6_##name, const uint_fast8_t d, const uint_fast8_t r, const uint_fast8_t q)
#define CALL_UINSTd5rXYZq6(name)\
	UINST(call_d5rXYZq6_##name) { \
		UINST_GET_R1(d5); \
		UINST_GET_R2(r); \
		UINST_GET_R3(q6); \
		DO_UINSTarg(d5rXYZq6_##name, d5, r, q6); \
	}
#define INSTd5rXYZq6(name) \
	DEF_INSTarg(d5rXYZq6_##name, uint_fast8_t r) { \
		INST_GET_D5(d5, opcode); \
		INST_GET_Q6(q6, opcode); \
	\
		DO_UINSTarg(d5rXYZq6_##name, d5, r, q6); \
		uFlashWrite(avr, avr->pc, OPCODE(d5rXYZq6_##name, d5, r, q6)); \
		ITRACE(0); \
	}
#define COMBINING_INSTd5rXYZq6(name) \
	DEF_INSTarg(d5rXYZq6_##name, uint_fast8_t r) { \
		INST_GET_D5(d5, opcode); \
		INST_GET_Q6(q6, opcode); \
	\
		DO_UINSTarg(d5rXYZq6_##name, d5, r, q6); \
		uint_fast32_t u_opcode = OPCODE(d5rXYZq6_##name, d5, r, q6); \
		BEGIN_COMBINING

#define UINSTd5x16(name) \
	UINSTarg(d5x16_##name, const uint_fast8_t d, const uint_fast16_t x)
#define CALL_UINSTd5x16(name) \
	UINST(call_d5x16_##name) { \
		UINST_GET_R1(d5); \
		UINST_GET_X16(x16); \
		DO_UINSTarg(d5x16_##name, d5, x16); \
	}
#define INSTd5x16(name) \
	DEF_INST(d5x16_##name) { \
		INST_GET_D5(d5, opcode); \
		I_FETCH_OPCODE(x16, new_pc[0]); \
		DO_UINSTarg(d5x16_##name, d5, x16); \
		uFlashWrite(avr, avr->pc, OPCODE(d5x16_##name, d5, x16, 0)); \
		ITRACE(0); \
	}
#define COMBINING_INSTd5x16(name) \
	DEF_INST(d5x16_##name) { \
		INST_GET_D5(d5, opcode); \
		I_FETCH_OPCODE(x16, new_pc[0]); \
		DO_UINSTarg(d5x16_##name, d5, x16); \
		uint_fast32_t u_opcode = OPCODE(d5x16_##name, d5, x16, 0); \
		BEGIN_COMBINING

#define UINSTd5Wr5W(name) \
	UINSTarg(d5Wr5W_##name, const uint_fast8_t d, const uint_fast8_t r)
#define CALL_UINSTd5Wr5W(name)\
	UINST(call_d5Wr5W_##name) { \
		UINST_GET_R1(d5); \
		UINST_GET_R2(r5); \
		DO_UINSTarg(d5Wr5W_##name, d5, r5); \
	}

#define UINSTd16r16(name) \
	UINSTarg(d16r16_##name, const uint_fast8_t d, const uint_fast8_t r)
#define CALL_UINSTd16r16(name)\
	UINST(call_d16r16_##name) { \
		UINST_GET_R1(d16); \
		UINST_GET_R2(r16); \
		DO_UINSTarg(d16r16_##name, d16, r16); \
	}
#define INSTd16r16(name) \
	DEF_INST(d16r16_##name) { \
		INST_GET_D16(d16, opcode); \
		INST_GET_R16(r16, opcode); \
		DO_UINSTarg(d16r16_##name, d16, r16); \
		uFlashWrite(avr, avr->pc, OPCODE(d16r16_##name, d16, r16, 0)); \
		ITRACE(0); \
	}

#define UINSTh4k8(name) \
	UINSTarg(h4k8_##name, const uint_fast8_t h, const uint_fast8_t k)
#define CALL_UINSTh4k8(name)\
	UINST(call_h4k8_##name) { \
		UINST_GET_R1(h4); \
		UINST_GET_R2(k8); \
		DO_UINSTarg(h4k8_##name, h4, k8); \
	}
#define INSTh4k8(name) \
	DEF_INST(h4k8_##name) { \
		INST_GET_H4(h4, opcode); \
		INST_GET_K8(k8, opcode); \
	\
		DO_UINSTarg(h4k8_##name, h4, k8); \
		uFlashWrite(avr, avr->pc, OPCODE(h4k8_##name, h4, k8, 0)); \
		ITRACE(0); \
	}
#define COMBINING_INSTh4k8(name) \
	DEF_INST(h4k8_##name) { \
		INST_GET_H4(h4, opcode); \
		INST_GET_K8(k8, opcode); \
	\
		DO_UINSTarg(h4k8_##name, h4, k8); \
	\
		uint_fast32_t u_opcode = OPCODE(h4k8_##name, h4, k8, 0); \
		BEGIN_COMBINING
#define UINSTh4k8a6(name) \
	UINSTarg(h4k8a6_##name, const uint_fast8_t h, const uint_fast8_t k, const uint_fast8_t a)
#define CALL_UINSTh4k8a6(name)\
	UINST(call_h4k8a6_##name) { \
		UINST_GET_R1(h4); \
		UINST_GET_R2(k8); \
		UINST_GET_R3(a6); \
		DO_UINSTarg(h4k8a6_##name, h4, k8, a6); \
	}
#define UINSTh4k8k8(name) \
	UINSTarg(h4k8k8_##name, const uint_fast8_t h, const uint_fast8_t k1, const uint_fast8_t k2)
#define CALL_UINSTh4k8k8(name)\
	UINST(call_h4k8k8_##name) { \
		UINST_GET_R1(h4); \
		UINST_GET_R2(k8a); \
		UINST_GET_R3(k8b); \
		DO_UINSTarg(h4k8k8_##name, h4, k8a, k8b); \
	}
#define UINSTh4r5k8(name) \
	UINSTarg(h4r5k8_##name, const uint_fast8_t h, const uint_fast8_t r, const uint_fast8_t k)
#define CALL_UINSTh4r5k8(name)\
	UINST(call_h4r5k8_##name) { \
		UINST_GET_R1(h4); \
		UINST_GET_R2(r5); \
		UINST_GET_R3(k8); \
		DO_UINSTarg(h4r5k8_##name, h4, r5, k8); \
	}

#define UINSTh4k16(name) \
	UINSTarg(h4k16_##name, const uint_fast8_t h, const uint_fast16_t k)
#define CALL_UINSTh4k16(name) \
	UINST(call_h4k16_##name) { \
		UINST_GET_R1(h4); \
		UINST_GET_X16(k16); \
		DO_UINSTarg(h4k16_##name, h4, k16); \
	}

#define UINSTo7(name) \
	UINSTarg(o7_##name, const int_fast8_t o, const uint_fast8_t b, const uint_fast8_t mask)
#define CALL_UINSTo7(name) \
	UINST(call_o7_##name) { \
	/*	UINST_GET_R1(b); */\
		UINST_GET_R2(o); \
	/*	UINST_GET_R3(mask); */\
		DO_UINSTarg(o7_##name, o, 0, 0); \
	}

#define UINSTo7b3(name) \
	UINSTarg(o7b3_##name, const int_fast8_t o, const uint_fast8_t b, const uint_fast8_t mask)
#define CALL_UINSTo7b3(name) \
	UINST(call_o7b3_##name) { \
		UINST_GET_R1(b); \
		UINST_GET_R2(o); \
	/*	UINST_GET_R3(mask); */\
		DO_UINSTarg(o7b3_##name, o, b, 0); \
	}
#define INSTo7b3(name) \
	DEF_INST(o7b3_##name) {\
		INST_GET_O7(o7, opcode); \
		INST_GET_B3a(b3, opcode); \
	\
		DO_UINSTarg(o7b3_##name, o7, b3, (1 << b3)); \
		uFlashWrite(avr, avr->pc, OPCODE(o7b3_##name, b3, o7, (1 << b3))); \
		ITRACE(0); \
	}
#define COMPLEX_INSTo7b3(name) \
	DEF_INST(o7b3_##name) {\
		INST_GET_O7(o7, opcode); \
		INST_GET_B3a(b3, opcode); \
	\
		DO_UINSTarg(o7b3_##name, o7, b3, (1 << b3)); \
		uint32_t u_opcode = OPCODE(o7b3_##name, b3, o7, (1 << b3)); \
		BEGIN_COMPLEX

#define UINSTo12(name)  \
	UINSTarg(o12_##name, const int_fast16_t o)
#define CALL_UINSTo12(name) \
	UINST(call_o12_##name) { \
		UINST_GET_X16(o12); \
		DO_UINSTarg(o12_##name, o12); \
	}
#define INSTo12(name) \
	DEF_INST(o12_##name) { \
		INST_GET_O12(o12, opcode); \
	\
		DO_UINSTarg(o12_##name, o12); \
		uFlashWrite(avr, avr->pc, OPCODE(o12_##name, 0, o12, 0)); \
		ITRACE(0); \
	}

#define UINSTp2k6(name) \
	UINSTarg(p2k6_##name, const uint_fast8_t p, const uint_fast8_t k)
#define CALL_UINSTp2k6(name) \
	UINST(call_p2k6_##name) { \
		UINST_GET_R1(p2); \
		UINST_GET_R2(k6); \
		DO_UINSTarg(p2k6_##name, p2, k6); \
	}
#define INSTp2k6(name) \
	DEF_INST(p2k6_##name) { \
		INST_GET_P2(p2, opcode); \
		INST_GET_K6(k6, opcode); \
	\
		DO_UINSTarg(p2k6_##name, p2, k6); \
		uFlashWrite(avr, avr->pc, OPCODE(p2k6_##name, p2, k6, 0)); \
		ITRACE(0); \
	}

#define UINSTx22(name)  \
	UINSTarg(x22_##name, const uint_fast32_t x22)
#define CALL_UINSTx22(name) \
	UINST(call_x22_##name) { \
		UINST_GET_X24(x22); \
		DO_UINSTarg(x22_##name, x22); \
	}
#define INSTx22(name) \
	DEF_INST(x22_##name) { \
		uint_fast8_t x6 = ((INST_DECODE_D5(opcode) << 1) | (opcode & 0x0001)); \
		I_FETCH_OPCODE(x16, new_pc[0]); \
		uint_fast32_t x22 = ((x6 << 16) | x16) << 1; \
	\
		DO_UINSTarg(x22_##name, x22); \
		uFlashWrite(avr, avr->pc, (x22 << 8) | k_avr_uinst_x22_##name); \
		ITRACE(0); \
	}

#define UINST_RMW_VD() uint8_t* pvd; uint_fast8_t vd = _avr_rmw_r(avr, d, &pvd)
#define UINST_GET_VD() uint_fast8_t vd = _avr_get_r(avr, d)
#define UINST_RMW_VH() uint8_t* pvh; uint_fast8_t vh = _avr_rmw_r(avr, h, &pvh)
#define UINST_GET_VH() uint_fast8_t vh = _avr_get_r(avr, h)
#define UINST_GET_VR() uint_fast8_t vr = _avr_get_r(avr, r)

#define UINST_GET_VA() uint_fast8_t va = _avr_reg_io_read(avr, cycle, a)
#define UINST_GET_VIO() uint_fast8_t vio = _avr_reg_io_read(avr, cycle, io)

#define UINST_RMW_VD16le() uint16_t* pvd; uint_fast16_t vd = _avr_rmw_r16le(avr, d, &pvd)
#define UINST_GET_VD16le() uint_fast16_t vd = _avr_get_r16le(avr, d)
#define UINST_RMW_VH16le() uint16_t* pvh; uint_fast16_t vh = _avr_rmw_r16le(avr, h, &pvh)
#define UINST_GET_VH16le() uint_fast16_t vh = _avr_get_r16le(avr, h)
#define UINST_RMW_VP16le() uint16_t* pvp; uint_fast16_t vp = _avr_rmw_r16le(avr, p, &pvp)
#define UINST_GET_VP16le() uint_fast16_t vp = _avr_get_r16le(avr, p)
#define UINST_RMW_VR16le() uint16_t* pvr; uint_fast16_t vr = _avr_rmw_r16le(avr, r, &pvr)
#define UINST_GET_VR16le() uint_fast16_t vr = _avr_get_r16le(avr, r)

#if 1
#define UINST_GET_VD_VR() \
	uint8_t* data = avr->data; \
	uint_fast8_t vd = data[d]; \
	uint_fast8_t vr = data[r];
#define UINST_RMW_VD_VR() \
	uint8_t* data = avr->data; \
	NO_RMW(uint_fast8_t vd = data[d]); \
	RMW(uint8_t* pvd; uint_fast8_t vd = *(pvd = &data[d])); \
	uint_fast8_t vr = data[r];
#define UINST_GET16le_VD_VR() \
	uint8_t* data = avr->data; \
	uint_fast16_t vd = *((uint16_t*)&data[d]); \
	uint_fast16_t vr = *((uint16_t*)&data[r]);
#define UINST_RMW16le_VD_VR() \
	uint8_t* data = avr->data; \
	NO_RMW(uint_fast16_t vd = *((uint16_t*)&data[d])); \
	RMW(uint16_t* pvd; uint_fast16_t vd = *(pvd = (uint16_t*)&data[d])); \
	uint_fast16_t vr = *((uint16_t*)&data[r]);
#else
#define UINST_GET_VD_VR() \
	UINST_GET_VD(); \
	UINST_GET_VR();
#define UINST_RMW_VD_VR() \
	NO_RMW(UINST_GET_VD()); \
	RMW(UINST_RMW_VD()); \
	UINST_GET_VR();
#define UINST_GET16le_VD_VR() \
	UINST_GET_VD16le(); \
	UINST_GET_VR16le();
#define UINST_RMW16le_VD_VR() \
	NO_RMW(UINST_GET_VD16le()); \
	RMW(UINST_RMW_VD16le()); \
	UINST_GET_VR16le();
#endif

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

	_avr_flags_add(avr, res, vd, vr);
	_avr_flags_zns(avr, res);

	SREG();
}
CALL_UINSTd5r5(adc)
INSTd5r5(adc)

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

	_avr_flags_add(avr, res, vd, vr);
	_avr_flags_zns(avr, res);

	SREG();
}
CALL_UINSTd5r5(add)
COMBINING_INSTd5r5(add)
	INST_GET_D5(d5b, next_opcode);
	INST_GET_R5(r5b, next_opcode);

	if( (0x0c00 /* ADD */ == (opcode & 0xfc00)) && (0x1c00 /* ADDC */ == (next_opcode & 0xfc00))
			&& ((d5 + 1) == d5b) && ((r5 + 1) == r5b) ) {
		u_opcode = OPCODE(d5Wr5W_add_adc, d5, r5, 0);
//		u_opcode = iget_UINSTd5r5(add_adc, d5, r5);
	} else
		combining = 0;
END_COMBINING

UINSTd5Wr5W(add_adc) {
	NO_RMW(UINST_GET16le_VD_VR());
	RMW(UINST_RMW16le_VD_VR());
	uint_fast16_t res = vd + vr;

	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vdl = vd & 0xff; uint8_t vrl = vr & 0xff);
	T(uint8_t res0 = vdl + vrl);
	STATE("add %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdl, avr_regname(r), vrl, res0);
	T(_avr_flags_add(avr, res0, vdl, vrl));
	T(_avr_flags_zns(avr, res0));
	SREG();
#else
//	STATE("/ add %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdl, avr_regname(r), vrl, res0);
	STATE("add.adc %s:%s[%04x], %s:%s[%04x] = %04x\n", avr_regname(d), avr_regname(d + 1), vd, 
		avr_regname(r), avr_regname(r + 1), vr, res);
#endif


	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(STEP_PC());
	T(uint8_t vdh = vd >> 8; uint8_t vrh = vr >> 8);
	T(uint8_t res1 = vdh + vrh + avr->sreg[S_C]);
	STATE("addc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdh, avr_regname(r), vrh, res1);
#else
//	STATE("\\ addc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdh, avr_regname(r), vrh, res1);
#endif

	NO_RMW(_avr_set_r16le(avr, d, res));
	RMW(_avr_rmw_write16le(pvd, res));

	_avr_flags_add16(avr, res, vd, vr);
	_avr_flags_zns16(avr, res);

	SREG();

	NEW_PC(2); CYCLES(1);
}
CALL_UINSTd5Wr5W(add_adc)

UINSTp2k6(adiw) {
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

	CYCLES(1);
}
CALL_UINSTp2k6(adiw)
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
}
CALL_UINSTd5r5(and)
COMPLEX_INSTd5r5(and)
	if(d5 == r5)
		u_opcode = OPCODE(d5_tst, d5, 0, 0);
END_COMPLEX

UINSTh4k8(andi) {
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
}
CALL_UINSTh4k8(andi)
COMBINING_INSTh4k8(andi)
	INST_GET_H4(h4b, next_opcode);
	INST_GET_D5(d5, next_opcode);
	
	if( (0x7000 == (opcode & 0xf000)) && ( 0x7000 == (next_opcode & 0xf000))
			&& ((h4 + 1) == h4b) ) {
		INST_GET_K8(k8b, next_opcode);
		u_opcode = OPCODE(h4k16_andi_andi, h4, k8, k8b);
	} else if( (0x7000 /* ANDI */ == (opcode & 0xf000)) && ( 0x2800 == (next_opcode /* OR */ & 0xfc00))
			&& (h4 == d5) ) {
		INST_GET_R5(r5, next_opcode);
		u_opcode = OPCODE(h4r5k8_andi_or, h4, r5, k8);
	} else if( (0x7000 /* ANDI */ == (opcode & 0xf000)) && ( 0x6000 == (next_opcode /* ORI */ & 0xf000))
			&& (h4 == h4b) ) {
		INST_GET_K8(k8b, next_opcode);
		u_opcode = OPCODE(h4k8k8_andi_ori, h4, k8, k8b);
	} else

		combining = 0;
END_COMBINING

UINSTh4k16(andi_andi) {
	NO_RMW(UINST_GET_VH16le());
	RMW(UINST_RMW_VH16le());
	uint_fast16_t res = vh & k;

	STATE("andi %s:%s[%04x], 0x%04x\n", avr_regname(h), avr_regname(h + 1), vh, k);

	NO_RMW(_avr_set_r16le(avr, h, res));
	RMW(_avr_rmw_write16le(pvh, res));
	
	_avr_flags_znv0s16(avr, res);

	SREG();
	
	NEW_PC(2); CYCLES(1);
}
CALL_UINSTh4k16(andi_andi)

UINSTh4r5k8(andi_or) {
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

	T(STEP_PC());
	NEW_PC(2); CYCLES(1);

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
}
CALL_UINSTh4r5k8(andi_or)

UINSTh4k8k8(andi_ori) {
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

	T(STEP_PC());
	NEW_PC(2); CYCLES(1);

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
}
CALL_UINSTh4k8k8(andi_ori)

UINSTd5(asr) {
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
}
CALL_UINSTd5(asr)
INSTd5(asr)

UINSTb3(bclr) {
	avr->sreg[b]=0;
#ifdef FAST_CORE_FAST_INTERRUPTS
	if(S_I == b) CLI(avr);
#endif
	STATE("cl%c\n", _sreg_bit_name[b]);
	SREG();
}
CALL_UINSTb3(bclr)
INSTb3(bclr)

UINSTd5m8(bld) {
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	uint_fast8_t res = vd | (avr->sreg[S_T] * (mask));
#else
	uint_fast8_t res = vd | (avr->sreg[S_T] ? (mask) : 0);
#endif

	STATE("bld %s[%02x], 0x%02x = %02x\n", avr_regname(d), vd, mask, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));
}
CALL_UINSTd5m8(bld)
INSTd5b3(bld)

UINSTo7(brcs) {
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	int flag = avr->sreg[S_C];
	int branch = (0 != flag);
	avr_flashaddr_t branch_pc = new_pc[0] + (o * branch);
#else
	int branch = (0 != avr->sreg[S_C]);
	avr_flashaddr_t branch_pc = new_pc[0] + (branch ? o : 0);
#endif

	STATE("brcs .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc[0] + o, branch ? "":" not");

#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	CYCLES(branch);
	new_pc[0] = branch_pc;
#else
	if(branch) {
		CYCLES(1);
		new_pc[0] = branch_pc;
	}
#endif	
}
CALL_UINSTo7(brcs)

UINSTo7(brne) {
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	int flag = avr->sreg[S_Z];
	int branch = (0 == flag);
	avr_flashaddr_t branch_pc = new_pc[0] + (o * branch);
#else
	int branch = (0 == avr->sreg[S_Z]);
	avr_flashaddr_t branch_pc = new_pc[0] + (branch ? o : 0);
#endif

	STATE("brne .%d [%04x]\t; Will%s branch\n", o >> 1, new_pc[0] + o, branch ? "":" not");

#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	CYCLES(branch);
	new_pc[0] = branch_pc;
#else
	if(branch) {
		CYCLES(1);
		new_pc[0] = branch_pc;
	}
#endif
}
CALL_UINSTo7(brne)

UINSTo7b3(brxc) {
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	int flag = avr->sreg[b];
	int branch = (0 == flag);
	avr_flashaddr_t branch_pc = new_pc[0] + (o * branch);
#else
	int 		branch = (0 == avr->sreg[b]);
	avr_flashaddr_t branch_pc = new_pc[0] + (branch ? o : 0);
#endif
	const char *names[8] = {
		"brcc", "brne", "brpl", "brvc", NULL, "brhc", "brtc", "brid"
	};

	if (names[b]) {
		STATE("%s .%d [%04x]\t; Will%s branch\n", names[b], o >> 1, new_pc[0] + o, branch ? "":" not");
	} else {
		STATE("brbc%c .%d [%04x]\t; Will%s branch\n", _sreg_bit_name[b], o >> 1, new_pc[0] + o, branch ? "":" not");
	}
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	CYCLES(branch);
	new_pc[0] = branch_pc;
#else
	if (branch) {
		CYCLES(1); // 2 cycles if taken, 1 otherwise
		new_pc[0] = branch_pc;
	}
#endif
}
CALL_UINSTo7b3(brxc)
COMPLEX_INSTo7b3(brxc)
	if(S_Z == b3)
		u_opcode = OPCODE(o7_brne, 0, o7, 0);
END_COMPLEX

UINSTo7b3(brxs) {
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	int flag = avr->sreg[b];
	int branch = (0 != flag);
	avr_flashaddr_t branch_pc = new_pc[0] + (o * branch);
#else
	int branch = (0 != avr->sreg[b]);
	avr_flashaddr_t branch_pc = new_pc[0] + (branch ? o : 0);
#endif

	const char *names[8] = {
		"brcs", "breq", "brmi", "brvs", NULL, "brhs", "brts", "brie"
	};
	if (names[b]) {
		STATE("%s .%d [%04x]\t; Will%s branch\n", names[b], o >> 1, new_pc[0] + o, branch ? "":" not");
	} else {
		STATE("brbs%c .%d [%04x]\t; Will%s branch\n", _sreg_bit_name[b], o >> 1, new_pc[0] + o, branch ? "":" not");
	}
#ifdef FAST_CORE_USE_BRANCHLESS_WITH_MULTIPLY
	CYCLES(branch);
	new_pc[0] = branch_pc;
#else
	if (branch) {
		CYCLES(1); // 2 cycles if taken, 1 otherwise
		new_pc[0] = branch_pc;
	}
#endif
}
CALL_UINSTo7b3(brxs)
COMPLEX_INSTo7b3(brxs)
	if(S_C == b3)
		u_opcode = OPCODE(o7_brcs, 0, o7, 0);
END_COMPLEX

UINSTb3(bset) {
	avr->sreg[b]=1;
#ifdef FAST_CORE_FAST_INTERRUPTS
	if(S_I == b) SEI(avr);
#endif
	STATE("se%c\n", _sreg_bit_name[b]);
	SREG();
}
CALL_UINSTb3(bset)
INSTb3(bset)

UINSTd5b3(bst) {
	UINST_GET_VD();
	uint_fast8_t res = (vd >> b) & 1;

	STATE("bst %s[%02x], 0x%02x = %02x\n", avr_regname(d), vd, (1 << b), res);
	
	avr->sreg[S_T] = res;

	SREG();
}
CALL_UINSTd5b3(bst)
INSTd5b3(bst)


UINSTx22(call) {
	STATE("call 0x%06x\n", x22);

	NEW_PC(2);
	_avr_push16be(avr, cycle, new_pc[0] >> 1);
	new_pc[0] = x22;
	
	CYCLES(3);	// 4 cycles; FIXME 5 on devices with 22 bit PC
	TRACE_JUMP();
	STACK_FRAME_PUSH();
}
CALL_UINSTx22(call)
INSTx22(call)

UINSTa5m8(cbi) {
	UINST_GET_VIO();
	uint_fast8_t res = vio & ~(mask);

	STATE("cbi %s[%04x], 0x%02x = %02x\n", avr_regname(io), vio, mask, res);

	_avr_reg_io_write(avr, cycle, io, res);

	CYCLES(1);
}
CALL_UINSTa5m8(cbi)
INSTa5b3(cbi)

UINSTd5(clr) {
	T(UINST_GET_VD());
	STATE("clr %s[%02x]\n", avr_regname(d), vd);

	_avr_set_r(avr, d, 0);

	avr->sreg[S_N] = 0;
	avr->sreg[S_S] = 0;
	avr->sreg[S_V] = 0;
	avr->sreg[S_Z] = 1;
	
	SREG();
}
CALL_UINSTd5(clr)

UINSTd5(com) {
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = 0xff - vd;

	STATE("com %s[%02x] = %02x\n", avr_regname(d), vd, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	avr->sreg[S_C] = 1;

	_avr_flags_znv0s(avr, res);

	SREG();
}
CALL_UINSTd5(com)
INSTd5(com)

UINSTd5r5(cp) {
	UINST_GET_VD_VR();
	uint_fast8_t res = vd - vr;

	STATE("cp %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_flags_sub(avr, res, vd, vr);
	_avr_flags_zns(avr, res);

	SREG();
}
CALL_UINSTd5r5(cp)
COMBINING_INSTd5r5(cp)
	INST_GET_D5(d5b, next_opcode);
	INST_GET_R5(r5b, next_opcode);

	if( (0x1400 /* CP */ == (opcode & 0xfc00)) && (0x0400 /* CPC */ == (next_opcode & 0xfc00))
			&& ((d5 + 1) == d5b) && ((r5 + 1) == r5b) ) { \
		u_opcode = OPCODE(d5Wr5W_cp_cpc, d5, r5, 0);
//		u_opcode = iget_UINSTd5r5(cp_cpc, d5, r5);
	} else
		combining = 0;
END_COMBINING

UINSTd5Wr5W(cp_cpc) {
	UINST_GET16le_VD_VR();
	uint_fast16_t res = vd - vr;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vdl = vd & 0xff; uint8_t vrl = vr & 0xff);
	T(uint8_t res0 = vdl  - vrl);
	STATE("cp %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vdl, avr_regname(r), vrl, res0);
	T(_avr_flags_sub(avr, res0, vdl, vrl));
	T(_avr_flags_zns(avr, res0));
	T(SREG());
#else
	STATE("cp.cpc %s:%s[%04x], %s:%s[%04x] = %04x\n", avr_regname(d), avr_regname(d + 1), vd, 
		avr_regname(r), avr_regname(r + 1), vr, res);
#endif

#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(STEP_PC());
	T(uint8_t vdh = (vd >> 8) & 0xff; uint8_t vrh = (vr >> 8) & 0xff);
	T(uint8_t res1 = vdl  - vrl);
	STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(d + 1), vdh, avr_regname(r + 1), vrh, res1);
#endif
	_avr_flags_sub16(avr, res, vd, vr);
	_avr_flags_zns16(avr, res);

	SREG();

	NEW_PC(2); CYCLES(1);
}
CALL_UINSTd5Wr5W(cp_cpc)

UINSTd5r5(cpc) {
	UINST_GET_VD_VR();
	uint_fast8_t res = vd - vr - avr->sreg[S_C];

	STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_flags_sub(avr, res, vd, vr);
	_avr_flags_Rzns(avr, res);

	SREG();
}
CALL_UINSTd5r5(cpc)
INSTd5r5(cpc)

UINSTh4k8(cpi) {
	UINST_GET_VH();
	uint_fast8_t res = vh - k;

#ifndef CORE_FAST_CORE_DIFF_TRACE
	STATE("cpi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);
#else
	STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);
#endif

	_avr_flags_sub(avr, res, vh, k);
	_avr_flags_zns(avr, res);

	SREG();
}
CALL_UINSTh4k8(cpi)
COMBINING_INSTh4k8(cpi)
	INST_GET_D5(d5, next_opcode);

	if( (0x3000 /* CPI.l */ == (opcode & 0xf000)) && (0x0400 /* CPC.h */ == (next_opcode & 0xfc00))
			&& ((h4 + 1) == d5) ) {

		INST_GET_R5(r5, next_opcode);
		u_opcode = OPCODE(h4r5k8_cpi_cpc, h4, r5, k8);
	} else
		combining = 0;
END_COMBINING

UINSTh4r5k8(cpi_cpc) {
	UINST_GET_VH16le();
	UINST_GET_VR();
	
	uint_fast16_t vrk = (vr << 8) | k;
	uint_fast16_t res = vh - vrk;

#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vhl = vh & 0xff);
	T(uint8_t res0 = vhl  - k);
	STATE("cpi %s[%02x], 0x%02x\n", avr_regname(h), vhl, k);
	T(_avr_flags_sub(avr, res0, vhl, k));
	T(_avr_flags_zns(avr, res0));
	T(SREG());
#else
	STATE("cpi.cpc %s:%s[%04x], 0x%04x = %04x\n", avr_regname(h), avr_regname(h+1), vh, 
		vrk, res);
#endif

#ifdef CORE_FAST_CORE_DIFF_TRACE
	T(STEP_PC());
	T(uint8_t vhh = (vh >> 8) & 0xff; uint8_t vrh = (vrk >> 8) & 0xff);
	T(uint8_t res1 = vhh  - vrh - avr->sreg[S_C]);
	STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(h + 1), vhh, avr_regname(r), vr, res1);
#endif

	_avr_flags_sub16(avr, res, vh, vrk);
	_avr_flags_zns16(avr, res);

	SREG();

	NEW_PC(2); CYCLES(1);
}
CALL_UINSTh4r5k8(cpi_cpc)

UINSTd5r5(cpse) {
	UINST_GET_VD();
	UINST_GET_VR();
	uint_fast8_t res = vd == vr;

	STATE("cpse %s[%02x], %s[%02x]\t; Will%s skip\n", avr_regname(d), vd, avr_regname(r), vr, res ? "":" not");

	if (res) {
#ifdef FAST_CORE_SKIP_SHIFT
		int shift = _avr_is_instruction_32_bits(avr, new_pc[0]);
		NEW_PC(2 << shift);
		CYCLES(1 << shift);
#else		
		if (_avr_is_instruction_32_bits(avr, new_pc[0])) {
			NEW_PC(4);
			CYCLES(2);
		} else {
			NEW_PC(2);
			CYCLES(1);
		}
#endif
	}
}
CALL_UINSTd5r5(cpse)
INSTd5r5(cpse)

UINSTd5(dec) {
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = vd - 1;

	STATE("dec %s[%02x] = %02x\n", avr_regname(d), vd, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	avr->sreg[S_V] = res == 0x80;

	_avr_flags_zns(avr, res);

	SREG();
}
CALL_UINSTd5(dec)
INSTd5(dec)

UINST(eicall) {
	uint_fast32_t z = (_avr_get_r16le(avr, R_ZL) | avr->data[avr->eind] << 16) << 1;

	STATE("eicall Z[%04x]\n", z);

	CYCLES(1);
	_avr_push16be(avr, cycle, new_pc[0] >> 1);

	new_pc[0] = z;
	CYCLES(1);
	TRACE_JUMP();
}
CALL_UINST(eicall)
INST(eicall)

UINST(eijmp) {
	uint_fast32_t z = (_avr_get_r16le(avr, R_ZL) | avr->data[avr->eind] << 16) << 1;

	STATE("eijmp Z[%04x]\n", z);

	new_pc[0] = z;
	CYCLES(1);
	TRACE_JUMP();
}
CALL_UINST(eijmp)
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
}
CALL_UINSTd5r5(eor)
COMPLEX_INSTd5r5(eor)
	if(d5 == r5)
		u_opcode = OPCODE(d5_clr, d5, 0, 0);
END_COMPLEX

UINST(icall) {
	uint_fast16_t z = _avr_get_r16le(avr, R_ZL) << 1;

	STATE("icall Z[%04x]\n", z);

	CYCLES(1);
	_avr_push16be(avr, cycle, new_pc[0] >> 1);

	new_pc[0] = z;
	CYCLES(1);
	TRACE_JUMP();
}
CALL_UINST(icall)
INST(icall)

UINST(ijmp) {
	uint_fast16_t z = _avr_get_r16le(avr, R_ZL) << 1;

	STATE("ijmp Z[%04x]\n", z);

	new_pc[0] = z;
	CYCLES(1);
	TRACE_JUMP();
}
CALL_UINST(ijmp)
INST(ijmp)

UINSTd5a6(in) {
	UINST_GET_VA();

	STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);

	_avr_set_r(avr, d, va);
}
CALL_UINSTd5a6(in)
COMBINING_INSTd5a6(in)
	INST_GET_D5(d5b, next_opcode);

	if( (0xb000 /* IN */ == (opcode & 0xf800)) && (0xfe00 /* SBRS */ == (next_opcode & 0xfe00))
			&& (d5 == d5b) ) {
		INST_GET_B3a(b3, next_opcode);
		u_opcode = OPCODE(d5a6m8_in_sbrs, d5, a6, (1 << b3));
#if 1
	} else	if( (0xb000 /* IN */ == (opcode & 0xf800)) && (0x920f /* PUSH */ == (0xfe0f & next_opcode))
			&& (d5 == d5b) ) {
		u_opcode = OPCODE(d5a6_in_push, d5, a6, 0);
#endif
	} else
		combining = 0;
END_COMBINING

UINSTd5a6(in_push) {
	UINST_GET_VA();

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);
#else
	STATE("/ in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);
#endif

	_avr_set_r(avr, d, va);
	
	T(STEP_PC()); CYCLES(1);

	
	_avr_push8(avr, cycle, va);

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d), va, _avr_sp_get(avr));
#else
	STACK_STATE("\\ push %s[%02x] (@%04x)\n", avr_regname(d), va, _avr_sp_get(avr));
#endif

	NEW_PC(2);
	CYCLES(1);
}
CALL_UINSTd5a6(in_push)

UINSTd5a6m8(in_sbrs) {
	UINST_GET_VA();
	_avr_set_r(avr, d, va);

//	int	branch = (0 != (va & (mask)));
	int	branch = va & (mask);

	STATE("sbrs (in %s, %s[%02x]),  0x%02x; Will%s branch\n", avr_regname(d), avr_regname(a), 
		va, mask, branch ? "":" not");

	NEW_PC(2); CYCLES(1);

	if (branch) {
#ifdef FAST_CORE_SKIP_SHIFT
		int shift = _avr_is_instruction_32_bits(avr, new_pc[0]);
		NEW_PC(2 << shift);
		CYCLES(1 << shift);
#else		
		if (_avr_is_instruction_32_bits(avr, new_pc[0])) {
			NEW_PC(4);
			CYCLES(2);
		} else {
			NEW_PC(2);
			CYCLES(1);
		}
#endif
	}
}
CALL_UINSTd5a6m8(in_sbrs)

UINSTd5(inc) {
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = vd + 1;

	STATE("inc %s[%02x] = %02x\n", avr_regname(d), vd, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	avr->sreg[S_V] = res == 0x7f;

	_avr_flags_zns(avr, res);

	SREG();
}
CALL_UINSTd5(inc)
INSTd5(inc)

UINSTx22(jmp) {
	STATE("jmp 0x%06x\n", x22);

	new_pc[0] = x22;
	CYCLES(2);
	TRACE_JUMP();
}
CALL_UINSTx22(jmp)
INSTx22(jmp)

UINSTd5rXYZop(ld) {
	NO_RMW(UINST_GET_VR16le());
	RMW(UINST_RMW_VR16le());
	uint_fast8_t ivr;

	CYCLES(1); // 2 cycles (1 for tinyavr, except with inc/dec 2)

	if (op == 2) vr--;
	_avr_set_r(avr, d, ivr = _avr_get_ram(avr, cycle, vr));
	if (op == 1) vr++;
	
	STATE("ld %s, %s%c[%04x]%s\n", avr_regname(d), op == 2 ? "--" : "", *avr_regname(r), vr, op == 1 ? "++" : "");

	if(op) {
		NO_RMW(_avr_set_r16le(avr, r, vr));
		RMW(_avr_rmw_write16le(pvr, vr));
	}
}
CALL_UINSTd5rXYZop(ld)
INSTd5rXYZop(ld)
#if 0
	INST_GET_D5(d5b, next_opcode)

	if( ((opcode & 0xfe0f) == (next_opcode & 0xfe0f))
		&& ((d5 + 1) = d5b) )

#endif


UINSTd5rXYZq6(ldd) {
	UINST_GET_VR16le() + q;
	uint_fast8_t ivr;

	_avr_set_r(avr, d, ivr = _avr_get_ram(avr, cycle, vr));

	STATE("ld %s, (%c+%d[%04x])=[%02x]\n", avr_regname(d), *avr_regname(r), q, vr, ivr);

	CYCLES(1); // 2 cycles (1 for tinyavr, except with inc/dec 2)
}
CALL_UINSTd5rXYZq6(ldd)
COMBINING_INSTd5rXYZq6(ldd)
	INST_GET_D5(d5b, next_opcode);
	INST_GET_Q6(q6b, next_opcode);

	if( ((opcode & 0xd208) == (next_opcode & 0xd208))
			&& (d5 == (d5b + 1)) && (q6 == (q6b - 1)) ) {
		u_opcode = OPCODE(d5rXYZq6_ldd_ldd, d5, r, q6);
	} else
		combining=0;
END_COMBINING

UINSTd5rXYZq6(ldd_ldd) {
	UINST_GET_VR16le() + q;

	CYCLES(1); // 2 cycles (1 for tinyavr, except with inc/dec 2)

#if 1
#if 1
	uint_fast16_t ivr = (_avr_get_ram(avr, cycle, vr) << 8);
		ivr |= _avr_get_ram(avr, cycle, vr + 1);
	_avr_set_r16le(avr, d - 1, ivr);
#else
	uint_fast8_t ivrh = _avr_get_ram(avr, cycle, vr);
	_avr_set_r(avr, d, ivrh);
	uint_fast8_t ivrl = _avr_get_ram(avr, cycle, vr + 1);
	_avr_set_r(avr, d - 1, ivrl);
	T(uint_fast16_t ivr = (ivrh << 8) | ivrl);
#endif
#else
	uint_fast16_t ivr = _avr_get_ram16be(avr, cycle, vr);
	_avr_set_r16le(avr, d - 1, ivr);
#endif

	STATE("ld %s:%s, (%s+%d:%d[%04x:%04x])=[%04x]\n", 
		avr_regname(d), avr_regname(d - 1), 
		avr_regname(r), q, q +1, vr, vr + 1, 
		ivr);

	NEW_PC(2); CYCLES(2);
}
CALL_UINSTd5rXYZq6(ldd_ldd)

UINSTh4k8(ldi) {
	STATE("ldi %s, 0x%02x\n", avr_regname(h), k);

	_avr_set_r(avr, h, k);
}
CALL_UINSTh4k8(ldi)
COMBINING_INSTh4k8(ldi)
	INST_GET_H4(h4b, next_opcode);
	INST_GET_D5(d5, next_opcode);
	
	if( (0xe000 /* LDI.l */ == (opcode & 0xf000)) && (0xe000 /* LDI.h */== (next_opcode & 0xf000))
			&& ((h4 + 1) == h4b) ) {
		INST_GET_K8(k8b, next_opcode);
		u_opcode = OPCODE(h4k16_ldi_ldi, h4, k8, k8b);
	} else if( (0xe000 /* LDI.h */ == (opcode & 0xf000)) && (0xe000 /* LDI.l */== (next_opcode & 0xf000))
			&& (h4 == (h4b + 1)) ) {
		INST_GET_K8(k8b, next_opcode);
		u_opcode = OPCODE(h4k16_ldi_ldi, h4b, k8b, k8);
	} else if( (0xe000 /* LDI */ == (opcode & 0xf000)) && (0xb800 /* OUT */== (next_opcode & 0xf800))
			&& (h4 == d5) ) {
		INST_GET_A6(a6, next_opcode);
		u_opcode = OPCODE(h4k8a6_ldi_out, h4, k8, a6);
	} else

		combining = 0;
END_COMBINING

UINSTh4k16(ldi_ldi) {
	_avr_set_r16le(avr, h, k);

	STATE("ldi.w %s:%s, 0x%04x\n", avr_regname(h), avr_regname(h+1), k);

	NEW_PC(2); CYCLES(1);
}
CALL_UINSTh4k16(ldi_ldi)

UINSTh4k8a6(ldi_out) {
	STATE("ldi %s, 0x%02x\n", avr_regname(h), k);

	_avr_set_r(avr, h, k);

	T(STEP_PC());
	NEW_PC(2); CYCLES(1);

	STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(h), k);

	_avr_reg_io_write(avr, cycle, a, k);
}
CALL_UINSTh4k8a6(ldi_out)

UINSTd5x16(lds) {
	STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_get_r(avr, d), x);

	_avr_set_r(avr, d, _avr_get_ram(avr, cycle, x));

	NEW_PC(2); CYCLES(1); // 2 cycles
}
CALL_UINSTd5x16(lds)
COMBINING_INSTd5x16(lds)
	INST_GET_D5(d5b, next_opcode);
	INST_GET_R5(r5, next_opcode);
	I_FETCH_OPCODE(x16b, new_pc[0] + 2);

	if( (0x9000 /* LDS */ == (0xfe0f & opcode)) && (0x9000 /* LDS */ == (0xfe0f & next_opcode))
			&& ((d5 + 1) == d5b) && ((x16 + 1) == x16b) ) {
		u_opcode = OPCODE(d5x16_lds_lds, d5, x16, 0);
	} else if( (0x9000 /* LDS */ == (0xfe0f & opcode)) && (0x2000 /* TST */ == (0xfc00 & next_opcode))
			&& (d5 == d5b) && (d5 == r5) ) {
		u_opcode = OPCODE(d5x16_lds_tst, d5, x16, 0);
	} else
		combining = 0;
END_COMBINING

UINSTd5x16(lds_lds) {
	STATE("lds.w %s:%s[%04x], 0x%04x:0x%04x\n", avr_regname(d), avr_regname(d + 1), _avr_get_r16(avr, d), x, x + 1);

	/* lds low:high, sts high:low ... replicate order incase in the instance io is accessed. */
	
	NEW_PC(2); CYCLES(1); // 2 cycles

#if 1
#if 1
	uint_fast8_t vxl = _avr_get_ram(avr, cycle, x);
	uint_fast8_t vxh = _avr_get_ram(avr, cycle, x + 1);
	_avr_set_r16le(avr, d, (vxh << 8) | vxl);
#else
	uint_fast8_t vxl = _avr_get_ram(avr, cycle, x);
	_avr_set_r(avr, d, vxl);
	uint_fast8_t vxh = _avr_get_ram(avr, cycle, x + 1);
	_avr_set_r(avr, d + 1, vxh);
#endif
#else
	uint_fast16_t vx = _avr_get_ram16le(avr, cycle, x);
	_avr_set_r16le(avr, d, vx);
#endif

	NEW_PC(4); CYCLES(2);
}
CALL_UINSTd5x16(lds_lds)

UINSTd5x16(lds_tst) {
	STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_get_r(avr, d), x);

	uint_fast8_t vd = _avr_get_ram(avr, cycle, x);
	_avr_set_r(avr, d, vd);

	NEW_PC(2); CYCLES(1); // 2 cycles
	T(STEP_PC());

	STATE("tst %s[%02x]\n", avr_regname(d), vd);

	_avr_flags_znv0s(avr, vd);

	SREG();
	
	NEW_PC(2); CYCLES(1);
}
CALL_UINSTd5x16(lds_tst)


UINSTd5(lpm_z0) {
	uint_fast16_t z = _avr_get_r16le(avr, R_ZL);

	STATE("lpm %s, (Z[%04x])\n", avr_regname(d), z);

	_avr_set_r(avr, d, avr->flash[z]);
	
	CYCLES(2); // 3 cycles
}
CALL_UINSTd5(lpm_z0)
COMBINING_INSTd5(lpm_z0)
	INST_GET_D5(d5b, next_opcode);

	if( (0x9004 /* LPM_Z0 */ == (0xfe0e & opcode)) && (0x920c /* ST */ == (0xfe0e & next_opcode))
			&& (d5 == d5b) ) {
		uint_fast8_t regs[4] = {R_ZL, 0x00, R_YL, R_XL};
		uint_fast8_t r = regs[(next_opcode & 0x000c)>>2];
		uint_fast8_t opr = next_opcode & 0x0003;
		u_opcode = OPCODE(d5rXYZop_lpm_z0_st, d5, r, opr);
	 } else
		combining = 0;
END_COMBINING

UINSTd5rXYZop(lpm_z0_st) {
	uint_fast16_t z = _avr_get_r16le(avr, R_ZL);
	uint_fast8_t vd = avr->flash[z];

	_avr_set_r(avr, d, vd);
	
	CYCLES(2); // 3 cycles

	NO_RMW(UINST_GET_VR16le());
	RMW(UINST_RMW_VR16le());

	if (op == 2) vr--;
	_avr_set_ram(avr, cycle, vr, vd);
	if (op == 1) vr++;
	
	if(op) {
		NO_RMW(_avr_set_r16le(avr, r, vr));
		RMW(_avr_rmw_write16le(pvr, vr));
	}
	STATE("st %s%c[%04x]%s, (lpm %s, (Z[%04x])[0x%02x]\n", op == 2 ? "--" : "", *avr_regname(r), vr,
		op == 1 ? "++" : "", avr_regname(d), z, vd);
	
	CYCLES(1); // 2 cycles, except tinyavr
	NEW_PC(2);
}
CALL_UINSTd5rXYZop(lpm_z0_st)

UINSTd5(lpm_z1) {
	uint_fast16_t z = _avr_get_r16le(avr, R_ZL);

	STATE("lpm %s, (Z[%04x]+)\n", avr_regname(d), z);

	_avr_set_r(avr, d, avr->flash[z]);

	_avr_set_r16le(avr, R_ZL, z+1);

	CYCLES(2); // 3 cycles
}
CALL_UINSTd5(lpm_z1)
COMBINING_INSTd5(lpm_z1)
	INST_GET_D5(d5b, next_opcode);

	if( ((0x9e0c & opcode) == (0x9e0c & next_opcode))
			&& ((d5 + 1) == d5b) ) {
		u_opcode = OPCODE(d5_lpm_z1_lpm_z1, d5, 0, 0);
	 } else
		combining = 0;
END_COMBINING

UINSTd5(lpm_z1_lpm_z1) {
	uint_fast16_t z = _avr_get_r16le(avr, R_ZL);

	STATE("lpm.w %s:%s, (Z[%04x:%04x]+)\n", avr_regname(d), avr_regname(d + 1), z, z + 1);

	_avr_set_r(avr, d, avr->flash[z]);
	_avr_set_r(avr, d + 1, avr->flash[z + 1]);

	_avr_set_r16le(avr, R_ZL, z+2);

	CYCLES(2); // 3 cycles

	NEW_PC(2);
	CYCLES(3);
}
CALL_UINSTd5(lpm_z1_lpm_z1)

UINSTd5(lsr) {
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = vd >> 1;

	STATE("lsr %s[%02x]\n", avr_regname(d), vd);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_zcn0vs(avr, res, vd);

	SREG();
}
CALL_UINSTd5(lsr)
COMBINING_INSTd5(lsr)
	INST_GET_D5(__attribute__((__unused__))d5b, next_opcode);

	if( (0x9406 /* LSR */ == (0xfe0f & opcode)) && (0x9407 /* ROR */ == (0xfe0f & next_opcode))
			&& (d5 == (d5b + 1)) ) {
		u_opcode = OPCODE(d5_lsr_ror, d5b, 0, 0);
	 } else
		combining = 0;
END_COMBINING

UINSTd5(lsr_ror) {
	NO_RMW(UINST_GET_VD16le());
	RMW(UINST_RMW_VD16le());
	uint_fast16_t res = vd >> 1;

	T(uint8_t vdh = vd >> 8);
	T(uint8_t res0 = vdh >> 1);
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("lsr %s[%02x]\n", avr_regname(d + 1), vdh);
#else
	STATE("/ lsr %s[%02x] = [%02x]\n", avr_regname(d + 1), vdh, res0);
#endif

	T(_avr_flags_zcn0vs(avr, res0, vdh));
	SREG();

	T(STEP_PC());
	T(uint8_t vdl = vd & 0xff);

#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("ror %s[%02x]\n", avr_regname(d), vdl);
#else
	T(uint8_t res1 = (avr->sreg[S_C] ? 0x80 : 0) | vd >> 1);
	STATE("\\ ror %s[%02x] = [%02x]\n", avr_regname(d), vdl, res1);
#endif

	NO_RMW(_avr_set_r16le(avr, d, res));
	RMW(_avr_rmw_write16le(pvd, res));

	_avr_flags_zcn0vs16(avr, res, vd);

	CYCLES(1);
	NEW_PC(2);

	SREG();
}
CALL_UINSTd5(lsr_ror)

UINSTd5r5(mov) {
	_avr_mov_r(avr, d, r);

	T(UINST_GET_VR());
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("mov %s, %s[%02x] = %02x\n", avr_regname(d), avr_regname(r), vr, vr);
#else
	STATE("mov %s, %s[%02x]\n", avr_regname(d), avr_regname(r), vr);
#endif
}
CALL_UINSTd5r5(mov)
INSTd5r5(mov)

UINSTd4r4(movw) {
	_avr_mov_r16(avr, d, r);

	T(UINST_GET_VR16le());
	STATE("movw %s:%s, %s:%s[%04x]\n", avr_regname(d), avr_regname(d+1), avr_regname(r), avr_regname(r+1), vr);
}
CALL_UINSTd4r4(movw)
INSTd4r4(movw)

UINSTd5r5(mul) {
	UINST_GET_VD_VR();
	uint_least16_t res = vd * vr;

	STATE("mul %s[%02x], %s[%02x] = %04x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	CYCLES(1);

	_avr_set_r16le(avr, 0, res);

	_avr_flags_zc16(avr, res);

	SREG();
}
CALL_UINSTd5r5(mul)
INSTd5r5(mul)

UINSTd16r16(muls) {
	int_least8_t vd = _avr_get_r(avr, d);
	int_least8_t vr = _avr_get_r(avr, r);
	int_least16_t res = vr * vd;

	STATE("muls %s[%d], %s[%02x] = %d\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_set_r16le(avr, 0, res);

	_avr_flags_zc16(avr, res);

	SREG();
}
CALL_UINSTd16r16(muls)
INSTd16r16(muls)

UINSTd5(neg) {
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
}
CALL_UINSTd5(neg)
INSTd5(neg)

UINST(nop) {
	STATE("nop\n");
}
CALL_UINST(nop)
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
}
CALL_UINSTd5r5(or)
INSTd5r5(or)

UINSTh4k8(ori) {
	NO_RMW(UINST_GET_VH());
	RMW(UINST_RMW_VH());
	uint_fast8_t res = vh | k;

	STATE("ori %s[%02x], 0x%02x\n", avr_regname(h), vh, k);

	NO_RMW(_avr_set_r(avr, h, res));
	RMW(_avr_set_rmw(pvh, res));

	_avr_flags_znv0s(avr, res);

	SREG();
}
CALL_UINSTh4k8(ori)
INSTh4k8(ori)

UINSTd5a6(out) {
	UINST_GET_VD();

	STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), vd);

	_avr_reg_io_write(avr, cycle, a, vd);
}
CALL_UINSTd5a6(out)
INSTd5a6(out)

UINSTd5(pop) {
	uint_fast8_t vd = _avr_pop8(avr, cycle);
	_avr_set_r(avr, d, vd);

	STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d), _avr_sp_get(avr), vd);
	
	CYCLES(1);
}
CALL_UINSTd5(pop);
COMBINING_INSTd5(pop)
	INST_GET_D5(__attribute__((__unused__))d5b, next_opcode);
#if 1
	if( (0x900f /* POP */ == (0xfe0f & opcode)) && (0x900f /* POP */ == (0xfe0f & next_opcode))
			&& (d5 == (d5b + 1)) ) {
		u_opcode = OPCODE(d5_pop_pop16be, d5b, 0, 0);
	} else if( (0x900f /* POP */ == (0xfe0f & opcode)) && (0x900f /* POP */ == (0xfe0f & next_opcode))
			&& ((d5 + 1) == d5b) ) {
		u_opcode = OPCODE(d5_pop_pop16le, d5, 0, 0);
	} else if( (0x900f /* POP */ == (0xfe0f & opcode)) && (0xb800 /* OUT */ == (0xf800 & next_opcode))
			&& (d5 == d5b) ) {
		INST_GET_A6(a6, next_opcode);
		u_opcode = OPCODE(d5a6_pop_out, d5, a6, 0);
	} else
#endif
		combining = 0;
END_COMBINING

UINSTd5a6(pop_out) {
	uint_fast8_t vd = _avr_pop8(avr, cycle);
	_avr_set_r(avr, d, vd);


#ifdef CORE_FAST_CORE_DIFF_TRACE
	STACK_STATE("pop %s (@%04x)[%02x]\n", avr_regname(d), _avr_sp_get(avr), vd);
#else
	STACK_STATE("/ pop %s (@%04x)[%02x]\n", avr_regname(d), _avr_sp_get(avr), vd);
#endif

	T(STEP_PC());
	CYCLES(1);
	
#ifdef CORE_FAST_CORE_DIFF_TRACE
	STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), vd);
#else
	STATE("\\ out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), vd);
#endif

	_avr_reg_io_write(avr, cycle, a, vd);
	
	NEW_PC(2); CYCLES(1);
}
CALL_UINSTd5a6(pop_out);

UINSTd5(pop_pop16le) {
	uint_fast16_t vd = _avr_pop16le(avr, cycle);
	_avr_set_r16le(avr, d, vd);

	STACK("pop.w %s:%s (@%04x)[%04x]\n", avr_regname(d + 1), avr_regname(d), _avr_sp_get(avr), vd);
	
	CYCLES(1);
	NEW_PC(2); CYCLES(2);
}
CALL_UINSTd5(pop_pop16le);

UINSTd5(pop_pop16be) {
	uint_fast16_t vd = _avr_pop16be(avr, cycle);
	_avr_set_r16le(avr, d, vd);

	STACK("pop.w %s:%s (@%04x)[%04x]\n", avr_regname(d + 1), avr_regname(d), _avr_sp_get(avr), vd);
	
	CYCLES(1);
	NEW_PC(2); CYCLES(2);
}
CALL_UINSTd5(pop_pop16be);

UINSTd5(push) {
	UINST_GET_VD();
	_avr_push8(avr, cycle, vd);

	STACK_STATE("push %s[%02x] (@%04x)\n", avr_regname(d), vd, _avr_sp_get(avr));

	CYCLES(1);
}
CALL_UINSTd5(push)

COMBINING_INSTd5(push)
	INST_GET_D5(__attribute__((__unused__))d5b, next_opcode);
#if 1
	if( (0x920f /* PUSH */ == (0xfe0f & opcode)) && (0x920f /* PUSH */ == (0xfe0f & next_opcode))
			&& ((d5 + 1) == d5b) ) {
		u_opcode = OPCODE(d5_push_push16be, d5, 0, 0);
	} else if( (0x920f /* PUSH */ == (0xfe0f & opcode)) && (0x920f /* PUSH */ == (0xfe0f & next_opcode))
			&& (d5 == (d5b + 1)) ) {
		u_opcode = OPCODE(d5_push_push16le, d5b, 0, 0);
	} else
#endif
		combining = 0;
END_COMBINING

UINSTd5(push_push16be) {
	UINST_GET_VD16le();
	_avr_push16be(avr, cycle, vd);

	STACK("push.w %s:%s[%04x] (@%04x)\n", avr_regname(d+1), avr_regname(d), vd, _avr_sp_get(avr));

	CYCLES(1);
	NEW_PC(2); CYCLES(2);
}
CALL_UINSTd5(push_push16be)

UINSTd5(push_push16le) {
	UINST_GET_VD16le();
	_avr_push16le(avr, cycle, vd);

	STACK("push.w %s:%s[%04x] (@%04x)\n", avr_regname(d+1), avr_regname(d), vd, _avr_sp_get(avr));

	CYCLES(1);
	NEW_PC(2); CYCLES(2);
}
CALL_UINSTd5(push_push16le)

UINSTo12(rcall) {
	avr_flashaddr_t branch_pc = new_pc[0] + (int16_t)o;

	STATE("rcall .%d [%04x]\n", (int16_t)o >> 1, branch_pc);

	_avr_push16be(avr, cycle, new_pc[0] >> 1);

	new_pc[0] = branch_pc;
	CYCLES(2);
	// 'rcall .1' is used as a cheap "push 16 bits of room on the stack"
	if (o != 0) {
		TRACE_JUMP();
		STACK_FRAME_PUSH();
	}
}
CALL_UINSTo12(rcall)
INSTo12(rcall)

UINST(ret) {
	new_pc[0] = _avr_pop16be(avr, cycle) << 1;
	CYCLES(3);
	STATE("ret\n");
	TRACE_JUMP();
	STACK_FRAME_POP();
}
CALL_UINST(ret)
INST(ret)

UINST(reti) {
	new_pc[0] = _avr_pop16be(avr, cycle) << 1;
	avr->sreg[S_I] = 1;
#ifdef FAST_CORE_FAST_INTERRUPTS
	SEI(avr);
#endif
	CYCLES(3);
	STATE("reti\n");
	TRACE_JUMP();
	STACK_FRAME_POP();
}
CALL_UINST(reti)
INST(reti)

UINSTo12(rjmp) {
	avr_flashaddr_t	branch_pc = new_pc[0] + (int16_t)o;

	STATE("rjmp .%d [%04x]\n", (int16_t)o >> 1, branch_pc);

	new_pc[0] = branch_pc;
	CYCLES(1);
	TRACE_JUMP();
}
CALL_UINSTo12(rjmp)
INSTo12(rjmp)

UINSTd5(ror) {
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

	_avr_flags_zcn0vs(avr, res, vd);

	SREG();
}
CALL_UINSTd5(ror)
INSTd5(ror)

UINSTd5r5(sbc) {
	NO_RMW(UINST_GET_VD_VR());
	RMW(UINST_RMW_VD_VR());
	uint_fast8_t res = vd - vr - avr->sreg[S_C];

	STATE("sbc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_sub(avr, res, vd, vr);
	_avr_flags_Rzns(avr, res);

	SREG();
}
CALL_UINSTd5r5(sbc)
INSTd5r5(sbc)

UINSTa5m8(sbi) {
	UINST_GET_VIO();
	uint_fast8_t res = vio | mask;

	STATE("sbi %s[%04x], 0x%02x = %02x\n", avr_regname(io), vio, mask, res);

	_avr_reg_io_write(avr, cycle, io, res);

	CYCLES(1);
}
CALL_UINSTa5m8(sbi)
INSTa5b3(sbi)

UINSTa5m8(sbic) {
	UINST_GET_VIO();
	uint_fast8_t res = vio & mask;

	STATE("sbic %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), vio, mask, !res?"":" not");

	if (!res) {
#ifdef FAST_CORE_SKIP_SHIFT
		int shift = _avr_is_instruction_32_bits(avr, new_pc[0]);
		NEW_PC(2 << shift);
		CYCLES(1 << shift);
#else		
		if (_avr_is_instruction_32_bits(avr, new_pc[0])) {
			NEW_PC(4);
			CYCLES(2);
		} else {
			NEW_PC(2);
			CYCLES(1);
		}
#endif
	}
}
CALL_UINSTa5m8(sbic)
INSTa5b3(sbic)

UINSTa5m8(sbis) {
	UINST_GET_VIO();
	uint_fast8_t res = vio & mask;

	STATE("sbis %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), vio, mask, res?"":" not");

	if (res) {
#ifdef FAST_CORE_SKIP_SHIFT
		int shift = _avr_is_instruction_32_bits(avr, new_pc[0]);
		NEW_PC(2 << shift);
		CYCLES(1 << shift);
#else		
		if (_avr_is_instruction_32_bits(avr, new_pc[0])) {
			NEW_PC(4);
			CYCLES(2);
		} else {
			NEW_PC(2);
			CYCLES(1);
		}
#endif
	}
}
CALL_UINSTa5m8(sbis)
INSTa5b3(sbis)

UINSTp2k6(sbiw) {
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

	CYCLES(1);
}
CALL_UINSTp2k6(sbiw)
INSTp2k6(sbiw)

UINSTh4k8(sbci) {
	UINST_GET_VH();
	uint_fast8_t res = vh - k - avr->sreg[S_C];

	STATE("sbci %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);

	_avr_set_r(avr, h, res);

#ifndef CORE_FAST_CORE_DIFF_TRACE
	/* CORE BUG - standard core does not calculate half carry with this instruction. */
	avr->sreg[S_H] = ((k + avr->sreg[S_C]) & 0x07) > (vh & 0x07);
#endif
	avr->sreg[S_C] = (k + avr->sreg[S_C]) > vh;

	_avr_flags_Rzns(avr, res);

	SREG();
}
CALL_UINSTh4k8(sbci)
INSTh4k8(sbci)

UINSTd5m8(sbrc) {
	UINST_GET_VD();
	int	branch = (0 == (vd & (mask)));

	STATE("sbrc %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), vd, mask, branch ? "":" not");

	if (branch) {
#ifdef FAST_CORE_SKIP_SHIFT
		int shift = _avr_is_instruction_32_bits(avr, new_pc[0]);
		NEW_PC(2 << shift);
		CYCLES(1 << shift);
#else		
		if (_avr_is_instruction_32_bits(avr, new_pc[0])) {
			NEW_PC(4);
			CYCLES(2);
		} else {
			NEW_PC(2);
			CYCLES(1);
		}
#endif
	}

}
CALL_UINSTd5m8(sbrc)
INSTd5b3(sbrc)

UINSTd5m8(sbrs) {
	UINST_GET_VD();
	int	branch = (0 != (vd & (mask)));

	STATE("sbrs %s[%02x], 0x%02x\t; Will%s branch\n", avr_regname(d), vd, mask, branch ? "":" not");

	if (branch) {
#ifdef FAST_CORE_SKIP_SHIFT
		int shift = _avr_is_instruction_32_bits(avr, new_pc[0]);
		NEW_PC(2 << shift);
		CYCLES(1 << shift);
#else		
		if (_avr_is_instruction_32_bits(avr, new_pc[0])) {
			NEW_PC(4);
			CYCLES(2);
		} else {
			NEW_PC(2);
			CYCLES(1);
		}
#endif
	}

}
CALL_UINSTd5m8(sbrs)
INSTd5b3(sbrs)

UINST(sleep) {
	STATE("sleep\n");
	/* Don't sleep if there are interrupts about to be serviced.
	 * Without this check, it was possible to incorrectly enter a state
	 * in which the cpu was sleeping and interrupts were disabled. For more
	 * details, see the commit message. */
	if (!avr_has_pending_interrupts(avr) || !avr->sreg[S_I])
		avr->state = cpu_Sleeping;
#ifdef FAST_CORE_FAST_INTERRUPTS
	avr->cycle += cycle[0];
	cycle[0] = 0;
#endif
}
CALL_UINST(sleep)
INST(sleep)

UINSTd5rXYZop(st) {
	UINST_GET_VD();
	NO_RMW(UINST_GET_VR16le());
	RMW(UINST_RMW_VR16le());

	STATE("st %s%c[%04x]%s, %s[%02x] \n", op == 2 ? "--" : "", *avr_regname(r), vr,
		op == 1 ? "++" : "", avr_regname(d), vd);

	CYCLES(1); // 2 cycles, except tinyavr

	if (op == 2) vr--;
	_avr_set_ram(avr, cycle, vr, vd);
	if (op == 1) vr++;

	if(op) {
		NO_RMW(_avr_set_r16le(avr, r, vr));
		RMW(_avr_rmw_write16le(pvr, vr));
	}
}
CALL_UINSTd5rXYZop(st)
INSTd5rXYZop(st)

UINSTd5rXYZq6(std) {
	UINST_GET_VD();
	UINST_GET_VR16le() + q;

	STATE("st (%c+%d[%04x]), %s[%02x]\n", *avr_regname(r), q, vr, avr_regname(d), vd);

	_avr_set_ram(avr, cycle, vr, vd);

	CYCLES(1); // 2 cycles, except tinyavr
}
CALL_UINSTd5rXYZq6(std)
COMBINING_INSTd5rXYZq6(std)
	INST_GET_D5(__attribute__((__unused__)) d5b, next_opcode);
	INST_GET_Q6(__attribute__((__unused__)) q6b, next_opcode);

//	STATE("0x%04x 0x%04x, 0x%02x:0x%02x 0x%02x:0x%02x\n", opcode&0xd208, next_opcode&0xd208, d5,d5b, q6,q6b);

#if 1
	if( ((opcode & 0xd208) == (next_opcode & 0xd208))
			&& (d5 == (d5b + 1)) && (q6 == (q6b + 1)) ) {
		u_opcode = OPCODE(d5rXYZq6_std_std_hhll, d5b, r, q6b);
	} else	if( ((opcode & 0xd208) == (next_opcode & 0xd208))
			&& ((d5 + 1) == d5b) && (q6 == (q6b + 1)) ) {
		u_opcode = OPCODE(d5rXYZq6_std_std_hllh, d5, r, q6b);
	} else
#endif
		combining=0;
END_COMBINING

UINSTd5rXYZq6(std_std_hhll) {
	UINST_GET_VD16le();
	UINST_GET_VR16le() + q;

	STATE("st (%c+%d:%d[%04x:%04x]), %s:%s[%04x]\n", *avr_regname(r), q, q + 1, vr, vr + 1,
		avr_regname(d), avr_regname(d + 1), vd);

	_avr_set_ram(avr, cycle, vr + 1, vd >> 8);
	_avr_set_ram(avr, cycle, vr, vd & 0xff);

	CYCLES(1); // 2 cycles, except tinyavr
	
	NEW_PC(2);
	CYCLES(2);
}
CALL_UINSTd5rXYZq6(std_std_hhll)

UINSTd5rXYZq6(std_std_hllh) {
	UINST_GET_VD16le();
	UINST_GET_VR16le() + q;

	STATE("st (%c+%d:%d[%04x:%04x]), %s:%s[%04x]\n", *avr_regname(r), q + 1, q , vr + 1, vr,
		avr_regname(d), avr_regname(d + 1), vd);

	_avr_set_ram(avr, cycle, vr + 1, vd & 0xff);
	_avr_set_ram(avr, cycle, vr , vd >> 8);

	CYCLES(1); // 2 cycles, except tinyavr

	NEW_PC(2);
	CYCLES(2);
}
CALL_UINSTd5rXYZq6(std_std_hllh)

UINSTd5x16(sts) {
	UINST_GET_VD();

	NEW_PC(2);

	STATE("sts 0x%04x, %s[%02x]\n", x, avr_regname(d), vd);

	CYCLES(1);

	_avr_set_ram(avr, cycle, x, vd);
}
CALL_UINSTd5x16(sts)
COMBINING_INSTd5x16(sts)
	INST_GET_D5(d5b, next_opcode);
	I_FETCH_OPCODE(x16b, new_pc[0] + 2);

	if( (0x9200 /* STS */ == (0xfe0f & opcode)) && (0x9200 /* STS */ == (0xfe0f & next_opcode))
			&& (d5 == (d5b + 1)) && (x16 == (x16b + 1)) ) {
		u_opcode = OPCODE(d5x16_sts_sts, d5b, x16b, 0);
	} else
		combining = 0;
END_COMBINING

UINSTd5x16(sts_sts) {
	UINST_GET_VD16le();

	STATE("sts.w 0x%04x:0x%04x, %s:%s[%04x]\n", x, x + 1, avr_regname(d), avr_regname(d + 1), _avr_get_r16(avr, d));

	/* lds low:high, sts high:low ... replicate order incase in the instance io is accessed. */
	_avr_set_ram(avr, cycle, x + 1, vd >> 8);
//	NEW_PC(2); CYCLES(1);
	_avr_set_ram(avr, cycle, x, vd & 0xff);
	NEW_PC(2); CYCLES(1);
	NEW_PC(4); CYCLES(2);
}
CALL_UINSTd5x16(sts_sts)

UINSTd5r5(sub) {
	NO_RMW(UINST_GET_VD_VR());
	RMW(UINST_RMW_VD_VR());
	uint_fast8_t res = vd - vr;

	STATE("sub %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));

	_avr_flags_sub(avr, res, vd, vr);
	_avr_flags_zns(avr, res);

	SREG();
}
CALL_UINSTd5r5(sub)
INSTd5r5(sub)

UINSTh4k8(subi) {
	NO_RMW(UINST_GET_VH());
	RMW(UINST_RMW_VH());
	uint_fast8_t res = vh - k;

	STATE("subi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);

	NO_RMW(_avr_set_r(avr, h, res));
	RMW(_avr_set_rmw(pvh, res));

#ifndef CORE_FAST_CORE_DIFF_TRACE
	/* CORE BUG - standard core does not calculate half carry with this instruction. */
	avr->sreg[S_H] = (k & 0x07) > (vh & 0x07);
#endif
	avr->sreg[S_C] = k > vh;

	_avr_flags_zns(avr, res);

	SREG();
}
CALL_UINSTh4k8(subi)
COMBINING_INSTh4k8(subi)
	INST_GET_H4(__attribute__((__unused__)) h4b, next_opcode);

	if( (0x5000 /* SUBI.l */ == (opcode & 0xf000)) && (0x4000 /* SBCI.h */ == (next_opcode & 0xf000))
			&& ((h4 + 1) == h4b) ) {
		INST_GET_K8(__attribute__((__unused__)) k8b, next_opcode);
		u_opcode = OPCODE(h4k16_subi_sbci, h4, k8, k8b);
	} else
		combining = 0;
END_COMBINING

UINSTh4k16(subi_sbci) {
	NO_RMW(UINST_GET_VH16le());
	RMW(UINST_RMW_VH16le());
	uint_fast16_t res = vh - k;

#ifndef CORE_FAST_CORE_DIFF_TRACE
	T(uint8_t vhl = vh & 0xff; vkl = k & 0xff);
	T(uint8_t res0 = vhl - vkl);
	STATE("subi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vhl, vkl, res0);
#ifndef CORE_FAST_CORE_DIFF_TRACE
	/* CORE BUG - standard core does not calculate half carry with this instruction. */
	T(avr->sreg[S_H] = (k & 0x07) > (vh & 0x07));
#endif
	T(avr->sreg[S_C] = k > vh);
	SREG();
#endif

	NO_RMW(_avr_set_r16le(avr, h, res));
	RMW(_avr_rmw_write16le(pvh, res));


#ifndef CORE_FAST_CORE_DIFF_TRACE
	T(STEP_PC());
	T(uint8_t vhh = vh >> 8; uint8_t vkh = k >> 8);
	T(uint_fast8_t res1 = vhh - vkh - avr->sreg[S_C]);
	STATE("sbci %s[%02x], 0x%02x = %02x\n", avr_regname(h), vhh, vkh, res1);
#else
	STATE("subi.sbci %s:%s[%04x], 0x%04x = %04x\n", avr_regname(h), avr_regname(h + 1), vh, k, res);
#endif

	_avr_flags_sub16(avr, res, vh, k);
	_avr_flags_zns16(avr, res);
	
	SREG();

	NEW_PC(2); CYCLES(1);
}
CALL_UINSTh4k16(subi_sbci)

UINSTd5(swap) {
	NO_RMW(UINST_GET_VD());
	RMW(UINST_RMW_VD());
	uint_fast8_t res = (vd >> 4) | (vd << 4);

	STATE("swap %s[%02x] = %02x\n", avr_regname(d), vd, res);

	NO_RMW(_avr_set_r(avr, d, res));
	RMW(_avr_set_rmw(pvd, res));
}
CALL_UINSTd5(swap)
INSTd5(swap)

UINSTd5(tst) {
	UINST_GET_VD();

	STATE("tst %s[%02x]\n", avr_regname(d), vd);

	_avr_flags_znv0s(avr, vd);

	SREG();
}
CALL_UINSTd5(tst)

/*
 * Called when an invalid opcode is decoded
 */
static inline void _avr_invalid_opcode(avr_t * avr)
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

#if 0
static inline void avr_inst_misc(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle, uint_fast16_t opcode) {
	uint_fast8_t u_opcode;
	switch(opcode) {
		case	0x0000:
//			u_opcode = _nop;
			break;
		case	0x9588:
//			u_opcode = k_inst_sleep;
	}
}
#endif


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
extern inline void avr_decode_one(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle)
{
//	new_pc[0] = avr->pc + 2;	// future "default" pc
//	cycle[0] = 1;

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

	I_FETCH_OPCODE(opcode, avr->pc);

#ifdef FAST_CORE_DECODE_TRAP
	U_FETCH_OPCODE(u_opcode, avr->pc);
	if(unlikely(u_opcode)) {
		xSTATE("opcode trap, not handled: 0x%08x [0x%04x]\n", u_opcode, opcode);
	}
#endif

	switch (opcode & 0xf000) {
		case 0x0000: {
			switch (opcode) {
				case 0x0000: {	// NOP
					DO_INST(nop);
				}	break;
				default: {
					switch (opcode & 0xfc00) {
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
							switch (opcode & 0xff00) {
								case 0x0100: {	// MOVW – Copy Register Word 0000 0001 dddd rrrr
									DO_INST(d4r4_movw);
								}	break;
								case 0x0200: {	// MULS – Multiply Signed 0000 0010 dddd rrrr
									DO_INST(d16r16_muls);
								}	break;
								case 0x0300: {	// MUL Multiply 0000 0011 fddd frrr
									int8_t r = 16 + (opcode & 0x7);
									int8_t d = 16 + ((opcode >> 4) & 0x7);
									int16_t res = 0;
									uint8_t c = 0;
									T(const char * name = "";)
									switch (opcode & 0x88) {
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
									CYCLES(1);
									STATE("%s %s[%d], %s[%02x] = %d\n", name, avr_regname(d), ((int8_t)avr->data[d]), avr_regname(r), ((int8_t)avr->data[r]), res);
									_avr_set_r16le(avr, 0, res);
									avr->sreg[S_C] = c;
									avr->sreg[S_Z] = res == 0;
									SREG();
								}	break;
								default: _avr_invalid_opcode(avr);
							}
					}
				}
			}
		}	break;

		case 0x1000: {
			switch (opcode & 0xfc00) {
				case 0x1800: {	// SUB without carry 0000 10 rd dddd rrrr
					DO_INST(d5r5_sub);
				}	break;
				case 0x1000: {	// CPSE Compare, skip if equal 0000 00 rd dddd rrrr
					DO_INST(d5r5_cpse);
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
			switch (opcode & 0xfc00) {
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
			switch (opcode & 0xd008) {
				case 0xa000:
				case 0x8000: {	// LD (LDD) – Load Indirect using Z 10q0 qq0r rrrr 0qqq
					if(opcode & 0x0200)
						DO_INSTarg(d5rXYZq6_std, R_ZL);
					else
						DO_INSTarg(d5rXYZq6_ldd, R_ZL);
				}	break;
				case 0xa008:
				case 0x8008: {	// LD (LDD) – Load Indirect using Y 10q0 qq0r rrrr 1qqq
					if(opcode & 0x0200)
						DO_INSTarg(d5rXYZq6_std, R_YL);
					else
						DO_INSTarg(d5rXYZq6_ldd, R_YL);
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0x9000: {
#if 0
			/* this is an annoying special case, but at least these lines handle all the SREG set/clear opcodes */
			if ((opcode & 0xff0f) == 0x9408) {
				uint8_t b = (opcode >> 4) & 7;
				STATE("%s%c\n", opcode & 0x0080 ? "cl" : "se", _sreg_bit_name[b]);
				avr->sreg[b] = (opcode & 0x0080) == 0;
				SREG();
			} else 
#endif
				switch (opcode) {
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
						new_pc[0] = avr->pc;
#ifdef FAST_CORE_FAST_INTERRUPTS
						avr->cycle = cycle[0];
						cycle[0] = 0;
#endif
					}
				}	break;
				case 0x95a8: { // WDR
					STATE("wdr\n");
					avr_ioctl(avr, AVR_IOCTL_WATCHDOG_RESET, 0);
				}	break;
				case 0x95e8: { // SPM
					STATE("spm\n");
					avr_ioctl(avr, AVR_IOCTL_FLASH_SPM, 0);
				}	break;
				case 0x9409: {   // IJMP Indirect jump 					1001 0100 0000 1001
					DO_INST(ijmp);
				}	break;
				case 0x9419: {  // EIJMP Indirect jump 					1001 0100 0001 1001   bit 4 is "indirect"
					if (!avr->eind)
						_avr_invalid_opcode(avr);
					else
						DO_INST(eijmp);
				}	break;
				case 0x9509: {  // ICALL Indirect Call to Subroutine		1001 0101 0000 1001
					DO_INST(icall);
				}	break;
				case 0x9519: { // EICALL Indirect Call to Subroutine	1001 0101 0001 1001   bit 8 is "push pc"
					if (!avr->eind)
						_avr_invalid_opcode(avr);
					else
						DO_INST(eicall);
				}	break;
				case 0x9518: {	// RETI
					DO_INST(reti);
				}	break;
				case 0x9508: {	// RET
					DO_INST(ret);
				}	break;
				case 0x95c8: {	// LPM Load Program Memory R0 <- (Z) 1001 0101 1100 1000
					opcode = 0x9004;
					DO_INSTarg(d5_lpm_z0);
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
					switch (opcode & 0xfe0f) {
						case 0x9000: {	// LDS Load Direct from Data Space, 32 bits
							DO_INST(d5x16_lds);
						}	break;
						case 0x9005: {	// LPM Load Program Memory 1001 000d dddd 01oo
							DO_INST(d5_lpm_z1);
//							DO_INST(d5_lpm_z0);
						}	break;
						case 0x9004: {	// LPM Load Program Memory 1001 000d dddd 01oo
							DO_INST(d5_lpm_z0);
//							DO_INST(d5_lpm_z1);
						}	break;
						case 0x9006:
						case 0x9007: {	// ELPM Extended Load Program Memory 1001 000d dddd 01oo
							if (!avr->rampz)
								_avr_invalid_opcode(avr);
							uint32_t z = _avr_get_r16le(avr, R_ZL) | (avr->data[avr->rampz] << 16);

							uint_fast8_t r = (opcode >> 4) & 0x1f;
							int op = opcode & 3;
							STATE("elpm %s, (Z[%02x:%04x]%s)\n", avr_regname(r), z >> 16, z&0xffff, opcode?"+":"");
							_avr_set_r(avr, r, avr->flash[z]);
							if (op == 3) {
								z++;
								_avr_set_r(avr, avr->rampz, z >> 16);
								_avr_set_r16le(avr, R_ZL, (z&0xffff));
							}
							CYCLES(2); // 3 cycles
						}	break;
						/*
						 * Load store instructions
						 *
						 * 1001 00sr rrrr iioo
						 * s = 0 = load, 1 = store
						 * ii = 16 bits register index, 11 = Z, 10 = Y, 00 = X
						 * oo = 1) post increment, 2) pre-decrement
						 */
						case 0x900c:
						case 0x900d:
						case 0x900e: {	// LD Load Indirect from Data using X 1001 000r rrrr 11oo
							DO_INSTarg(d5rXYZop_ld);
						}	break;
						case 0x920c:
						case 0x920d:
						case 0x920e: {	// ST Store Indirect Data Space X 1001 001r rrrr 11oo
							DO_INSTarg(d5rXYZop_st);
						}	break;
						case 0x9009:
						case 0x900a: {	// LD Load Indirect from Data using Y 1001 000r rrrr 10oo
							DO_INSTarg(d5rXYZop_ld);
						}	break;
						case 0x9209:
						case 0x920a: {	// ST Store Indirect Data Space Y 1001 001r rrrr 10oo
							DO_INSTarg(d5rXYZop_st);
						}	break;
						case 0x9200: {	// STS ! Store Direct to Data Space, 32 bits
							DO_INST(d5x16_sts);
						}	break;
						case 0x9001:
						case 0x9002: {	// LD Load Indirect from Data using Z 1001 001r rrrr 00oo
							DO_INSTarg(d5rXYZop_ld);
						}	break;
						case 0x9201:
						case 0x9202: {	// ST Store Indirect Data Space Z 1001 001r rrrr 00oo
							DO_INSTarg(d5rXYZop_st);
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
							switch (opcode & 0xff00) {
								case 0x9600: {	// ADIW - Add Immediate to Word 1001 0110 KKdd KKKK
									DO_INST(p2k6_adiw);
								}	break;
								case 0x9700: {	// SBIW - Subtract Immediate from Word 1001 0110 KKdd KKKK
									DO_INST(p2k6_sbiw);
								}	break;
								case 0x9800: {	// CBI - Clear Bit in I/O Register 1001 1000 AAAA Abbb
									DO_INST(a5b3_cbi);
								}	break;
								case 0x9900: {	// SBIC - Skip if Bit in I/O Register is Cleared 1001 0111 AAAA Abbb
									DO_INST(a5b3_sbic);
								}	break;
								case 0x9a00: {	// SBI - Set Bit in I/O Register 1001 1000 AAAA Abbb
									DO_INST(a5b3_sbi);
								}	break;
								case 0x9b00: {	// SBIS - Skip if Bit in I/O Register is Set 1001 1011 AAAA Abbb
									DO_INST(a5b3_sbis);
								}	break;
								default:
									switch (opcode & 0xfc00) {
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
			switch (opcode & 0xf800) {
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
			switch (opcode & 0xfe00) {
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
					DO_INST(d5b3_bld);
				}	break;
				case 0xfa00:
				case 0xfb00:{	// BST – Bit Store into T from bit in Register 1111 100r rrrr 0bbb
					DO_INST(d5b3_bst);
				}	break;
				case 0xfc00: {
					DO_INST(d5b3_sbrc);
				}	break;
				case 0xfe00: {	// SBRS/SBRC – Skip if Bit in Register is Set/Clear 1111 11sr rrrr 0bbb
					DO_INST(d5b3_sbrs);
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;
		default: _avr_invalid_opcode(avr);
	}

	avr->cycle += cycle[0];
//	return(new_pc[0]);
}


#define UINST_ESAC(name) case k_avr_uinst_##name: U_DO_UINST(call_##name); break

static inline void _avr_fast_core_run_one(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle) {
	new_pc[0] = avr->pc + 2;
	cycle[0] = 1;

	U_FETCH_OPCODE(opcode, avr->pc);
	UINST_GET_OP(u_opcode_op);

#if 0
	if(u_opcode_op & 0x80) {
		avr_instruction_t* i = (avr_instruction_t*)((opcode & 0xffffff00) | ((opcode & 0x7f) << 1));
		i->handler(avr, new_pc, cycle, i);
	} else 
#endif
	switch(u_opcode_op) {
		UINST_ESAC(d5r5_adc);
		UINST_ESAC(d5r5_add);
		UINST_ESAC(d5Wr5W_add_adc);
		UINST_ESAC(p2k6_adiw);
		UINST_ESAC(d5r5_and);
		UINST_ESAC(h4k8_andi);
		UINST_ESAC(h4k16_andi_andi);
		UINST_ESAC(h4r5k8_andi_or);
		UINST_ESAC(h4k8k8_andi_ori);
		UINST_ESAC(d5_asr);
		UINST_ESAC(b3_bclr);
		UINST_ESAC(d5b3_bld);
		UINST_ESAC(o7_brcs);
		UINST_ESAC(o7_brne);
		UINST_ESAC(o7b3_brxc);
		UINST_ESAC(o7b3_brxs);
		UINST_ESAC(b3_bset);
		UINST_ESAC(d5b3_bst);
		UINST_ESAC(x22_call);
		UINST_ESAC(a5b3_cbi);
		UINST_ESAC(d5_clr);
		UINST_ESAC(d5_com);
		UINST_ESAC(d5r5_cp);
		UINST_ESAC(d5Wr5W_cp_cpc);
		UINST_ESAC(d5r5_cpc);
		UINST_ESAC(h4k8_cpi);
		UINST_ESAC(h4r5k8_cpi_cpc);
		UINST_ESAC(d5r5_cpse);
		UINST_ESAC(d5_dec);
		UINST_ESAC(eicall);
		UINST_ESAC(eijmp);
		UINST_ESAC(d5r5_eor);
		UINST_ESAC(icall);
		UINST_ESAC(ijmp);
		UINST_ESAC(d5a6_in);
		UINST_ESAC(d5a6_in_push);
		UINST_ESAC(d5a6m8_in_sbrs);
		UINST_ESAC(d5_inc);
		UINST_ESAC(x22_jmp);
		UINST_ESAC(d5rXYZop_ld);
		UINST_ESAC(d5rXYZq6_ldd);
		UINST_ESAC(d5rXYZq6_ldd_ldd);
		UINST_ESAC(h4k8_ldi);
		UINST_ESAC(h4k16_ldi_ldi);
		UINST_ESAC(h4k8a6_ldi_out);
		UINST_ESAC(d5x16_lds);
		UINST_ESAC(d5x16_lds_lds);
		UINST_ESAC(d5x16_lds_tst);
		UINST_ESAC(d5_lpm_z0);
		UINST_ESAC(d5rXYZop_lpm_z0_st);
		UINST_ESAC(d5_lpm_z1);
		UINST_ESAC(d5_lpm_z1_lpm_z1);
		UINST_ESAC(d5_lsr);
		UINST_ESAC(d5_lsr_ror);
		UINST_ESAC(d5r5_mov);
		UINST_ESAC(d4r4_movw);
		UINST_ESAC(d5r5_mul);
		UINST_ESAC(d16r16_muls);
		UINST_ESAC(d5_neg);
		UINST_ESAC(nop);
		UINST_ESAC(d5r5_or);
		UINST_ESAC(h4k8_ori);
		UINST_ESAC(d5a6_out);
		UINST_ESAC(d5_pop);
		UINST_ESAC(d5a6_pop_out);
		UINST_ESAC(d5_pop_pop16be);
		UINST_ESAC(d5_pop_pop16le);
		UINST_ESAC(d5_push);
		UINST_ESAC(d5_push_push16be);
		UINST_ESAC(d5_push_push16le);
		UINST_ESAC(o12_rcall);
		UINST_ESAC(ret);
		UINST_ESAC(reti);
		UINST_ESAC(o12_rjmp);
		UINST_ESAC(d5_ror);
		UINST_ESAC(d5r5_sbc);
		UINST_ESAC(h4k8_sbci);
		UINST_ESAC(a5b3_sbi);
		UINST_ESAC(a5b3_sbic);
		UINST_ESAC(a5b3_sbis);
		UINST_ESAC(p2k6_sbiw);
		UINST_ESAC(d5b3_sbrc);
		UINST_ESAC(d5b3_sbrs);
		UINST_ESAC(sleep);
		UINST_ESAC(d5rXYZop_st);
		UINST_ESAC(d5rXYZq6_std);
		UINST_ESAC(d5rXYZq6_std_std_hhll);
		UINST_ESAC(d5rXYZq6_std_std_hllh);
		UINST_ESAC(d5x16_sts);
		UINST_ESAC(d5x16_sts_sts);
		UINST_ESAC(d5r5_sub);
		UINST_ESAC(h4k8_subi);
		UINST_ESAC(h4k16_subi_sbci);
		UINST_ESAC(d5_swap);
		UINST_ESAC(d5_tst);
		default:
			goto notFound;
			break;

	}

	avr->cycle += cycle[0];
	return;

notFound: /* run it through the decoder(which also runs the instruction), we'll (most likely) get it on the next run. */
	avr_decode_one(avr, new_pc, cycle);
}

avr_flashaddr_t avr_fast_core_run_one(avr_t* avr) {
	avr_flashaddr_t new_pc;
	int cycle;

	_avr_fast_core_run_one(avr, &new_pc, &cycle);
	return(new_pc);
}

extern void avr_core_run_many(avr_t* avr);
static void __attribute__((__unused__)) avr_core_run_many_sleep(avr_t* avr) {
	avr_cycle_count_t	nextTimerCycle;

	nextTimerCycle = avr_cycle_timer_process(avr);

	if (avr->state == cpu_Sleeping) {
		if (unlikely(!avr->sreg[S_I])) {
			if (avr->log)
				printf("simavr: sleeping with interrupts off, quitting gracefully\n");
			avr->state = cpu_Done;
			return;
		}
		/*
		 * try to sleep for as long as we can (?)
		 */
		avr->sleep(avr, nextTimerCycle);
		avr->cycle += 1 + nextTimerCycle;
	} else
		avr->run = avr_core_run_many;

	avr_service_interrupts(avr);
}

void avr_core_run_many(avr_t* avr) {
	fast_core_cycle_count_t	nextTimerCycle;
	avr_flashaddr_t		new_pc = avr->pc;
	int			cycle = 1;

	nextTimerCycle = avr_cycle_timer_process(avr);

	if(avr->state == cpu_Sleeping)
		goto leaveCore;

	do {
		if(likely(avr->state == cpu_Running)) {	
			_avr_fast_core_run_one(avr, &new_pc, &cycle);
			avr->pc = new_pc;
		} else
			goto leaveCore;

		nextTimerCycle -= cycle;
//		if(avr->sreg[S_I])
//			avr_service_interrupts(avr);
	} while((0 < cycle) && (0 < nextTimerCycle));
	
leaveCore:
#ifndef FAST_CORE_FAST_INTERRUPTS
	// if we just re-enabled the interrupts...
	// double buffer the I flag, to detect that edge
	if (avr->sreg[S_I] && !avr->i_shadow)
		avr->interrupts.pending_wait++;
	avr->i_shadow = avr->sreg[S_I];
#endif
	nextTimerCycle = avr_cycle_timer_process(avr);

	if (unlikely(avr->state == cpu_Sleeping)) {
		if (unlikely(!avr->sreg[S_I])) {
			if (avr->log)
				printf("simavr: sleeping with interrupts off, quitting gracefully\n");
			avr->state = cpu_Done;
			return;
		}
		/*
		 * try to sleep for as long as we can (?)
		 */
		avr->sleep(avr, nextTimerCycle);
		avr->cycle += 1 + nextTimerCycle;
	}

	avr_service_interrupts(avr);
}

void sim_fast_core_init(avr_t* avr) {
	/* avr program memory is 16 bits wide, byte addressed. */
	uint32_t flashsize = (avr->flashend + 1); // 2
	/* yes... we are using twice the space we need...  tradeoff to gain a
		few extra cycles. */
	uint32_t uflashsize = flashsize << 2; // 4,8,16,32,64
#ifndef FAST_CORE_USE_GLOBAL_FLASH_ACCESS
	avr->flash = realloc(avr->flash, flashsize + uflashsize);
	assert(0 != avr->flash);
	
	memset(&avr->flash[flashsize], 0, uflashsize);
#else
	_uflash = malloc(uflashsize);
	memset(_uflash, 0, uflashsize);
#endif
}

#endif

