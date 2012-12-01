/*
	sim_core.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
	Copyright 2013 Michel Hughes <squirmyworms@embarqmail.com>

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

#define _avr_sp_get _avr_sp_get_v2
#define _avr_sp_set _avr_sp_set_v2

#define _avr_push16 _avr_push16_v2

#if 0
#define new_pc avr->new_pc
#endif

#define xT(x) x

#define xSTATE(_f, args...) { \
		printf("0x%04x: " _f, avr->pc, ##args); \
/*		if(0x05c2==avr->pc) \
			avr->state=cpu_Stopped; \
*/	}

// SREG bit names
extern const char * _sreg_bit_name;

#define xSREG() {\
	printf("%04x: \t\t\t\t\t\t\t\t\tSREG = ", avr->pc); \
	for (int _sbi = 0; _sbi < 8; _sbi++)\
		printf("%c", avr->sreg[_sbi] ? toupper(_sreg_bit_name[_sbi]) : '.');\
	printf("\n");\
}

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
		if (avr->trace_data->codeline && avr->trace_data->codeline[avr->pc>>1]) {\
			const char * symn = avr->trace_data->codeline[avr->pc>>1]->symbol; \
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
#define T(w)
#define REG_TOUCH(a, r)
#define STATE(_f, args...)
#define SREG()
#endif

/*
 * Set any address to a value; split between registers and SRAM
 */
static inline void _avr_set_ram(avr_t * avr, uint16_t addr, uint8_t v)
{
	if (addr < 256) {
		REG_TOUCH(avr, addr);

		if (addr == R_SREG) {
			avr->data[R_SREG] = v;
			// unsplit the SREG
			SET_SREG_FROM(avr, v);
			SREG();
		}
		if (addr > 31) {
			uint8_t io = AVR_DATA_TO_IO(addr);
			if (avr->io[io].w.c)
				avr->io[io].w.c(avr, addr, v, avr->io[io].w.param);
			else
				avr->data[addr] = v;
			if (avr->io[io].irq) {
				avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
				for (int i = 0; i < 8; i++)
					avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);				
			}
		} else
			avr->data[addr] = v;
	} else
		avr_core_watch_write(avr, addr, v);
}

/*
 * Get a value from SRAM.
 */
static inline uint8_t _avr_get_ram(avr_t * avr, uint16_t addr)
{
	if (addr == R_SREG) {
		/*
		 * SREG is special it's reconstructed when read
		 * while the core itself uses the "shortcut" array
		 */
		READ_SREG_INTO(avr, avr->data[R_SREG]);
		
	} else if (addr > 31 && addr < 256) {
		uint8_t io = AVR_DATA_TO_IO(addr);
		
		if (avr->io[io].r.c)
			avr->data[addr] = avr->io[io].r.c(avr, addr, avr->io[io].r.param);

		if (avr->io[io].irq) {
			uint8_t v = avr->data[addr];
			avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
			for (int i = 0; i < 8; i++)
				avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);				
		}
	}
	return avr_core_watch_read(avr, addr);
}

#include "sim_core_v2_mem_funcs.h"

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

/****************************************************************************\
 *
 * Helper functions for calculating the status register bit values.
 * See the Atmel data sheet for the instruction set for more info.
 *
\****************************************************************************/

static inline uint8_t
get_add_carry (uint8_t res, uint8_t rd, uint8_t rr, int b)
{
    uint8_t resb = res >> b & 0x1;
    uint8_t rdb = rd >> b & 0x1;
    uint8_t rrb = rr >> b & 0x1;
    return (rdb & rrb) | (rrb & ~resb) | (~resb & rdb);
}

static inline uint8_t
get_add_overflow (uint8_t res, uint8_t rd, uint8_t rr)
{
    uint8_t res7 = res >> 7 & 0x1;
    uint8_t rd7 = rd >> 7 & 0x1;
    uint8_t rr7 = rr >> 7 & 0x1;
    return (rd7 & rr7 & ~res7) | (~rd7 & ~rr7 & res7);
}

static inline uint8_t
get_sub_carry (uint8_t res, uint8_t rd, uint8_t rr, int b)
{
    uint8_t resb = res >> b & 0x1;
    uint8_t rdb = rd >> b & 0x1;
    uint8_t rrb = rr >> b & 0x1;
    return (~rdb & rrb) | (rrb & resb) | (resb & ~rdb);
}

static inline uint8_t
get_sub_overflow (uint8_t res, uint8_t rd, uint8_t rr)
{
    uint8_t res7 = res >> 7 & 0x1;
    uint8_t rd7 = rd >> 7 & 0x1;
    uint8_t rr7 = rr >> 7 & 0x1;
    return (rd7 & ~rr7 & ~res7) | (~rd7 & rr7 & res7);
}

static inline uint8_t
get_compare_carry (uint8_t res, uint8_t rd, uint8_t rr, int b)
{
    uint8_t resb = (res >> b) & 0x1;
    uint8_t rdb = (rd >> b) & 0x1;
    uint8_t rrb = (rr >> b) & 0x1;
    return (~rdb & rrb) | (rrb & resb) | (resb & ~rdb);
}

static inline uint8_t
get_compare_overflow (uint8_t res, uint8_t rd, uint8_t rr)
{
    uint8_t res7 = res >> 7 & 0x1;
    uint8_t rd7 = rd >> 7 & 0x1;
    uint8_t rr7 = rr >> 7 & 0x1;
    return (rd7 & ~rr7 & ~res7) | (~rd7 & rr7 & res7);
}

static inline int _avr_is_instruction_32_bits(avr_t * avr, avr_flashaddr_t pc)
{
	uint16_t o = (avr->flash[pc] | (avr->flash[pc+1] << 8)) & 0xfc0f;
	return	o == 0x9200 || // STS ! Store Direct to Data Space
			o == 0x9000 || // LDS Load Direct from Data Space
			o == 0x940c || // JMP Long Jump
			o == 0x940d || // JMP Long Jump
			o == 0x940e ||  // CALL Long Call to sub
			o == 0x940f; // CALL Long Call to sub
}

