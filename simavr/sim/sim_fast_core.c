#include <stdio.h>	// printf
#include <ctype.h>	// toupper
#include <arpa/inet.h> 	// byteorder macros
#include <stdlib.h>	// abort
#include <endian.h>

#include "sim_avr.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "avr_flash.h"
#include "avr_watchdog.h"

#define FAST_CORE_DECODE_TRAP
#define FAST_CORE_AGGRESSIVE_CHECKS

#define FAST_CORE_LOCAL_TRACE
#define FAST_CORE_ITRACE

#define _avr_sp_get _avr_sp_get_v2
#define _avr_sp_set _avr_sp_set_v2
#define _avr_push16 _avr_push16_v2

#define CONCAT(x, y) (x ## y)

#define xSTATE(_f, args...) { \
	printf("%06x: " _f, avr->pc, ## args);\
	}
#define xSREG() {\
	printf("%06x: \t\t\t\t\t\t\t\t\tSREG = ", avr->pc); \
	for (int _sbi = 0; _sbi < 8; _sbi++)\
		printf("%c", avr->sreg[_sbi] ? toupper(_sreg_bit_name[_sbi]) : '.');\
	printf("\n");\
}

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

static inline uint8_t _avr_data_read(avr_t* avr, uint16_t addr) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(addr > avr->ramend) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	return(avr->data[addr]);
}

static inline void _avr_data_write(avr_t* avr, uint16_t addr, uint8_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(addr > avr->ramend) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	avr->data[addr]=data;
}


#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint16_t _avr_bswap16le(uint16_t v) {
	return(v);
}

static inline uint16_t _avr_bswap16be(uint16_t v) {
	return(((v&0xff00)>>8)|((v&0x00ff)<<8));
}
#else
static inline uint16_t _avr_bswap16le(uint16_t v) {
	return(((v&0xff00)>>8)|((v&0x00ff)<<8));
}

static inline uint16_t _avr_bswap16be(uint16_t v) {
	return(v);
}
#endif

static inline uint16_t _avr_fetch16(void* p, uint16_t addr) {
	return(*((uint16_t*)&((uint8_t *)p)[addr]));
}

static inline void _avr_store16(void*p, uint16_t addr, uint16_t data) {
	*((uint16_t*)&((uint8_t *)p)[addr])=data;
}

static inline uint16_t _avr_data_read16(avr_t* avr, uint16_t addr) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if((addr + 1) > avr->ramend) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	return(_avr_fetch16(avr->data, addr));
}

static inline void _avr_data_write16(avr_t* avr, uint16_t addr, uint16_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if((addr + 1) > avr->ramend) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	_avr_store16(avr->data, addr, data);
}

static inline uint16_t _avr_data_read16le(avr_t* avr, uint16_t addr) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if((addr + 1) > avr->ramend) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	return(_avr_bswap16le(_avr_fetch16(avr->data, addr)));
}

static inline void _avr_data_write16le(avr_t* avr, uint16_t addr, uint16_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if((addr + 1) > avr->ramend) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	_avr_store16(avr->data, addr, _avr_bswap16le(data));
}

/*
 * Stack pointer access
 */
static inline uint16_t _avr_sp_get(avr_t * avr)
{
	return(_avr_data_read16le(avr, R_SPL));
}

static inline void _avr_sp_set(avr_t * avr, uint16_t sp)
{
	_avr_data_write16le(avr, R_SPL, sp);
}

/*
 * Register access funcitons
 */
static inline uint8_t _avr_get_r(avr_t* avr, uint8_t reg) {
	return(_avr_data_read(avr, reg));
}

static inline void _avr_set_r(avr_t* avr, uint8_t reg, uint8_t v) {
	_avr_data_write(avr, reg, v);
}

static inline uint16_t _avr_get_r16(avr_t* avr, uint8_t addr) {
	return(_avr_data_read16(avr, addr));
}

static inline void _avr_set_r16(avr_t* avr, uint8_t addr, uint16_t data) {
	_avr_data_write16(avr, addr, data);
}

static inline uint16_t _avr_get_r16le(avr_t* avr, uint8_t addr) {
	return(_avr_data_read16le(avr, addr));
}

static inline void _avr_set_r16le(avr_t* avr, uint8_t addr, uint16_t data) {
	_avr_data_write16le(avr, addr, data);
}


/*
 * Flash accessors
 */
static inline uint16_t _avr_flash_read16le(avr_t* avr, uint16_t addr) {
	return(_avr_bswap16le(_avr_fetch16(avr->flash, addr)));
}

static inline uint16_t _avr_flash_read16be(avr_t* avr, uint16_t addr) {
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

/*
 * Set any address to a value; split between registers and SRAM
 */
static inline void _avr_set_ram(avr_t * avr, uint16_t addr, uint8_t v)
{
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(addr > avr->ramend) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	if (addr < 256) {
		REG_TOUCH(avr, addr);

		if (addr == R_SREG) {
			_avr_data_write(avr, addr, v);
			// unsplit the SREG
			SET_SREG_FROM(avr, v);
			SREG();
		}
		if (addr > 31) {
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
		} else
			_avr_data_write(avr, addr, v);
	} else
		avr_core_watch_write(avr, addr, v);
}

/*
 * Get a value from SRAM.
 */
static inline uint8_t _avr_get_ram(avr_t * avr, uint16_t addr)
{
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(addr > avr->ramend) {
		printf("%s: access at 0x%04x past end of ram, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif
	if (addr == R_SREG) {
		/*
		 * SREG is special it's reconstructed when read
		 * while the core itself uses the "shortcut" array
		 */
		uint8_t sreg = _avr_data_read(avr, R_SREG);
		READ_SREG_INTO(avr, sreg);
	} else if (addr > 31 && addr < 256) {
		uint8_t io = AVR_DATA_TO_IO(addr);
		
		if (avr->io[io].r.c)
			_avr_data_write(avr, addr, avr->io[io].r.c(avr, addr, avr->io[io].r.param));

		if (avr->io[io].irq) {
			uint8_t v = _avr_data_read(avr, addr);
			avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
			for (int i = 0; i < 8; i++)
				avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);				
		}
	}
	return avr_core_watch_read(avr, addr);
}

/*
 * Stack push accessors. Push/pop 8 and 16 bits
 */
static inline void _avr_push8(avr_t * avr, uint16_t v)
{
	uint16_t sp = _avr_sp_get(avr);
	_avr_set_ram(avr, sp, v);
	_avr_sp_set(avr, sp-1);
}

static inline uint8_t _avr_pop8(avr_t * avr)
{
	uint16_t sp = _avr_sp_get(avr) + 1;
	uint8_t res = _avr_get_ram(avr, sp);
	_avr_sp_set(avr, sp);
	return res;
}

static inline void _avr_push16(avr_t * avr, uint16_t v)
{
	_avr_push8(avr, v);
	_avr_push8(avr, v >> 8);
}

static inline uint16_t _avr_pop16(avr_t * avr)
{
	uint16_t res = _avr_pop8(avr) << 8;
	res |= _avr_pop8(avr);
	return res;
}

static inline int _avr_is_instruction_32_bits(avr_t * avr, avr_flashaddr_t pc)
{
	uint16_t o = (_avr_flash_read16le(avr, pc)) & 0xfc0f;
	
	return	o == 0x9200 || // STS ! Store Direct to Data Space
			o == 0x9000 || // LDS Load Direct from Data Space
			o == 0x940c || // JMP Long Jump
			o == 0x940d || // JMP Long Jump
			o == 0x940e ||  // CALL Long Call to sub
			o == 0x940f; // CALL Long Call to sub
}

static inline void _avr_flags_zc16(avr_t* avr, const uint16_t res) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = (res >> 15) & 1;
}

static inline void _avr_flags_zcn0vs(avr_t* avr, const uint8_t res, const uint8_t vr) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vr & 1;
	avr->sreg[S_N] = 0;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_flags_zns(avr_t* avr, const uint8_t res) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

#if 1
static inline void _avr_flags_Zns(avr_t* avr, const uint8_t res) {
	if (res)
		avr->sreg[S_Z] = 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}
#else
static inline void _avr_flags_Zns(avr_t* avr, const uint8_t res) {
	_avr_flags_zns(avr, res);
}
#endif

static inline void _avr_flags_zns16(avr_t* avr, const uint16_t res) {
	avr->sreg[S_Z] = (res & 0xffff) == 0;
	avr->sreg[S_N] = (res >> 15) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_flags_znv0s(avr_t* avr, const uint8_t res) {
	avr->sreg[S_V] = 0;
#if 0
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
#else
	_avr_flags_zns(avr, res);
#endif
}

#if 0
static inline void _avr_flags_add_carry(avr_t* avr, const uint8_t res, const uint8_t rd, const uint8_t rr) {
	uint8_t result = ((rd & rr) | (rr & ~res) | (~res & rd));

	avr->sreg[S_H] = ((result & 0x04) >> 3);
	avr->sreg[S_C] = ((result & 0x80) >> 7);
}

static inline void _avr_flags_add_overflow(avr_t* avr, const uint8_t res, const uint8_t rd, const uint8_t rr) {
	uint8_t result = (rd & rr & res) | (~rd & ~rr & res);

	avr->sreg[S_V] = (result & 0x80) >> 7;
}
#else
static inline void _avr_flags_add_carry(avr_t* avr, const uint8_t res, const uint8_t rd, const uint8_t rr) {
	uint8_t	result_rd_rr = (rd & rr);

	uint8_t result_carry = (result_rd_rr | (rr & ~res) | (~res & rd));
	avr->sreg[S_H] = ((result_carry & 0x04) >> 3);
	avr->sreg[S_C] = ((result_carry & 0x80) >> 7);

	uint8_t result_overflow = ((result_rd_rr & res) | (~result_rd_rr & res));
	avr->sreg[S_V] = (result_overflow & 0x80) >> 7;
	
}
static inline void _avr_flags_add_overflow(avr_t* avr, const uint8_t res, const uint8_t rd, const uint8_t rr) {
}
#endif

static inline void _avr_flags_add16_carry(avr_t* avr, const uint16_t res, const uint16_t rd, const uint16_t rr) {
	uint16_t result_rd_rr = (rd & rr);

	uint16_t result_carry = (result_rd_rr | (rr & ~res) | (~res & rd));
	avr->sreg[S_H] = ((result_carry & 0x0400) >> 11);
	avr->sreg[S_C] = ((result_carry & 0x8000) >> 15);

	uint16_t result_overflow = ((result_rd_rr & res) | (~result_rd_rr & res));
	avr->sreg[S_V] = (result_overflow & 0x8000) >> 15;
	
}
static inline void _avr_flags_add16_overflow(avr_t* avr, const uint16_t res, const uint16_t rd, const uint16_t rr) {
}

static inline uint8_t _get_sub_carry(const uint8_t res, const uint8_t rd, const uint8_t rr) {
	return((~rd & rr) | (rr & res) | (res & ~rd));
}
static inline uint8_t _get_sub_overflow(const uint8_t res, const uint8_t rd, const uint8_t rr) {
	return((rd & ~rr & ~res) | (~rd & rr & res));
}


#if 0
static inline void _avr_flags_sub_carry(avr_t* avr, const uint8_t res, const uint8_t rd, const uint8_t rr) {
	uint8_t result = _get_sub_carry(res, rd, rr);

	avr->sreg[S_H] = ((result & 0x04) >> 3);
	avr->sreg[S_C] = ((result & 0x80) >> 7);
}

static inline void _avr_flags_sub_overflow(avr_t* avr, const uint8_t res, const uint8_t rd, const uint8_t rr) {
	uint8_t result = _get_sub_overflow(res, rd, rr);

	avr->sreg[S_V] = (result & 0x80) >> 7;
}
#else
static inline void _avr_flags_sub_carry(avr_t* avr, const uint8_t res, const uint8_t rd, const uint8_t rr) {
	uint8_t	result_rr_res = (rr & res);

	uint8_t result_carry = ((~rd & rr) | result_rr_res | (res & ~rd));
	avr->sreg[S_H] = ((result_carry & 0x04) >> 3);
	avr->sreg[S_C] = ((result_carry & 0x80) >> 7);

	uint8_t	result_overflow = ((rd & ~result_rr_res) | (~rd & result_rr_res));
	avr->sreg[S_V] = (result_overflow & 0x80) >> 7;
}
static inline void _avr_flags_sub_overflow(avr_t* avr, const uint8_t res, const uint8_t rd, const uint8_t rr) {
}
#endif

static inline void _avr_flags_sub16_carry(avr_t* avr, const uint16_t res, const uint16_t rd, const uint16_t rr) {
	uint16_t result_rr_res = (rr & res);

	uint16_t result_carry = ((~rd & rr) | result_rr_res | (res & ~rd));
	avr->sreg[S_H] = ((result_carry & 0x0400) >> 11);
	avr->sreg[S_C] = ((result_carry & 0x8000) >> 15);

	uint16_t	result_overflow = ((rd & ~result_rr_res) | (~rd & result_rr_res));
	avr->sreg[S_V] = (result_overflow & 0x8000) >> 15;
}
static inline void _avr_flags_sub16_overflow(avr_t* avr, const uint16_t res, const uint16_t rd, const uint16_t rr) {
}


static inline uint8_t OPCODEop(uint32_t opcode) {
	return(opcode&0x000000ff);
}

static inline uint8_t OPCODEr1(uint32_t opcode) {
	return((opcode&0x0000ff00)>>8);
}

static inline uint8_t OPCODEr2(uint32_t opcode) {
	return((opcode&0x00ff0000)>>16);
}

static inline uint8_t OPCODEr3(uint32_t opcode) {
	return((opcode&0xff000000)>>24);
}

static inline int16_t OPCODEx16(uint32_t opcode) {
	return((opcode&0xffff0000)>>16);
}

#define UINST(name) \
	static inline void _avr_uinst##name(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle)

#define UINSTarg(name, args...) \
	static inline void _avr_uinst_##name(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle, ##args)

#define UINSTb3(name) \
	UINSTarg(b3##name, const uint8_t b)

#define UINSTd5(name) \
	UINSTarg(d5##name, const uint8_t d)

#define UINSTbIO(name)  \
	UINSTarg(bIO##name, const uint8_t io, const uint8_t b)

#define UINSTd5a6(name) \
	UINSTarg(d5a6##name, const uint8_t d, const uint8_t a)

#define UINSTd5a6b3(name) \
	UINSTarg(d5a6b3##name, const uint8_t d, const uint8_t a, const uint8_t b)

#define UINSTd5b3(name) \
	UINSTarg(d5b3##name, const uint8_t d, const uint8_t b)

#define UINSTd4r4(name) \
	UINSTarg(d4r4##name, const uint8_t d, const uint8_t r)

#define UINSTd5r5(name) \
	UINSTarg(d5r5##name, const uint8_t d, const uint8_t r)

#define UINSTd5Wr5W(name) \
	UINSTarg(d5Wr5W##name, const uint8_t d, const uint8_t r)

#define UINSTd16r16(name) \
	UINSTarg(d16r16##name, const uint8_t d, const uint8_t r)

#define UINSTd5rXYZop(name) \
	UINSTarg(d5rXYZop##name, const uint8_t d, const uint8_t r, const uint8_t op)

#define UINSTd5rXYZq6(name) \
	UINSTarg(d5rXYZq6##name, const uint8_t d, const uint8_t r, const uint8_t q)

#define UINSTd5x16(name) \
	UINSTarg(d5x16##name, const uint8_t d, const uint16_t x)

#define UINSTh4k8(name) \
	UINSTarg(h4k8##name, const uint8_t h, const uint8_t k)

#define UINSTh4k16(name) \
	UINSTarg(h4k16##name, const uint8_t h, const uint16_t k)

#define UINSTh4r5k8(name) \
	UINSTarg(h4r5k8##name, const uint8_t h, const uint8_t r, const uint8_t k)

#define UINSTo7b3(name) \
	UINSTarg(o7b3##name, const int8_t o, const uint8_t b)

#define UINSTo12(name)  \
	UINSTarg(o12##name, const int16_t o)

#define UINSTp2k6(name) \
	UINSTarg(p2k6##name, const uint8_t p, const uint8_t k)

#define UINSTx24(name)  \
	UINSTarg(x24##name, const uint8_t x6, const uint16_t x16)

UINSTd5r5(_add) {
	const uint8_t vd = _avr_get_r(avr, d);
	const uint8_t vr = _avr_get_r(avr, r);
	const uint8_t res = vd + vr;

	if (r == d) {
		STATE("lsl %s[%02x] = %02x\n", avr_regname(d), vd, res & 0xff);
	} else {
		STATE("add %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
	}

	_avr_set_r(avr, d, res);

	_avr_flags_add_carry(avr, res, vd, vr);
	_avr_flags_add_overflow(avr, res, vd, vr);

	_avr_flags_zns(avr, res);

	SREG();
}

UINSTd5Wr5W(_add) {
	const uint16_t vd = _avr_get_r16le(avr, d);
	const uint16_t vr = _avr_get_r16le(avr, r);
	const uint16_t res = vd + vr;

	if (r == d) {
		STATE("lsl.w %s[%04x] = %04x\n", avr_regname(d), vd, res & 0xff);
	} else {
		STATE("add.w %s:%s[%04x], %s:%s[%04x] = %04x\n", avr_regname(d), avr_regname(d+1), vd, 
			avr_regname(r), avr_regname(r+1), vr, res);
	}

	_avr_set_r16le(avr, d, res);

	_avr_flags_add16_carry(avr, res, vd, vr);
	_avr_flags_add16_overflow(avr, res, vd, vr);

	_avr_flags_zns16(avr, res);

	new_pc[0] += 2; cycle[0] ++;

	SREG();
}

UINSTp2k6(_adiw) {
	uint16_t vp = _avr_get_r16le(avr, p);
	uint32_t res = vp + k;

	STATE("adiw %s:%s[%04x], 0x%02x = 0x%04x\n", avr_regname(p), avr_regname(p+1), vp, k, res);

	_avr_set_r16le(avr, p, res);

	avr->sreg[S_V] = (((~vp) & res) & 0x8000) >> 15;
	avr->sreg[S_C] = (((~res) & vp) & 0x8000) >> 15;

	_avr_flags_zns16(avr, res);

	SREG();

	cycle[0]++;
}

UINSTd5r5(_adc) {
	const uint8_t vd = _avr_get_r(avr, d);
	const uint8_t vr = _avr_get_r(avr, r);

	const uint8_t res = vd + vr + avr->sreg[S_C];

	if (r == d) {
		STATE("rol %s[%02x] = %02x\n", avr_regname(d), vd, res);
	} else {
		STATE("addc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
	}

	_avr_set_r(avr, d, res);

	_avr_flags_add_carry(avr, res, vd, vr);
	_avr_flags_add_overflow(avr, res, vd, vr);

	_avr_flags_zns(avr, res);

	SREG();
}

UINSTd5r5(_and) {
	const uint8_t vd = _avr_get_r(avr, d);
	const uint8_t vr = _avr_get_r(avr, r);

	const uint8_t res = vd & vr;

	if (r == d) {
		STATE("tst %s[%02x]\n", avr_regname(d), vd);
	} else {
		STATE("and %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
	}

	_avr_set_r(avr, d, res);

	_avr_flags_znv0s(avr, res);

	SREG();
}

UINSTh4k8(_andi) {
	uint8_t vh = _avr_get_r(avr, h);
	uint8_t res = vh & k;

	STATE("andi %s[%02x], 0x%02x\n", avr_regname(h), vh, k);

	_avr_set_r(avr, h, res);

	_avr_flags_znv0s(avr, res);

	SREG();
}

UINSTd5(_asr) {
	uint8_t vd = _avr_get_r(avr, d);

	uint8_t res = (vd >> 1) | (vd & 0x80);

	STATE("asr %s[%02x]\n", avr_regname(d), vd);

	_avr_set_r(avr, d, res);
	
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vd & 1;
	avr->sreg[S_N] = res >> 7;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];

	SREG();
}

UINSTb3(_bclr) {
	avr->sreg[b]=0;
	STATE("bclr %c\n", _sreg_bit_name[b]);
	SREG();
}

UINSTd5b3(_bld) {
	uint8_t vd = _avr_get_r(avr, d);

	uint8_t res = vd | (avr->sreg[S_T] ? (1 << b) : 0);

	STATE("bld %s[%02x], 0x%02x = %02x\n", avr_regname(d), vd, 1 << b, res);

	_avr_set_r(avr, d, res);
}

UINSTo7b3(_brxc) {
	int 		branch = (0 == avr->sreg[b]);
	avr_flashaddr_t	branch_pc = new_pc[0] + o;

	const char *names[8] = {
		"brcc", "brne", "brpl", "brvc", NULL, "brhc", "brtc", "brid"
	};

	if (names[b]) {
		STATE("%s .%d [%04x]\t; Will%s branch\n", names[b], o, branch_pc, branch ? "":" not");
	} else {
		STATE("%s%c .%d [%04x]\t; Will%s branch\n", 0 ? "brbs" : "brbc", _sreg_bit_name[b], o, branch_pc, branch ? "":" not");
	}
	if (branch) {
		cycle[0]++; // 2 cycles if taken, 1 otherwise
		new_pc[0] = branch_pc;
	}
}

UINSTo7b3(_brxs) {
	int		branch = (0 != avr->sreg[b]);
	avr_flashaddr_t	branch_pc = new_pc[0] + o;

	const char *names[8] = {
		"brcs", "breq", "brmi", "brvs", NULL, "brhs", "brts", "brie"
	};
	if (names[b]) {
		STATE("%s .%d [%04x]\t; Will%s branch\n", names[b], o, branch_pc, branch ? "":" not");
	} else {
		STATE("%s%c .%d [%04x]\t; Will%s branch\n", 1 ? "brbs" : "brbc", _sreg_bit_name[b], o, branch_pc, branch ? "":" not");
	}
	if (branch) {
		cycle[0]++; // 2 cycles if taken, 1 otherwise
		new_pc[0] = branch_pc;
	}
}

UINSTb3(_bset) {
	avr->sreg[b]=1;
	STATE("bset %c\n", _sreg_bit_name[b]);
	SREG();
}

UINSTd5b3(_bst) {
	uint8_t vd = _avr_get_r(avr, d);

	uint8_t res = (vd >> b) & 1;

	STATE("bst %s[%02x], 0x%02x = %02x\n", avr_regname(d), vd, 1 << b, res);
	
	avr->sreg[S_T] = res;

	SREG();
}

UINSTx24(_call) {
	uint32_t ea = ((x6 << 16) | x16) << 1;

	STATE("call 0x%06x\n", ea);
	
	new_pc[0] += 2;
	_avr_push16(avr, new_pc[0] >> 1);
	new_pc[0] = ea;
	
	cycle[0] += 3;	// 4 cycles; FIXME 5 on devices with 22 bit PC
	TRACE_JUMP();
	STACK_FRAME_PUSH();
}

UINSTbIO(_cbi) {
	uint8_t vio = _avr_get_ram(avr, io);
	uint8_t res = vio & ~(1 << b);

	STATE("cbi %s[%04x], 0x%02x = %02x\n", avr_regname(io), vio, 1<<b, res);

	_avr_set_ram(avr, io, res);

	cycle[0]++;
}

UINSTd5(_com) {
	uint8_t vd = _avr_get_r(avr, d);
	uint8_t res = 0xff - vd;

	STATE("com %s[%02x] = %02x\n", avr_regname(d), vd, res);

	_avr_set_r(avr, d, res);

	avr->sreg[S_C] = 1;

	_avr_flags_znv0s(avr, res);

	SREG();
}

UINSTd5r5(_cp) {
	uint8_t vd = _avr_get_r(avr, d);
	uint8_t vr = _avr_get_r(avr, r);

	uint8_t res = vd - vr;

	STATE("cp %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_flags_sub_carry(avr, res, vd, vr);
	_avr_flags_sub_overflow(avr, res, vd, vr);

	_avr_flags_zns(avr, res);

	SREG();
}

UINSTd5Wr5W(_cp) {
	uint16_t vd = _avr_get_r16le(avr, d);
	uint16_t vr = _avr_get_r16le(avr, r);

	uint16_t res = vd - vr;

	STATE("cp.w %s:%s[%04x], %s:%s[%04x] = %04x\n", avr_regname(d), avr_regname(d+1), vd, 
		avr_regname(r), avr_regname(r+1), vr, res);

	_avr_flags_sub16_carry(avr, res, vd, vr);
	_avr_flags_sub16_overflow(avr, res, vd, vr);

	_avr_flags_zns16(avr, res);

	new_pc[0] += 2; cycle[0] ++;

	SREG();
}

UINSTd5r5(_cpc) {
	uint8_t vd = _avr_get_r(avr, d);
	uint8_t vr = _avr_get_r(avr, r);

	uint8_t res = vd - vr - avr->sreg[S_C];

	STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_flags_sub_carry(avr, res, vd, vr);
	_avr_flags_sub_overflow(avr, res, vd, vr);

	_avr_flags_Zns(avr, res);

	SREG();
}

UINSTh4k8(_cpi) {
	uint8_t vh = _avr_get_r(avr, h);

	uint8_t res = vh - k;

	STATE("cpi %s[%02x], 0x%02x = 0x%02x\n", avr_regname(h), vh, k, res);

	_avr_flags_sub_carry(avr, res, vh, k);
	_avr_flags_sub_overflow(avr, res, vh, k);

	_avr_flags_zns(avr, res);

	SREG();
}

UINSTh4r5k8(_cpi) {
	uint16_t vh = _avr_get_r16le(avr, h);
	uint16_t vr = (_avr_get_r(avr,r) << 8) | k;
	uint16_t res = vh - vr;

	STATE("cpi.w %s:%s[%04x], 0x%04x = 0x%04x\n", avr_regname(h), avr_regname(h+1), vh, vr, res);
	
	_avr_flags_sub16_carry(avr, res, vh, vr);
	_avr_flags_sub16_overflow(avr, res, vh, vr);

	/* if we are performing a 16 bit comparison (cpi followed by an cpc, follow flags accord to cpc */
	_avr_flags_zns16(avr, res);

	new_pc[0] += 2; cycle[0] ++;

	SREG();
}


UINSTd5r5(_cpse) {
	uint8_t vd = _avr_get_r(avr, d);
	uint8_t vr = _avr_get_r(avr, r);

	uint16_t res = vd == vr;

	STATE("cpse %s[%02x], %s[%02x]\t; Will%s skip\n", avr_regname(d), vd, avr_regname(r), vr, res ? "":" not");

	if (res) {

		if (_avr_is_instruction_32_bits(avr, new_pc[0])) {
			new_pc[0] += 4;
			cycle[0] += 2;
		} else {
			new_pc[0] += 2;
			cycle[0]++;
		}
	}
}

UINSTd5(_dec) {
	uint8_t vd = _avr_get_r(avr, d);
	uint8_t res = vd - 1;

	STATE("dec %s[%02x] = %02x\n", avr_regname(d), vd, res);

	_avr_set_r(avr, d, res);

	avr->sreg[S_V] = res == 0x80;

	_avr_flags_zns(avr, res);

	SREG();
}

UINSTd5r5(_eor) {
	uint8_t vd = _avr_get_r(avr, d);
	uint8_t vr = _avr_get_r(avr, r);

	uint8_t res = vd ^ vr;

	if (r==d) {
		STATE("clr %s[%02x]\n", avr_regname(d), vd);
	} else {
		STATE("eor %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
	}

	_avr_set_r(avr, d, res);

	_avr_flags_znv0s(avr, res);

	SREG();
}

UINSTd5a6(_in) {
	uint8_t	va = _avr_get_ram(avr, a);

	STATE("in %s, %s[%02x]\n", avr_regname(d), avr_regname(a), va);

	_avr_set_r(avr, d, va);
}

UINSTd5a6b3(_in_sbrs) {
	uint8_t	va = _avr_get_ram(avr, a);

	STATE("in %s, %s[%02x]; ", avr_regname(d), avr_regname(a), va);

	_avr_set_r(avr, d, va);

	new_pc[0] += 2; cycle[0]++;

	int	branch = (0 != (va & (1 << b)));

	STATE("%s %s[%02x], 0x%02x\t; Will%s branch\n", 1 ? "sbrs" : "sbrc", avr_regname(d), va, 1 << b, branch ? "":" not");

	if (branch) {
		if (_avr_is_instruction_32_bits(avr, new_pc[0])) {
			new_pc[0] += 4; cycle[0] += 2;
		} else {
			new_pc[0] += 2; cycle[0] ++;
		}
	}

}

UINSTd5(_inc) {
	uint8_t vd = _avr_get_r(avr, d);

	uint8_t res = vd + 1;

	STATE("inc %s[%02x] = %02x\n", avr_regname(d), vd, res);

	_avr_set_r(avr, d, res);

	avr->sreg[S_V] = res == 0x7f;

	_avr_flags_zns(avr, res);

	SREG();
}

UINSTx24(_jmp) {
	uint32_t ea = ((x6 << 8) | x16) << 1;
	STATE("jmp 0x%06x\n", ea);
	new_pc[0] = ea;
	cycle[0] += 2;
	TRACE_JUMP();
}

UINSTd5rXYZop(_ld) {
	uint16_t vr = _avr_get_r16le(avr, r);
	uint8_t ivr;

	cycle[0]++; // 2 cycles (1 for tinyavr, except with inc/dec 2)

	if (op == 2) vr--;
	_avr_set_r(avr, d, ivr = _avr_get_ram(avr, vr));
	if (op == 1) vr++;

	STATE("ld %s, %s%s[%04x]%s\n", avr_regname(d), op == 2 ? "--" : "", avr_regname(r), vr, op == 1 ? "++" : "");

	if(op)
		_avr_set_r16le(avr, r, vr);
}

UINSTd5rXYZq6(_ldd) {
	uint16_t vr = _avr_get_r16le(avr, r) + q;
	uint8_t ivr;

	cycle[0]++; // 2 cycles (1 for tinyavr, except with inc/dec 2)

	_avr_set_r(avr, d, ivr = _avr_get_ram(avr, vr));

	STATE("ld %s, (%s+%d[%04x])=[%02x]\n", avr_regname(d), avr_regname(r), q, vr, ivr);
}

UINSTh4k8(_ldi) {
	STATE("ldi %s, 0x%02x\n", avr_regname(h), k);

	_avr_set_r(avr, h, k);
}

UINSTh4k16(_ldi) {
	STATE("ldi.w %s, 0x%04x\n", avr_regname(h), k);

	_avr_set_r16le(avr, h, k);

	new_pc[0] += 2; cycle[0]++;
}

UINSTd5x16(_lds) {
	new_pc[0] += 2;

	STATE("lds %s[%02x], 0x%04x\n", avr_regname(d), _avr_get_r(avr, d), x);

	_avr_set_r(avr, d, _avr_get_ram(avr, x));

	cycle[0]++; // 2 cycles
}

UINSTd5(_lpm_z0) {
	uint16_t z = _avr_get_r16le(avr, R_ZL);

	STATE("lpm %s, (Z[%04x])\n", avr_regname(d), z);

	_avr_set_r(avr, d, avr->flash[z]);
	
	cycle[0] += 2; // 3 cycles
}

UINSTd5rXYZop(_lpm_z0_st) {
	uint16_t z = _avr_get_r16le(avr, R_ZL);
	uint8_t vd = avr->flash[z];

	STATE("lpm %s, (Z[%04x]); ", avr_regname(d), z);

	_avr_set_r(avr, d, vd);
	
	new_pc[0] +=2; cycle[0] += 2; // 3 cycles

	uint16_t vr = _avr_get_r16le(avr, r);

	STATE("st %s%s:%s[%04x]%s, %s[%02x] \n", op == 2 ? "--" : "", avr_regname(r), avr_regname(r+1), vr, \
		op == 1 ? "++" : "", avr_regname(d), vd);

	cycle[0]++; // 2 cycles, except tinyavr

	if (op == 2) vr--;
	_avr_set_ram(avr, vr, vd);
	if (op == 1) vr++;

	if(op)
		_avr_set_r16le(avr, r, vr);
}

UINSTd5(_lpm_z1) {
	uint16_t z = _avr_get_r16le(avr, R_ZL);

	STATE("lpm %s, (Z[%04x]+)\n", avr_regname(d), z);

	_avr_set_r(avr, d, avr->flash[z]);

	_avr_set_r16le(avr, R_ZL, z+1);

	cycle[0] += 2; // 3 cycles
}

UINSTd5(_lsr) {
	uint8_t vd = _avr_get_r(avr, d);

	uint8_t res = vd >> 1;

	STATE("lsr %s[%02x]\n", avr_regname(d), vd);

	_avr_set_r(avr, d, res);

	_avr_flags_zcn0vs(avr, res, vd);

	SREG();
}

UINSTd5r5(_mov) {
	uint8_t vr = _avr_get_r(avr, r);

	uint8_t res = vr;

	STATE("mov %s, %s[%02x] = %02x\n", avr_regname(d), avr_regname(r), vr, res);

	_avr_set_r(avr, d, res);
}

UINSTd4r4(_movw) {
	uint16_t vr = _avr_get_r16(avr, r);

	STATE("movw %s:%s, %s:%s[%04x]\n", avr_regname(d), avr_regname(d+1), avr_regname(r), avr_regname(r+1), _avr_bswap16le(vr));

	_avr_set_r16(avr, d, vr);
}

UINSTd5r5(_mul) {
	uint8_t vd = _avr_get_r(avr, d);
	uint8_t vr = _avr_get_r(avr, r);

	uint16_t res = vd * vr;

	STATE("mul %s[%02x], %s[%02x] = %04x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	cycle[0]++;

	_avr_set_r16le(avr, 0, res);

	_avr_flags_zc16(avr, res);

	SREG();
}

UINSTd16r16(_muls) {
	int8_t vd = _avr_get_r(avr, d);
	int8_t vr = _avr_get_r(avr, r);

	int16_t res = vr * vd;

	STATE("muls %s[%d], %s[%02x] = %d\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_set_r16le(avr, 0, res);

	_avr_flags_zc16(avr, res);

	SREG();
}

UINSTd5(_neg) {
	uint8_t vd = _avr_get_r(avr, d);

	uint8_t res = 0x00 - vd;

	STATE("neg %s[%02x] = %02x\n", avr_regname(d), vd, res);

	_avr_set_r(avr, d, res);

	avr->sreg[S_H] = ((res | vd) >> 3) & 1;
	avr->sreg[S_V] = res == 0x80;
	avr->sreg[S_C] = res != 0;

	_avr_flags_zns(avr, res);

	SREG();
}

UINST(_nop) {
	STATE("nop\n");
}

UINSTd5r5(_or) {
	uint8_t vd = _avr_get_r(avr, d);
	uint8_t vr = _avr_get_r(avr, r);

	uint8_t res = vd | vr;

	STATE("or %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_set_r(avr, d, res);

	_avr_flags_znv0s(avr, res);

	SREG();
}

UINSTh4k8(_ori) {
	uint8_t vh = _avr_get_r(avr, h);

	uint8_t res = vh | k;

	STATE("ori %s[%02x], 0x%02x\n", avr_regname(h), vh, k);

	_avr_set_r(avr, h, res);

	_avr_flags_znv0s(avr, res);

	SREG();
}

UINSTd5a6(_out) {
	uint8_t vd = _avr_get_r(avr, d);

	STATE("out %s, %s[%02x]\n", avr_regname(a), avr_regname(d), vd);

	_avr_set_ram(avr, a, vd);
}

UINSTd5(_pop) {
	uint8_t	vd;
	_avr_set_r(avr, d, vd=_avr_pop8(avr));

	T(uint16_t sp = _avr_sp_get(avr);)
	STATE("pop %s (@%04x)[%02x]\n", avr_regname(d), sp, vd);
	
	cycle[0]++;
}

UINSTd5(_push) {
	uint8_t	vd = _avr_get_r(avr, d);

	_avr_push8(avr, vd);

	T(uint16_t sp = _avr_sp_get(avr);)
	STATE("push %s[%02x] (@%04x)\n", avr_regname(d), vd, sp);

	cycle[0]++;
}

UINSTo12(_rcall) {
	avr_flashaddr_t branch_pc = new_pc[0] + o;

	STATE("rcall .%d [%04x]\n", o, branch_pc);

	_avr_push16(avr, new_pc[0] >> 1);

	new_pc[0] = branch_pc;
	cycle[0] += 2;
	// 'rcall .1' is used as a cheap "push 16 bits of room on the stack"
	if (o != 0) {
		TRACE_JUMP();
		STACK_FRAME_PUSH();
	}
}

UINST(_ret) {
	new_pc[0] = _avr_pop16(avr) << 1;
	cycle[0] += 3;
	STATE("ret\n");
	TRACE_JUMP();
	STACK_FRAME_POP();
}

UINST(_reti) {
	new_pc[0] = _avr_pop16(avr) << 1;
	avr->sreg[S_I] = 1;
	cycle[0] += 3;
	STATE("reti\n");
	TRACE_JUMP();
	STACK_FRAME_POP();
}

UINSTo12(_rjmp) {
	avr_flashaddr_t	branch_pc = new_pc[0] + o;

	STATE("rjmp .%d [%04x]\n", o, branch_pc);

	new_pc[0] = branch_pc;
	cycle[0]++;
	TRACE_JUMP();
}

UINSTd5(_ror) {
	uint8_t vd = _avr_get_r(avr, d);

	uint8_t res = (avr->sreg[S_C] ? 0x80 : 0) | vd >> 1;

	STATE("ror %s[%02x]\n", avr_regname(d), vd);

	_avr_set_r(avr, d, res);

	_avr_flags_zcn0vs(avr, res, vd);

	SREG();
}

UINSTd5r5(_sbc) {
	uint8_t vd = _avr_get_r(avr, d);
	uint8_t vr = _avr_get_r(avr, r);

	uint8_t res = vd - vr - avr->sreg[S_C];

	STATE("sbc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_set_r(avr, d, res);

	_avr_flags_sub_carry(avr, res, vd, vr);
	_avr_flags_sub_overflow(avr, res, vd, vr);

	_avr_flags_Zns(avr, res);

	SREG();
}

UINSTbIO(_sbi) {
	uint8_t vio =_avr_get_ram(avr, io);
	uint8_t res = vio | (1 << b);

	STATE("sbi %s[%04x], 0x%02x = %02x\n", avr_regname(io), vio, 1<<b, res);

	_avr_set_ram(avr, io, res);

	cycle[0]++;
}

UINSTbIO(_sbic) {
	uint8_t vio = _avr_get_ram(avr, io);
	uint8_t res = vio & (1 << b);

	STATE("sbic %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), vio, 1<<b, !res?"":" not");

	if (!res) {
		if (_avr_is_instruction_32_bits(avr, new_pc[0])) {
			new_pc[0] += 4; cycle[0] += 2;
		} else {
			new_pc[0] += 2; cycle[0]++;
		}
	}
}

UINSTbIO(_sbis) {
	uint8_t vio = _avr_get_ram(avr, io);
	uint8_t res = vio & (1 << b);

	STATE("sbis %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), vio, 1<<b, res?"":" not");

	if (res) {
		if (_avr_is_instruction_32_bits(avr, new_pc[0])) {
			new_pc[0] += 4; cycle[0] += 2;
		} else {
			new_pc[0] += 2; cycle[0]++;
		}
	}
}

UINSTp2k6(_sbiw) {
	uint16_t vp = _avr_get_r16le(avr, p);
	uint32_t res = vp - k;

	STATE("sbiw %s:%s[%04x], 0x%02x\n", avr_regname(p), avr_regname(p+1), vp, k);

	_avr_set_r16le(avr, p, res);

	avr->sreg[S_V] = ((vp & (~res)) & 0x8000) >> 15;
	avr->sreg[S_C] = ((res & (~vp)) & 0x8000) >> 15;

	_avr_flags_zns16(avr, res);

	SREG();

	cycle[0]++;
}

UINSTh4k8(_sbci) {
	uint8_t vh = _avr_get_r(avr, h);

	uint8_t res = vh - k - avr->sreg[S_C];

	STATE("sbci %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);

	_avr_set_r(avr, h, res);

	avr->sreg[S_C] = (k + avr->sreg[S_C]) > vh;

	_avr_flags_Zns(avr, res);

	SREG();
}

UINSTd5b3(_sbrc) {
	uint8_t vd = _avr_get_r(avr, d);
	int	branch = (0 == (vd & (1 << b)));

	STATE("%s %s[%02x], 0x%02x\t; Will%s branch\n", 0 ? "sbrs" : "sbrc", avr_regname(d), vd, 1 << b, branch ? "":" not");

	if (branch) {
		if (_avr_is_instruction_32_bits(avr, new_pc[0])) {
			new_pc[0] += 4; cycle[0] += 2;
		} else {
			new_pc[0] += 2; cycle[0]++;
		}
	}

}

UINSTd5b3(_sbrs) {
	uint8_t vd = _avr_get_r(avr, d);
	int	branch = (0 != (vd & (1 << b)));

	STATE("%s %s[%02x], 0x%02x\t; Will%s branch\n", 1 ? "sbrs" : "sbrc", avr_regname(d), vd, 1 << b, branch ? "":" not");

	if (branch) {
		if (_avr_is_instruction_32_bits(avr, new_pc[0])) {
			new_pc[0] += 4; cycle[0] += 2;
		} else {
			new_pc[0] += 2; cycle[0]++;
		}
	}

}

UINST(_sleep) {
	STATE("sleep\n");
	/* Don't sleep if there are interrupts about to be serviced.
	 * Without this check, it was possible to incorrectly enter a state
	 * in which the cpu was sleeping and interrupts were disabled. For more
	 * details, see the commit message. */
	if (!avr_has_pending_interrupts(avr) || !avr->sreg[S_I])
		avr->state = cpu_Sleeping;
}

UINSTd5rXYZop(_st) {
	uint8_t vd = _avr_get_r(avr, d);
	uint16_t vr = _avr_get_r16le(avr, r);

	STATE("st %s%s:%s[%04x]%s, %s[%02x] \n", op == 2 ? "--" : "", avr_regname(r), avr_regname(r+1), vr, \
		op == 1 ? "++" : "", avr_regname(d), vd);

	cycle[0]++; // 2 cycles, except tinyavr

	if (op == 2) vr--;
	_avr_set_ram(avr, vr, vd);
	if (op == 1) vr++;

	if(op)
		_avr_set_r16le(avr, r, vr);
}

UINSTd5rXYZq6(_std) {
	uint8_t vd = _avr_get_r(avr, d);
	uint16_t vr = _avr_get_r16le(avr, r) + q;

	STATE("st (%s+%d[%04x]), %s[%02x]\n", avr_regname(r), q, vr, avr_regname(d), d);

	cycle[0]++; // 2 cycles, except tinyavr

	_avr_set_ram(avr, vr, vd);
}

UINSTd5x16(_sts) {
	uint8_t	vd = _avr_get_r(avr, d);

	new_pc[0] += 2;

	STATE("sts 0x%04x, %s[%02x]\n", x, avr_regname(d), vd);

	cycle[0]++;

	_avr_set_ram(avr, x, vd);
}

UINSTd5r5(_sub) {
	uint8_t vd = _avr_get_r(avr, d);
	uint8_t vr = _avr_get_r(avr, r);

	uint8_t res = vd - vr;

	STATE("sub %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	_avr_set_r(avr, d, res);

	_avr_flags_sub_carry(avr, res, vd, vr);
	_avr_flags_sub_overflow(avr, res, vd, vr);

	_avr_flags_zns(avr, res);

	SREG();
}

UINSTh4k8(_subi) {
	uint8_t vh = _avr_get_r(avr, h);

	uint8_t res = vh - k;

	STATE("subi %s[%02x], 0x%02x = %02x\n", avr_regname(h), vh, k, res);

	_avr_set_r(avr, h, res);

	avr->sreg[S_C] = k > vh;

	_avr_flags_zns(avr, res);

	SREG();
}

UINSTh4k16(_subi) {
	uint16_t vh = _avr_get_r16le(avr, h);

	uint16_t res = vh - k;

	STATE("subi.w %s:%s[%04x], 0x%04x = %04x\n", avr_regname(h), avr_regname(h+1), vh, k, res);

	_avr_set_r16le(avr, h, res);

	avr->sreg[S_C] = k > vh;

	_avr_flags_zns16(avr, res);
	
	new_pc[0] += 2; cycle[0] ++;

	SREG();
}

UINSTd5(_swap) {
	uint8_t vd = _avr_get_r(avr, d);
	uint8_t res = ((vd & 0xf0) >> 4) | ((vd & 0x0f) << 4);

	STATE("swap %s[%02x] = %02x\n", avr_regname(d), vd, res);

	_avr_set_r(avr, d, res);
}

enum {
	k_avr_uinst_zop=0x00,
	k_avr_uinst_nop,
	k_avr_uinst_ret,
	k_avr_uinst_reti,
	k_avr_uinst_sleep,
	k_avr_uinst_d5_asr=0x10,
	k_avr_uinst_b3_bclr,
	k_avr_uinst_b3_bset,
	k_avr_uinst_d5_com,
	k_avr_uinst_d5_dec,
	k_avr_uinst_d5_inc,
	k_avr_uinst_d5_lpm_z0,
	k_avr_uinst_d5_lpm_z1,
	k_avr_uinst_d5_lsr,
	k_avr_uinst_d5_neg,
	k_avr_uinst_d5_pop,
	k_avr_uinst_d5_push,
	k_avr_uinst_d5_ror,
	k_avr_uinst_d5_swap,
	k_avr_uinst_x24_call=0x20,
	k_avr_uinst_x24_jmp,
	k_avr_uinst_h4k16_ldi,
	k_avr_uinst_d5x16_lds,
	k_avr_uinst_o12_rcall,
	k_avr_uinst_o12_rjmp,
	k_avr_uinst_d5x16_sts,
	k_avr_uinst_h4k16_subi,
	k_avr_uinst_d5r5_add=0x80,
	k_avr_uinst_d5Wr5W_add,
	k_avr_uinst_d5r5_adc,
	k_avr_uinst_p2k6_adiw,
	k_avr_uinst_d5r5_and,
	k_avr_uinst_h4k8_andi,
	k_avr_uinst_d5b3_bld,
	k_avr_uinst_o7b3_brxc,
	k_avr_uinst_o7b3_brxs,
	k_avr_uinst_d5b3_bst,
	k_avr_uinst_bIO_cbi,
	k_avr_uinst_d5r5_cp,
	k_avr_uinst_d5Wr5W_cp,
	k_avr_uinst_d5r5_cpc,
	k_avr_uinst_h4k8_cpi,
	k_avr_uinst_h4r5k8_cpi,
	k_avr_uinst_d5r5_cpse,
	k_avr_uinst_d5r5_eor,
	k_avr_uinst_d5a6_in,
	k_avr_uinst_d5a6b3_in_sbrs,
	k_avr_uinst_d5rXYZop_ld,
	k_avr_uinst_d5rXYZq6_ldd,
	k_avr_uinst_h4k8_ldi,
	k_avr_uinst_d5rXYZop_lpm_z0_st,
	k_avr_uinst_d5r5_mov,
	k_avr_uinst_d4r4_movw,
	k_avr_uinst_d5r5_mul,
	k_avr_uinst_d16r16_muls,
	k_avr_uinst_d5r5_or,
	k_avr_uinst_h4k8_ori,
	k_avr_uinst_d5a6_out,
	k_avr_uinst_bIO_sbi,
	k_avr_uinst_bIO_sbic,
	k_avr_uinst_bIO_sbis,
	k_avr_uinst_p2k6_sbiw,
	k_avr_uinst_d5r5_sbc,
	k_avr_uinst_h4k8_sbci,
	k_avr_uinst_d5b3_sbrc,
	k_avr_uinst_d5b3_sbrs,
	k_avr_uinst_d5rXYZop_st,
	k_avr_uinst_d5rXYZq6_std,
	k_avr_uinst_d5r5_sub,
	k_avr_uinst_h4k8_subi,
};

static inline void uFlashWrite(avr_t* avr, avr_flashaddr_t addr, uint32_t data) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(addr > avr->flashend) {
		printf("%s: access at 0x%04x past end of flash, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif

#ifndef CONFIG_SIMAVR_FAST_CORE_PIGGYBACKED
	avr->uflash[addr >> 1]=data;
#else
	uint32_t	*uflash=((uint32_t*)&((uint8_t *)avr->flash)[avr->flashend+1]);
	uflash[addr >> 1]=data;
#endif
}

extern inline uint32_t uFlashRead(avr_t* avr, avr_flashaddr_t addr) {
#ifdef FAST_CORE_AGGRESSIVE_CHECKS
	if(addr > avr->flashend) {
		printf("%s: access at 0x%04x past end of flash, aborting.", __FUNCTION__, addr);
		abort();
	}
#endif

#ifndef CONFIG_SIMAVR_FAST_CORE_PIGGYBACKED
	return(avr->uflash[(addr >> 1)]);
#else
	uint32_t	*uflash=((uint32_t*)&((uint8_t *)avr->flash)[avr->flashend+1]);
	return(uflash[addr >> 1]);
#endif
}

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

static inline uint8_t _OP_DECODE_b3(const uint16_t o) {
	return(o & 0x0007);
}

static inline uint8_t _OP_DECODE_d4(const uint16_t o) {
	return((o & 0x00f0) >> 4);
}

static inline uint8_t _OP_DECODE_d5(const uint16_t o) {
	return((o & 0x01f0) >> 4);
}

static inline uint8_t _OP_DECODE_r4(const uint16_t o) {
	return(o & 0x000f);
}

static inline int8_t _OP_DECODE_o7(const uint16_t o) {
	return((int16_t)((o & 0x03f8) << 6) >> 8);
}

static inline uint8_t _OP_DECODE_r5(const uint16_t o) {
	return(((o & 0x0200) >> 5) | _OP_DECODE_r4(o));
}

static inline uint8_t _OP_DECODE_k6(const uint16_t o) {
	return(((o & 0x00c0) >> 2) | _OP_DECODE_r4(o));
}

static inline uint8_t _OP_DECODE_k8(const uint16_t o) {
	return(((o & 0x0f00) >> 4) | _OP_DECODE_r4(o));
}

static inline uint8_t _OP_DECODE_d16(const uint16_t o) {
	return(16+_OP_DECODE_d4(o));
}

static inline uint8_t _OP_DECODE_r16(const uint16_t o) {
	return(16+_OP_DECODE_r4(o));
}

static inline uint8_t _OP_DECODE_h4(const uint16_t o) {
	return(_OP_DECODE_d16(o));
}

static inline uint8_t _OP_DECODE_p2(const uint16_t o) {
	return(24+((o & 0x0030) >> 3));
}

static inline int16_t _OP_DECODE_o12(const uint16_t o) {
	return((int16_t)((o & 0x0fff) << 4) >> 3);
}		

static inline uint8_t _OP_DECODE_a6(const uint16_t o) {
	return(32+(((o & 0x0600) >> 5)|_OP_DECODE_r4(o)));
}

#define OPCODE(opcode, r1, r2, r3) ((r3<<24)|(r2<<16)|(r1<<8)|(opcode))

#define __INST(name) \
	static inline void _avr_inst##name(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle, uint16_t opcode)

#define __INSTarg(name, args...) \
	static inline void _avr_inst##name(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle, uint16_t opcode, ##args)

#ifdef FAST_CORE_ITRACE
#define ITRACE \
	xSTATE("i0x%04x u0x%08x 0xf000(0x%04x) 0xfe0e(0x%04x) %s\n", opcode, uFlashRead(avr, avr->pc), opcode & 0xf000, opcode & 0xfe0e, __FUNCTION__); 
#else
#define ITRACE
#endif

#define INST(name) __INST(name) { \
		_avr_uinst##name(avr, new_pc, cycle); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst##name, 0, 0, 0)); \
		ITRACE; \
	}

#define INSTb3(name) __INST(_b3##name) { \
		const uint8_t b3 = _OP_DECODE_d4(opcode) & 0x7; \
	\
		_avr_uinst_b3##name(avr, new_pc, cycle, b3); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_b3##name, b3, 0, 0)); \
		ITRACE; \
	}

#define INSTbIO(name) __INST(_bIO##name) { \
		const uint8_t a = 32 + ((opcode & 0x00f8) >> 3); \
		const uint8_t b = (opcode & 0x0007); \
	\
		_avr_uinst_bIO##name(avr, new_pc, cycle, a, b); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_bIO##name, a, b, 0)); \
		ITRACE; \
	}


#define INSTd4r4(name) __INST(_d4r4##name) {\
		const uint8_t d4 = _OP_DECODE_d4(opcode) << 1; \
		const uint8_t r4 = _OP_DECODE_r4(opcode) << 1; \
	\
		_avr_uinst_d4r4##name(avr, new_pc, cycle, d4, r4); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d4r4##name, d4, r4, 0)); \
		ITRACE; \
	}

#define INSTd5(name) __INST(_d5##name) { \
		const uint8_t d5 = _OP_DECODE_d5(opcode); \
	\
		_avr_uinst_d5##name(avr, new_pc, cycle, d5); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5##name, d5, 0, 0)); \
		ITRACE; \
	}

#define INSTd5rXYZop(name) __INST(_d5rXYZop##name) { \
		const uint8_t d5 = _OP_DECODE_d5(opcode); \
		const uint16_t	next_opcode = _avr_flash_read16le(avr, *new_pc); \
	\
		_avr_uinst_d5##name(avr, new_pc, cycle, d5); \
	\
		const uint8_t d5b = _OP_DECODE_d5(next_opcode); \
	\
		if( ((0x9004 /* LPM */ == (0xfe0e & opcode)) && ( 0x920c /* ST */ == ( 0xfe0e & next_opcode))) \
				&& (d5 == d5b)) { \
			const int regs[4] = {R_ZL, 0x00, R_YL, R_XL}; \
			const uint8_t r = regs[(next_opcode & 0x000c)>>2]; \
			const uint8_t opr = next_opcode & 0x0003; \
			uFlashWrite(avr, avr->pc, OPCODE(CONCAT(k_avr_uinst_d5rXYZop##name, _st), d5, r, opr)); \
		} else { \
			uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5##name, d5, 0, 0)); \
		} ITRACE; \
	}

#define INSTd5a6(name) __INST(_d5a6##name) { \
		const uint8_t d5 = _OP_DECODE_d5(opcode); \
		const uint8_t a6 = _OP_DECODE_a6(opcode); \
	\
		_avr_uinst_d5a6##name(avr, new_pc, cycle, d5, a6); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5a6##name, d5, a6, 0)); \
		ITRACE; \
	}

#define INSTd5a6b3(name) __INST(_d5a6b3##name) { \
		const uint8_t d5 = _OP_DECODE_d5(opcode); \
		const uint8_t a6 = _OP_DECODE_a6(opcode); \
		const uint16_t	next_opcode = _avr_flash_read16le(avr, *new_pc); \
	\
		_avr_uinst_d5a6##name(avr, new_pc, cycle, d5, a6); \
	\
		const uint8_t d5b = _OP_DECODE_d5(next_opcode); \
	\
		if((0xb000 /* IN */ == (opcode & 0xf800)) && (0xfe00 /* SBRS */ == (next_opcode & 0xfe00)) \
				&& (d5 == d5b)) {\
			const uint8_t b3 = _OP_DECODE_b3(next_opcode); \
			uFlashWrite(avr, avr->pc, OPCODE(CONCAT(k_avr_uinst_d5a6b3##name,_sbrs), d5, a6, b3)); \
		} else  \
			uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5a6##name, d5, a6, 0)); \
		ITRACE; \
	}

#define INSTd5b3(name) __INST(_d5b3##name) {\
		const uint8_t d5 = _OP_DECODE_d5(opcode); \
		const uint8_t b3 = _OP_DECODE_b3(opcode); \
	\
		_avr_uinst_d5b3##name(avr, new_pc, cycle, d5, b3); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5b3##name, d5, b3, 0)); \
		ITRACE; \
	}

#define INSTd5r5(name) __INST(_d5r5##name) { \
		const uint8_t d5 = _OP_DECODE_d5(opcode); \
		const uint8_t r5 = _OP_DECODE_r5(opcode); \
	\
		_avr_uinst_d5r5##name(avr, new_pc, cycle, d5, r5); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5r5##name, d5, r5, 0)); \
		ITRACE; \
	}

#define INSTd5Wr5W(name) __INST(_d5Wr5W##name) { \
		const uint8_t d5 = _OP_DECODE_d5(opcode); \
		const uint8_t r5 = _OP_DECODE_r5(opcode); \
		const uint16_t	next_opcode = _avr_flash_read16le(avr, *new_pc); \
	\
		_avr_uinst_d5r5##name(avr, new_pc, cycle, d5, r5); \
	\
		const uint8_t d5b = _OP_DECODE_d5(next_opcode); \
		const uint8_t r5b = _OP_DECODE_r5(next_opcode); \
		if((((0x0c00 /* ADD */ == (opcode & 0xfc00)) && (0x1c00 /* ADDC */ == (next_opcode & 0xfc00))) \
				|| ((0x1400 /* CP */ == (opcode & 0xfc00)) && (0x0400 /* CPC */ == (next_opcode & 0xfc00)))) \
				&& (((1 + d5) == d5b) && ((1 + r5) == r5b))) { \
			uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5Wr5W##name, d5, r5, 0)); \
		} else { \
			uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5r5##name, d5, r5, 0)); \
		} \
		ITRACE; \
	}

#define INSTd5rXYZq6(name) __INSTarg(_d5rXYZq6##name, uint8_t r) { \
		const uint8_t d5 = _OP_DECODE_d5(opcode); \
		const uint8_t q = ((opcode & 0x2000) >> 8) | ((opcode & 0x0c00) >> 7) | (opcode & 0x7); \
	\
		_avr_uinst_d5rXYZq6##name(avr, new_pc, cycle, d5, r, q); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5rXYZq6##name, d5, r, q)); \
		ITRACE; \
	}

#define INSTd5rXYZ(name) __INSTarg(_d5rXYZ##name, uint8_t r) { \
		const uint8_t d5 = _OP_DECODE_d5(opcode); \
		const uint8_t opr = opcode & 0x003; \
	\
		_avr_uinst_d5rXYZop##name(avr, new_pc, cycle, d5, r, opr); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5rXYZop##name, d5, r, opr)); \
		ITRACE; \
	}

#define INSTd16r16(name) __INST(_d16r16##name) {\
		const uint8_t d16 = _OP_DECODE_d16(opcode); \
		const uint8_t r16 = _OP_DECODE_r16(opcode); \
	\
		_avr_uinst_d16r16##name(avr, new_pc, cycle, d16, r16); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d16r16##name, d16, r16, 0)); \
		ITRACE; \
	}

#define INSTd5x16(name) __INST(_d5x16##name) { \
		const uint8_t d5 = _OP_DECODE_d5(opcode); \
		const uint16_t x16 = _avr_flash_read16le(avr, *new_pc); \
	\
		_avr_uinst_d5x16##name(avr, new_pc, cycle, d5, x16); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5x16##name, d5, x16, 0)); \
		ITRACE; \
	}

#define INSTh4k8(name) __INST(_h4k8##name) { \
		const uint8_t	h4=_OP_DECODE_h4(opcode); \
		const uint8_t	k8=_OP_DECODE_k8(opcode); \
	\
		_avr_uinst_h4k8##name(avr, new_pc, cycle, h4, k8); \
		uFlashWrite(avr, avr->pc,OPCODE(k_avr_uinst_h4k8##name, h4, k8, 0)); \
		ITRACE; \
	}

#define INSTh4k16(name) __INST(_h4k16##name) { \
		const uint8_t	h4 = _OP_DECODE_h4(opcode); \
		const uint8_t	k8 = _OP_DECODE_k8(opcode); \
		const uint16_t	next_opcode = _avr_flash_read16le(avr, *new_pc); \
	\
		_avr_uinst_h4k8##name(avr, new_pc, cycle, h4, k8); \
	\
		const uint8_t	h4b = _OP_DECODE_h4(next_opcode); \
	\
		if((((0x5000 /* SUBI.l */ == (opcode & 0xf000 )) && (0x4000 /* SBCI.h */ == (next_opcode & 0xf000))) \
			 	|| ((0xe000 /* LDI.l */ == (opcode & 0xf000 )) && (0xe000 /* LDI.h */== (next_opcode & 0xf000)))) \
				&& ((1 + h4) == h4b)) { \
			const uint8_t	k8b = _OP_DECODE_k8(next_opcode); \
			uFlashWrite(avr, avr->pc,OPCODE(k_avr_uinst_h4k16##name, h4, k8, k8b)); \
		} else { \
			uFlashWrite(avr, avr->pc,OPCODE(k_avr_uinst_h4k8##name, h4, k8, 0)); \
		} \
			ITRACE; \
	}

#define INSTh4r5k8(name) __INST(_h4r5k8##name) { \
		const uint8_t	h4 = _OP_DECODE_h4(opcode); \
		const uint8_t	k8 = _OP_DECODE_k8(opcode); \
		const uint16_t	next_opcode = _avr_flash_read16le(avr, *new_pc); \
	\
		_avr_uinst_h4k8##name(avr, new_pc, cycle, h4, k8); \
	\
		const uint8_t	d5 = _OP_DECODE_d5(next_opcode); \
	\
		if(((0x3000 /* CPI.l */ == (opcode & 0xf000)) && (0x0400 /* CPC.h */ == (next_opcode & 0xfc00))) \
				&& ((1 + h4) == d5)) { \
			const uint8_t	r5 = _OP_DECODE_r5(next_opcode); \
			uFlashWrite(avr, avr->pc,OPCODE(k_avr_uinst_h4r5k8##name, h4, r5, k8)); \
		} else { \
			uFlashWrite(avr, avr->pc,OPCODE(k_avr_uinst_h4k8##name, h4, k8, 0)); \
		} \
			ITRACE; \
	}

#define INSTo7b3(name) __INST(_o7b3##name) {\
		const uint8_t o7 = _OP_DECODE_o7(opcode); \
		const uint8_t b3 = _OP_DECODE_b3(opcode); \
	\
		_avr_uinst_o7b3##name(avr, new_pc, cycle, o7, b3); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_o7b3##name, o7, b3, 0)); \
		ITRACE; \
	}

#define INSTo12(name) __INST(_o12##name) {\
		const int16_t o12 = _OP_DECODE_o12(opcode); \
	\
		_avr_uinst_o12##name(avr, new_pc, cycle, o12); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_o12##name, 0, o12, 0)); \
		ITRACE; \
	}

#define INSTp2k6(name) __INST(_p2k6##name) { \
		const uint8_t p2 = _OP_DECODE_p2(opcode); \
		const uint8_t k6 = _OP_DECODE_k6(opcode); \
	\
		_avr_uinst_p2k6##name(avr, new_pc, cycle, p2, k6); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_p2k6##name, p2, k6, 0)); \
		ITRACE; \
	}

#define INSTx24(name) __INST(_x24##name) { \
		const uint8_t x6 = ((_OP_DECODE_d5(opcode) << 1) | (opcode & 0x0001)); \
		const uint16_t x16 = _avr_flash_read16le(avr, *new_pc); \
	\
		_avr_uinst_x24##name(avr, new_pc, cycle, x6, x16); \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_x24##name, x6, x16, 0)); \
		ITRACE; \
	}

INSTd5Wr5W(_add)

INSTp2k6(_adiw)

INSTd5r5(_adc)
INSTd5r5(_and)

INSTh4k8(_andi)

INSTd5(_asr)

INSTb3(_bclr)

INSTd5b3(_bld)

INSTo7b3(_brxc)
INSTo7b3(_brxs)

INSTb3(_bset)

INSTd5b3(_bst)

INSTx24(_call)

INSTbIO(_cbi)

INSTd5(_com)

INSTd5Wr5W(_cp)
INSTd5r5(_cpc)

INSTh4r5k8(_cpi)

INSTd5r5(_cpse)

INSTd5(_dec)

INSTd5r5(_eor)

INSTd5a6b3(_in)

INSTd5(_inc)

INSTx24(_jmp)

INSTd5rXYZ(_ld)
INSTd5rXYZq6(_ldd)

INSTh4k16(_ldi)

INSTd5x16(_lds)

INSTd5(_lpm_z0)
INSTd5rXYZop(_lpm_z0)
INSTd5(_lpm_z1)

INSTd5(_lsr)

INSTd5r5(_mov)

INSTd4r4(_movw)

INSTd5r5(_mul)

INSTd16r16(_muls)

INSTd5(_neg)

INST(_nop)

INSTd5r5(_or)

INSTh4k8(_ori)

INSTd5a6(_out)

INSTd5(_pop)

INSTd5(_push)

INST(_ret)
INST(_reti)

INSTd5(_ror)

INSTo12(_rcall)
INSTo12(_rjmp)

INSTd5r5(_sbc)

INSTd5b3(_sbrc)
INSTd5b3(_sbrs)

INSTbIO(_sbi)
INSTbIO(_sbic)
INSTbIO(_sbis)

INSTp2k6(_sbiw)

INSTh4k8(_sbci)

INST(_sleep)

INSTd5rXYZ(_st)
INSTd5rXYZq6(_std)

INSTd5x16(_sts)

INSTd5r5(_sub)

INSTh4k16(_subi)

INSTd5(_swap)

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
extern inline avr_flashaddr_t avr_decode_one(avr_t* avr)
{
	avr_flashaddr_t		new_pc = avr->pc + 2;	// future "default" pc
	int 			cycle = 1;

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

	uint16_t		opcode = _avr_flash_read16le(avr, avr->pc);

#ifdef FAST_CORE_DECODE_TRAP
	uint32_t uop = OPCODEop(uFlashRead(avr, avr->pc));
	if(uop) {
		xSTATE("opcode trap, not handled: 0x%08x [0x%04x]", uop, opcode);
	}
#endif

	switch (opcode & 0xf000) {
		case 0x0000: {
			switch (opcode) {
				case 0x0000: {	// NOP
					_avr_inst_nop(avr, &new_pc, &cycle, opcode);
				}	break;
				default: {
					switch (opcode & 0xfc00) {
						case 0x0400: {	// CPC compare with carry 0000 01rd dddd rrrr
							_avr_inst_d5r5_cpc(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x0c00: {	// ADD without carry 0000 11 rd dddd rrrr
							_avr_inst_d5Wr5W_add(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x0800: {	// SBC subtract with carry 0000 10rd dddd rrrr
							_avr_inst_d5r5_sbc(avr, &new_pc, &cycle, opcode);
						}	break;
						default:
							switch (opcode & 0xff00) {
								case 0x0100: {	// MOVW – Copy Register Word 0000 0001 dddd rrrr
									_avr_inst_d4r4_movw(avr, &new_pc, &cycle, opcode);
								}	break;
								case 0x0200: {	// MULS – Multiply Signed 0000 0010 dddd rrrr
									_avr_inst_d16r16_muls(avr, &new_pc, &cycle, opcode);
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
									cycle++;
									STATE("%s %s[%d], %s[%02x] = %d\n", name, avr_regname(d), ((int8_t)avr->data[d]), avr_regname(r), ((int8_t)avr->data[r]), res);
//									_avr_set_r(avr, 0, res);
//									_avr_set_r(avr, 1, res >> 8);
									_avr_data_write16le(avr, 0, res);
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
					_avr_inst_d5r5_sub(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0x1000: {	// CPSE Compare, skip if equal 0000 00 rd dddd rrrr
					_avr_inst_d5r5_cpse(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0x1400: {	// CP Compare 0000 01 rd dddd rrrr
					_avr_inst_d5Wr5W_cp(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0x1c00: {	// ADD with carry 0001 11 rd dddd rrrr
					_avr_inst_d5r5_adc(avr, &new_pc, &cycle, opcode);
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0x2000: {
			switch (opcode & 0xfc00) {
				case 0x2000: {	// AND	0010 00rd dddd rrrr
					_avr_inst_d5r5_and(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0x2400: {	// EOR	0010 01rd dddd rrrr
					_avr_inst_d5r5_eor(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0x2800: {	// OR Logical OR	0010 10rd dddd rrrr
					_avr_inst_d5r5_or(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0x2c00: {	// MOV	0010 11rd dddd rrrr
					_avr_inst_d5r5_mov(avr, &new_pc, &cycle, opcode);
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0x3000: {	// CPI 0011 KKKK dddd KKKK
			_avr_inst_h4r5k8_cpi(avr, &new_pc, &cycle, opcode);
		}	break;

		case 0x4000: {	// SBCI Subtract Immediate With Carry 0101 10 kkkk dddd kkkk
			_avr_inst_h4k8_sbci(avr, &new_pc, &cycle, opcode);
		}	break;

		case 0x5000: {	// SUB Subtract Immediate 0101 10 kkkk dddd kkkk
			_avr_inst_h4k16_subi(avr, &new_pc, &cycle, opcode);
		}	break;

		case 0x6000: {	// ORI aka SBR	Logical AND with Immediate	0110 kkkk dddd kkkk
			_avr_inst_h4k8_ori(avr, &new_pc, &cycle, opcode);
		}	break;

		case 0x7000: {	// ANDI	Logical AND with Immediate	0111 kkkk dddd kkkk
			_avr_inst_h4k8_andi(avr, &new_pc, &cycle, opcode);
		}	break;

		case 0xa000:
		case 0x8000: {
			switch (opcode & 0xd008) {
				case 0xa000:
				case 0x8000: {	// LD (LDD) – Load Indirect using Z 10q0 qq0r rrrr 0qqq
					if(opcode & 0x0200)
						_avr_inst_d5rXYZq6_std(avr, &new_pc, &cycle, opcode, R_ZL);
					else
						_avr_inst_d5rXYZq6_ldd(avr, &new_pc, &cycle, opcode, R_ZL);
				}	break;
				case 0xa008:
				case 0x8008: {	// LD (LDD) – Load Indirect using Y 10q0 qq0r rrrr 1qqq
					if(opcode & 0x0200)
						_avr_inst_d5rXYZq6_std(avr, &new_pc, &cycle, opcode, R_YL);
					else
						_avr_inst_d5rXYZq6_ldd(avr, &new_pc, &cycle, opcode, R_YL);
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
					_avr_inst_sleep(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0x9598: { // BREAK
					STATE("break\n");
					if (avr->gdb) {
						// if gdb is on, we break here as in here
						// and we do so until gdb restores the instruction
						// that was here before
						avr->state = cpu_StepDone;
						new_pc = avr->pc;
						cycle = 0;
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
				case 0x9409:   // IJMP Indirect jump 					1001 0100 0000 1001
				case 0x9419:   // EIJMP Indirect jump 					1001 0100 0001 1001   bit 4 is "indirect"
				case 0x9509:   // ICALL Indirect Call to Subroutine		1001 0101 0000 1001
				case 0x9519: { // EICALL Indirect Call to Subroutine	1001 0101 0001 1001   bit 8 is "push pc"
					int e = opcode & 0x10;
					int p = opcode & 0x100;
					if (e && !avr->eind)
						_avr_invalid_opcode(avr);
//					uint16_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8);
					uint16_t z = _avr_data_read16le(avr, R_ZL);
					if (e)
						z |= avr->data[avr->eind] << 16;
					STATE("%si%s Z[%04x]\n", e?"e":"", p?"call":"jmp", z << 1);
					if (p) {
						cycle++;
						_avr_push16(avr, new_pc >> 1);
					}
					new_pc = z << 1;
					cycle++;
					TRACE_JUMP();
				}	break;
				case 0x9518: {	// RETI
					_avr_inst_reti(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0x9508: {	// RET
					_avr_inst_ret(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0x95c8: {	// LPM Load Program Memory R0 <- (Z) 1001 0101 1100 1000
#if 1
					_avr_inst_d5_lpm_z0(avr, &new_pc, &cycle, opcode&0x9004);
#else
//					uint16_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8);
					uint16_t z = _avr_data_read16le(avr, R_ZL);
					STATE("lpm %s, (Z[%04x])\n", avr_regname(0), z);
					cycle += 2; // 3 cycles
//					_avr_set_r(avr, 0, avr->flash[z]);
					_avr_data_write(avr, 0, avr->flash[z]);
#endif
				}	break;
				case 0x9408:case 0x9418:case 0x9428:case 0x9438:case 0x9448:case 0x9458:case 0x9468:
				case 0x9478: // BSET 1001 0100 0ddd 1000
				{	_avr_inst_b3_bset(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0x9488:case 0x9498:case 0x94a8:case 0x94b8:case 0x94c8:case 0x94d8:case 0x94e8:
				case 0x94f8:	// bit 7 is 'clear vs set'
				{	// BCLR 1001 0100 1ddd 1000
					_avr_inst_b3_bclr(avr, &new_pc, &cycle, opcode);
				}	break;
				default:  {
					switch (opcode & 0xfe0f) {
						case 0x9000: {	// LDS Load Direct from Data Space, 32 bits
							_avr_inst_d5x16_lds(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x9005: {	// LPM Load Program Memory 1001 000d dddd 01oo
							_avr_inst_d5_lpm_z1(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x9004: {	// LPM Load Program Memory 1001 000d dddd 01oo
							_avr_inst_d5rXYZop_lpm_z0(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x9006:
						case 0x9007: {	// ELPM Extended Load Program Memory 1001 000d dddd 01oo
							if (!avr->rampz)
								_avr_invalid_opcode(avr);
//							uint32_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8) | (avr->data[avr->rampz] << 16);
							uint32_t z = _avr_data_read16le(avr, R_ZL) | (avr->data[avr->rampz] << 16);

							uint8_t r = (opcode >> 4) & 0x1f;
							int op = opcode & 3;
							STATE("elpm %s, (Z[%02x:%04x]%s)\n", avr_regname(r), z >> 16, z&0xffff, opcode?"+":"");
//							_avr_set_r(avr, r, avr->flash[z]);
							_avr_data_write(avr, r, avr->flash[z]);
							if (op == 3) {
								z++;
//								_avr_set_r(avr, avr->rampz, z >> 16);
								_avr_set_ram(avr, avr->rampz, z >> 16);
//								_avr_set_r(avr, R_ZH, z >> 8);
//								_avr_set_r(avr, R_ZL, z);
								_avr_data_write16le(avr, R_ZL, (z&0xffff));
							}
							cycle += 2; // 3 cycles
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
							_avr_inst_d5rXYZ_ld(avr, &new_pc, &cycle, opcode, R_XL);
						}	break;
						case 0x920c:
						case 0x920d:
						case 0x920e: {	// ST Store Indirect Data Space X 1001 001r rrrr 11oo
							_avr_inst_d5rXYZ_st(avr, &new_pc, &cycle, opcode, R_XL);
						}	break;
						case 0x9009:
						case 0x900a: {	// LD Load Indirect from Data using Y 1001 000r rrrr 10oo
							_avr_inst_d5rXYZ_ld(avr, &new_pc, &cycle, opcode, R_YL);
						}	break;
						case 0x9209:
						case 0x920a: {	// ST Store Indirect Data Space Y 1001 001r rrrr 10oo
							_avr_inst_d5rXYZ_st(avr, &new_pc, &cycle, opcode, R_YL);
						}	break;
						case 0x9200: {	// STS ! Store Direct to Data Space, 32 bits
							_avr_inst_d5x16_sts(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x9001:
						case 0x9002: {	// LD Load Indirect from Data using Z 1001 001r rrrr 00oo
							_avr_inst_d5rXYZ_ld(avr, &new_pc, &cycle, opcode, R_ZL);
						}	break;
						case 0x9201:
						case 0x9202: {	// ST Store Indirect Data Space Z 1001 001r rrrr 00oo
							_avr_inst_d5rXYZ_st(avr, &new_pc, &cycle, opcode, R_ZL);
						}	break;
						case 0x900f: {	// POP 1001 000d dddd 1111
							_avr_inst_d5_pop(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x920f: {	// PUSH 1001 001d dddd 1111
							_avr_inst_d5_push(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x9400: {	// COM – One’s Complement
							_avr_inst_d5_com(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x9401: {	// NEG – Two’s Complement
							_avr_inst_d5_neg(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x9402: {	// SWAP – Swap Nibbles
							_avr_inst_d5_swap(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x9403: {	// INC – Increment
							_avr_inst_d5_inc(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x9405: {	// ASR – Arithmetic Shift Right 1001 010d dddd 0101
							_avr_inst_d5_asr(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x9406: {	// LSR 1001 010d dddd 0110
							_avr_inst_d5_lsr(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x9407: {	// ROR 1001 010d dddd 0111
							_avr_inst_d5_ror(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x940a: {	// DEC – Decrement
							_avr_inst_d5_dec(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x940c:
						case 0x940d: {	// JMP Long Call to sub, 32 bits
							_avr_inst_x24_jmp(avr, &new_pc, &cycle, opcode);
						}	break;
						case 0x940e:
						case 0x940f: {	// CALL Long Call to sub, 32 bits
							_avr_inst_x24_call(avr, &new_pc, &cycle, opcode);
						}	break;

						default: {
							switch (opcode & 0xff00) {
								case 0x9600: {	// ADIW - Add Immediate to Word 1001 0110 KKdd KKKK
									_avr_inst_p2k6_adiw(avr, &new_pc, &cycle, opcode);
//									cycle++;
								}	break;
								case 0x9700: {	// SBIW - Subtract Immediate from Word 1001 0110 KKdd KKKK
									_avr_inst_p2k6_sbiw(avr, &new_pc, &cycle, opcode);
//									cycle++;
								}	break;
								case 0x9800: {	// CBI - Clear Bit in I/O Register 1001 1000 AAAA Abbb
									_avr_inst_bIO_cbi(avr, &new_pc, &cycle, opcode);
								}	break;
								case 0x9900: {	// SBIC - Skip if Bit in I/O Register is Cleared 1001 0111 AAAA Abbb
									_avr_inst_bIO_sbic(avr, &new_pc, &cycle, opcode);
								}	break;
								case 0x9a00: {	// SBI - Set Bit in I/O Register 1001 1000 AAAA Abbb
									_avr_inst_bIO_sbi(avr, &new_pc, &cycle, opcode);
								}	break;
								case 0x9b00: {	// SBIS - Skip if Bit in I/O Register is Set 1001 1011 AAAA Abbb
									_avr_inst_bIO_sbis(avr, &new_pc, &cycle, opcode);
								}	break;
								default:
									switch (opcode & 0xfc00) {
										case 0x9c00: {	// MUL - Multiply Unsigned 1001 11rd dddd rrrr
											_avr_inst_d5r5_mul(avr, &new_pc, &cycle, opcode);
//											cycle++;
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
					_avr_inst_d5a6_out(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0xb000: {	// IN Rd,A 1011 0AAr rrrr AAAA
					_avr_inst_d5a6b3_in(avr, &new_pc, &cycle, opcode);
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;
		case 0xc000: {	// RJMP 1100 kkkk kkkk kkkk
			_avr_inst_o12_rjmp(avr, &new_pc, &cycle, opcode);
		}	break;
		case 0xd000: {
			// RCALL 1100 kkkk kkkk kkkk
			_avr_inst_o12_rcall(avr, &new_pc, &cycle, opcode);
		}	break;
		case 0xe000: {	// LDI Rd, K 1110 KKKK RRRR KKKK -- aka SER (LDI r, 0xff)
			_avr_inst_h4k16_ldi(avr, &new_pc, &cycle, opcode);
		}	break;
		case 0xf000: {
			switch (opcode & 0xfe00) {
				case 0xf000:
				case 0xf200: {
					_avr_inst_o7b3_brxs(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0xf400:
				case 0xf600: {
					_avr_inst_o7b3_brxc(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0xf800:
				case 0xf900: {	// BLD – Bit Store from T into a Bit in Register 1111 100r rrrr 0bbb
					_avr_inst_d5b3_bld(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0xfa00:
				case 0xfb00:{	// BST – Bit Store into T from bit in Register 1111 100r rrrr 0bbb
					_avr_inst_d5b3_bst(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0xfc00: {
					_avr_inst_d5b3_sbrc(avr, &new_pc, &cycle, opcode);
				}	break;
				case 0xfe00: {	// SBRS/SBRC – Skip if Bit in Register is Set/Clear 1111 11sr rrrr 0bbb
					_avr_inst_d5b3_sbrs(avr, &new_pc, &cycle, opcode);
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;
		default: _avr_invalid_opcode(avr);
	}

	avr->cycle += cycle;

	return new_pc;
}

extern inline avr_flashaddr_t avr_fast_core_run_one(avr_t* avr) {
	avr_flashaddr_t		new_pc = avr->pc + 2;
	int			cycle = 1;

	uint32_t		opcode = uFlashRead(avr, avr->pc);
	uint8_t			soc = OPCODEop(opcode);
	uint8_t			r1 = OPCODEr1(opcode);

	if(0x80<=soc) {
		uint8_t r2 = OPCODEr2(opcode);
		switch(soc) {
			case	k_avr_uinst_d5r5_add:
				_avr_uinst_d5r5_add(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5Wr5W_add:
				_avr_uinst_d5Wr5W_add(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5r5_adc:
				_avr_uinst_d5r5_adc(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_p2k6_adiw:
				_avr_uinst_p2k6_adiw(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5r5_and:
				_avr_uinst_d5r5_and(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_h4k8_andi:
				_avr_uinst_h4k8_andi(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5b3_bld:
				_avr_uinst_d5b3_bld(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_o7b3_brxc:
				_avr_uinst_o7b3_brxc(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_o7b3_brxs:
				_avr_uinst_o7b3_brxs(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5b3_bst:
				_avr_uinst_d5b3_bst(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_bIO_cbi:
				_avr_uinst_bIO_cbi(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5r5_cp:
				_avr_uinst_d5r5_cp(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5Wr5W_cp:
				_avr_uinst_d5Wr5W_cp(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5r5_cpc:
				_avr_uinst_d5r5_cpc(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_h4k8_cpi:
				_avr_uinst_h4k8_cpi(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5r5_cpse:
				_avr_uinst_d5r5_cpse(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5r5_eor:
				_avr_uinst_d5r5_eor(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_h4k8_ldi:
				_avr_uinst_h4k8_ldi(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5r5_mov:
				_avr_uinst_d5r5_mov(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d4r4_movw:
				_avr_uinst_d4r4_movw(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5r5_mul:
				_avr_uinst_d5r5_mul(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d16r16_muls:
				_avr_uinst_d16r16_muls(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5a6_in:
				_avr_uinst_d5a6_in(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5r5_or:
				_avr_uinst_d5r5_or(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_h4k8_ori:
				_avr_uinst_h4k8_ori(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5a6_out:
				_avr_uinst_d5a6_out(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5r5_sbc:
				_avr_uinst_d5r5_sbc(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_h4k8_sbci:
				_avr_uinst_h4k8_sbci(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_bIO_sbi:
				_avr_uinst_bIO_sbi(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_bIO_sbic:
				_avr_uinst_bIO_sbic(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_bIO_sbis:
				_avr_uinst_bIO_sbis(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_p2k6_sbiw:
				_avr_uinst_p2k6_sbiw(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5b3_sbrc:
				_avr_uinst_d5b3_sbrc(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5b3_sbrs:
				_avr_uinst_d5b3_sbrs(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_d5r5_sub:
				_avr_uinst_d5r5_sub(avr, &new_pc, &cycle, r1, r2);
				break;
			case	k_avr_uinst_h4k8_subi:
				_avr_uinst_h4k8_subi(avr, &new_pc, &cycle, r1, r2);
				break;
			default: {
				uint8_t	r3 = OPCODEr3(opcode);
				switch(soc) {
					case	k_avr_uinst_h4r5k8_cpi:
						_avr_uinst_h4r5k8_cpi(avr, &new_pc, &cycle, r1, r2, r3);
						break;
					case	k_avr_uinst_d5a6b3_in_sbrs:
						_avr_uinst_d5a6b3_in_sbrs(avr, &new_pc, &cycle, r1, r2, r3);
						break;
					case	k_avr_uinst_d5rXYZop_ld:
						_avr_uinst_d5rXYZop_ld(avr, &new_pc, &cycle, r1, r2, r3);
						break;
					case	k_avr_uinst_d5rXYZq6_ldd:
						_avr_uinst_d5rXYZq6_ldd(avr, &new_pc, &cycle, r1, r2, r3);
						break;
					case	k_avr_uinst_d5rXYZop_lpm_z0_st:
						_avr_uinst_d5rXYZop_lpm_z0_st(avr, &new_pc, &cycle, r1, r2, r3);
						break;
					case	k_avr_uinst_d5rXYZop_st:
						_avr_uinst_d5rXYZop_st(avr, &new_pc, &cycle, r1, r2, r3);
						break;
					case	k_avr_uinst_d5rXYZq6_std:
						_avr_uinst_d5rXYZq6_std(avr, &new_pc, &cycle, r1, r2, r3);
						break;
					default:
						goto notFound;
						break;
				}
			}
		}
	} else if(0x20<=soc) {
		uint16_t	x16=OPCODEx16(opcode);
		switch(soc) {

			case	k_avr_uinst_x24_call:
				_avr_uinst_x24_call(avr, &new_pc, &cycle, r1, x16);
				break;
			case	k_avr_uinst_x24_jmp:
				_avr_uinst_x24_jmp(avr, &new_pc, &cycle, r1, x16);
				break;
			case	k_avr_uinst_h4k16_ldi:
				_avr_uinst_h4k16_ldi(avr, &new_pc, &cycle, r1, x16);
				break;
			case	k_avr_uinst_d5x16_lds:
				_avr_uinst_d5x16_lds(avr, &new_pc, &cycle, r1, x16);
				break;
			case	k_avr_uinst_o12_rcall:
				_avr_uinst_o12_rcall(avr, &new_pc, &cycle, x16);
				break;
			case	k_avr_uinst_o12_rjmp:
				_avr_uinst_o12_rjmp(avr, &new_pc, &cycle, x16);
				break;
			case	k_avr_uinst_d5x16_sts:
				_avr_uinst_d5x16_sts(avr, &new_pc, &cycle, r1, x16);
				break;
			case	k_avr_uinst_h4k16_subi:
				_avr_uinst_h4k16_subi(avr, &new_pc, &cycle, r1, x16);
				break;
			default:
				goto notFound;
				break;
		}
	} else if(0x10<=soc) {
		switch(soc) {
			case	k_avr_uinst_d5_asr:
				_avr_uinst_d5_asr(avr, &new_pc, &cycle, r1);
				break;
			case	k_avr_uinst_b3_bclr:
				_avr_uinst_b3_bclr(avr, &new_pc, &cycle, r1);
				break;
			case	k_avr_uinst_b3_bset:
				_avr_uinst_b3_bset(avr, &new_pc, &cycle, r1);
				break;
			case	k_avr_uinst_d5_com:
				_avr_uinst_d5_com(avr, &new_pc, &cycle, r1);
				break;
			case	k_avr_uinst_d5_dec:
				_avr_uinst_d5_dec(avr, &new_pc, &cycle, r1);
				break;
			case	k_avr_uinst_d5_inc:
				_avr_uinst_d5_inc(avr, &new_pc, &cycle, r1);
				break;
			case	k_avr_uinst_d5_lpm_z0:
				_avr_uinst_d5_lpm_z0(avr, &new_pc, &cycle, r1);
				break;
			case	k_avr_uinst_d5_lpm_z1:
				_avr_uinst_d5_lpm_z1(avr, &new_pc, &cycle, r1);
				break;
			case	k_avr_uinst_d5_lsr:
				_avr_uinst_d5_lsr(avr, &new_pc, &cycle, r1);
				break;
			case	k_avr_uinst_d5_neg:
				_avr_uinst_d5_neg(avr, &new_pc, &cycle, r1);
				break;
			case	k_avr_uinst_d5_pop:
				_avr_uinst_d5_pop(avr, &new_pc, &cycle, r1);
				break;
			case	k_avr_uinst_d5_push:
				_avr_uinst_d5_push(avr, &new_pc, &cycle, r1);
				break;
			case	k_avr_uinst_d5_ror:
				_avr_uinst_d5_ror(avr, &new_pc, &cycle, r1);
				break;
			case	k_avr_uinst_d5_swap:
				_avr_uinst_d5_swap(avr, &new_pc, &cycle, r1);
				break;
			default:
				goto notFound;
				break;
			}
	} else {
		switch(soc) {
			case	k_avr_uinst_nop:
				_avr_uinst_nop(avr, &new_pc, &cycle);
				break;
			case	k_avr_uinst_ret:
				_avr_uinst_ret(avr, &new_pc, &cycle);
				break;
			case	k_avr_uinst_reti:
				_avr_uinst_reti(avr, &new_pc, &cycle);
				break;
			case	k_avr_uinst_sleep:
				_avr_uinst_sleep(avr, &new_pc, &cycle);
				break;
			default:
				goto	notFound;
				break;
		}
	}

	avr->cycle += cycle;
	return(new_pc);

notFound: /* run it through the decoder(which also runs the instruction), we'll (most likely) get it on the next run. */
	return(avr_decode_one(avr));
}

void avr_core_run_many(avr_t* avr) {
	avr_cycle_count_t	nextTimerCycle;
	avr_flashaddr_t		new_pc = avr->pc;

runAgain:
	if(avr->state == cpu_Running)
		new_pc = avr_fast_core_run_one(avr);

	if(avr->sreg[S_I] && !avr->i_shadow)
			avr->interrupts.pending_wait++;
	avr->i_shadow=avr->sreg[S_I];

	nextTimerCycle = avr_cycle_timer_process(avr);

	avr->pc = new_pc;
	
	if (avr->state == cpu_Sleeping) {
		if (!avr->sreg[S_I]) {
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
		return;
	}

	if(avr->sreg[S_I]) {
		if(1 < avr->interrupts.pending_wait) {
			avr->interrupts.pending_wait--;
		} else if(avr_has_pending_interrupts(avr)) {
			avr_service_interrupts(avr);
			return;
		}
	}
	
	if(nextTimerCycle<avr->cycle)
		goto runAgain;
}

