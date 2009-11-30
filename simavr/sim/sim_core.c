/*
	sim_core.c

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "simavr.h"
#include "sim_core.h"

// SREG bit names
const char * _sreg_bit_name = "cznvshti";

/*
 * Handle "touching" registers, marking them changed.
 * This is used only for debugging purposes to be able to
 * print the effects of each instructions on registers
 */
#define REG_TOUCH(a, r) (a)->touched[(r) >> 5] |= (1 << ((r) & 0x1f))
#define REG_ISTOUCHED(a, r) ((a)->touched[(r) >> 5] & (1 << ((r) & 0x1f)))

/*
 * This allows a "special case" to skip indtruction tracing when in these
 * symbols. since printf() is useful to have, but generates a lot of cycles
 */
int dont_trace(const char * name)
{
	return (
		!strcmp(name, "uart_putchar") ||
		!strcmp(name, "fputc") ||
		!strcmp(name, "printf") ||
		!strcmp(name, "vfprintf") ||
		!strcmp(name, "__ultoa_invert") ||
		!strcmp(name, "__prologue_saves__") ||
		!strcmp(name, "__epilogue_restores__"));
}

int donttrace = 0;

#define STATE(_f, args...) { \
	if (avr->trace) {\
		if (avr->codeline[avr->pc>>1]) {\
			const char * symn = avr->codeline[avr->pc>>1]->symbol; \
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

/*
 * Set a register (r < 256)
 * if it's an IO regisrer (> 31) also (try to) call any callback that was
 * registered to track changes to that register.
 */
static inline void _avr_set_r(avr_t * avr, uint8_t r, uint8_t v)
{
	REG_TOUCH(avr, r);

	if (r == R_SREG) {
		avr->data[r] = v;
		// unsplit the SREG
		for (int i = 0; i < 8; i++)
			avr->sreg[i] = (avr->data[R_SREG] & (1 << i)) != 0;
		SREG();
	}
	if (r > 31) {
		uint8_t io = AVR_DATA_TO_IO(r);
		if (avr->iow[io].w)
			avr->iow[io].w(avr, r, v, avr->iow[io].param);
		else
			avr->data[r] = v;
	} else
		avr->data[r] = v;
}

/*
 * Stack pointer access
 */
inline uint16_t _avr_sp_get(avr_t * avr)
{
	return avr->data[R_SPL] | (avr->data[R_SPH] << 8);
}

inline void _avr_sp_set(avr_t * avr, uint16_t sp)
{
	_avr_set_r(avr, R_SPL, sp);
	_avr_set_r(avr, R_SPH, sp >> 8);
}

/*
 * Set any address to a value; split between registers and SRAM
 */
static inline void _avr_set_ram(avr_t * avr, uint16_t addr, uint8_t v)
{
	if (addr < 256)
		_avr_set_r(avr, addr, v);
	else
		avr_core_watch_write(avr, addr, v);
}

/*
 * Get a value from SRAM.
 */
static inline uint8_t _avr_get_ram(avr_t * avr, uint16_t addr)
{
	if (addr > 31 && addr < 256) {
		uint8_t io = AVR_DATA_TO_IO(addr);
		if (avr->ior[io].r)
			avr->data[addr] = avr->ior[io].r(avr, addr, avr->ior[io].param);
	}
	return avr_core_watch_read(avr, addr);
}

/*
 * Stack oush accessors. Push/pop 8 and 16 bits
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

inline void _avr_push16(avr_t * avr, uint16_t v)
{
	_avr_push8(avr, v >> 8);
	_avr_push8(avr, v);
}

static inline uint16_t _avr_pop16(avr_t * avr)
{
	uint16_t res = _avr_pop8(avr);
	res |= _avr_pop8(avr) << 8;
	return res;
}

/*
 * "Pretty" register names
 */
const char * reg_names[255] = {
		[R_XH] = "XH", [R_XL] = "XL",
		[R_YH] = "YH", [R_YL] = "YL",
		[R_ZH] = "ZH", [R_ZL] = "ZL",
		[R_SPH] = "SPH", [R_SPL] = "SPL",
		[R_SREG] = "SREG",
};


const char * avr_regname(uint8_t reg)
{
	if (!reg_names[reg]) {
		char tt[16];
		if (reg < 32)
			sprintf(tt, "r%d", reg);
		else
			sprintf(tt, "io:%02x", reg);
		reg_names[reg] = strdup(tt);
	}
	return reg_names[reg];
}

/*
 * Called when an invalid opcode is decoded
 */
static void _avr_invalid_opcode(avr_t * avr)
{
	printf("\e[31m*** %04x: %-25s Invalid Opcode SP=%04x O=%04x \e[0m\n",
			avr->pc, avr->codeline[avr->pc>>1]->symbol, _avr_sp_get(avr), avr->flash[avr->pc] | (avr->flash[avr->pc+1]<<8));
}

/*
 * Dump changed registers when tracing
 */
void avr_dump_state(avr_t * avr)
{
	if (!avr->trace || donttrace)
		return;

	int doit = 0;

	for (int r = 0; r < 3 && !doit; r++)
		if (avr->touched[r])
			doit = 1;
	if (!doit)
		return;
	printf("                                       ->> ");
	const int r16[] = { R_SPL, R_XL, R_YL, R_ZL };
	for (int i = 0; i < 4; i++)
		if (REG_ISTOUCHED(avr, r16[i]) || REG_ISTOUCHED(avr, r16[i]+1)) {
			REG_TOUCH(avr, r16[i]);
			REG_TOUCH(avr, r16[i]+1);
		}

	for (int i = 0; i < 3*32; i++)
		if (REG_ISTOUCHED(avr, i)) {
			printf("%s=%02x ", avr_regname(i), avr->data[i]);
		}
	printf("\n");
}

#define get_r_d_10(o) \
		const uint8_t r = ((o >> 5) & 0x10) | (o & 0xf); \
		const uint8_t d = (o >> 4) & 0x1f;\
		const uint8_t vd = avr->data[d], vr =avr->data[r];
#define get_k_r16(o) \
		const uint8_t r = 16 + ((o >> 4) & 0xf); \
		const uint8_t k = ((o & 0x0f00) >> 4) | (o & 0xf);

/*
 * Add a "jump" address to the jump trace buffer
 */
#define TRACE_JUMP()\
	avr->old[avr->old_pci].pc = avr->pc;\
	avr->old[avr->old_pci].sp = _avr_sp_get(avr);\
	avr->old_pci = (avr->old_pci + 1) & (OLD_PC_SIZE-1);\

#if AVR_STACK_WATCH
#define STACK_FRAME_PUSH()\
	avr->stack_frame[avr->stack_frame_index].pc = avr->pc;\
	avr->stack_frame[avr->stack_frame_index].sp = _avr_sp_get(avr);\
	avr->stack_frame_index++; 
#define STACK_FRAME_POP()\
	if (avr->stack_frame_index > 0) \
		avr->stack_frame_index--;
#else
#define STACK_FRAME_PUSH()
#define STACK_FRAME_POP()
#endif

/****************************************************************************\
 *
 * Helper functions for calculating the status register bit values.
 * See the Atmel data sheet for the instuction set for more info.
 *
\****************************************************************************/

static uint8_t
get_add_carry (uint8_t res, uint8_t rd, uint8_t rr, int b)
{
    uint8_t resb = res >> b & 0x1;
    uint8_t rdb = rd >> b & 0x1;
    uint8_t rrb = rr >> b & 0x1;
    return (rdb & rrb) | (rrb & ~resb) | (~resb & rdb);
}

static  uint8_t
get_add_overflow (uint8_t res, uint8_t rd, uint8_t rr)
{
    uint8_t res7 = res >> 7 & 0x1;
    uint8_t rd7 = rd >> 7 & 0x1;
    uint8_t rr7 = rr >> 7 & 0x1;
    return (rd7 & rr7 & ~res7) | (~rd7 & ~rr7 & res7);
}

static  uint8_t
get_sub_carry (uint8_t res, uint8_t rd, uint8_t rr, int b)
{
    uint8_t resb = res >> b & 0x1;
    uint8_t rdb = rd >> b & 0x1;
    uint8_t rrb = rr >> b & 0x1;
    return (~rdb & rrb) | (rrb & resb) | (resb & ~rdb);
}

static  uint8_t
get_sub_overflow (uint8_t res, uint8_t rd, uint8_t rr)
{
    uint8_t res7 = res >> 7 & 0x1;
    uint8_t rd7 = rd >> 7 & 0x1;
    uint8_t rr7 = rr >> 7 & 0x1;
    return (rd7 & ~rr7 & ~res7) | (~rd7 & rr7 & res7);
}

static  uint8_t
get_compare_carry (uint8_t res, uint8_t rd, uint8_t rr, int b)
{
    uint8_t resb = (res >> b) & 0x1;
    uint8_t rdb = (rd >> b) & 0x1;
    uint8_t rrb = (rr >> b) & 0x1;
    return (~rdb & rrb) | (rrb & resb) | (resb & ~rdb);
}

static  uint8_t
get_compare_overflow (uint8_t res, uint8_t rd, uint8_t rr)
{
    res >>= 7; rd >>= 7; rr >>= 7;
    /* The atmel data sheet says the second term is ~rd7 for CP
     * but that doesn't make any sense. You be the judge. */
    return (rd & ~rr & ~res) | (~rd & rr & res);
}

static inline int _avr_is_instruction_32_bits(avr_t * avr, uint32_t pc)
{
	uint16_t o = (avr->flash[pc] | (avr->flash[pc+1] << 8)) & 0xfc0f;
	return	o == 0x9200 || // STS ! Store Direct to Data Space
			o == 0x9000 || // LDS Load Direct from Data Space
			o == 0x940c || // JMP Long Jump
			o == 0x940d || // JMP Long Jump
			o == 0x940e ||  // CALL Long Call to sub
			o == 0x940f; // CALL Long Call to sub
}

/*
 * Main opcode decoder
 * 
 * The decoder was written by following the datasheet in no particular order.
 * As I went along, I noticed "bit patterns" that could be used to factor opcodes
 * However, a lot of these only becane apparent later on, so SOME instructions
 * (skip of bit set etc) are compact, and some could use some refactoring (the ALU
 * ones scream to be factored).
 * I assume that the decoder could easily be 2/3 of it's current size.
 * 
 * + It lacks the "extended" XMega jumps. 
 * + It also doesn't check wether the core it's
 *   emulating is suposed to have the fancy instructions, like multiply and such.
 * 
 * for now all instructions take "one" cycle, the cycle+=<extra> needs to be added.
 */
uint16_t avr_run_one(avr_t * avr)
{
	/*
	 * this traces spurious reset or bad jump/opcodes and dumps the last 32 "jumps" to track it down
	 */
	if ((avr->pc == 0 && avr->cycle > 0) || avr->pc >= avr->codeend) {
		avr->trace = 1;
		STATE("RESET\n");
		CRASH();
	}

	uint32_t	opcode = (avr->flash[avr->pc + 1] << 8) | avr->flash[avr->pc];
	uint32_t	new_pc = avr->pc + 2;	// future "default" pc
	int 		cycle = 1;

	avr->touched[0] = avr->touched[1] = avr->touched[2] = 0;

	switch (opcode & 0xf000) {
		case 0x0000: {
			switch (opcode) {
				case 0x0000: {	// NOP
					STATE("nop\n");
				}	break;
				default: {
					switch (opcode & 0xfc00) {
						case 0x0400: {	// CPC compare with carry 0000 01rd dddd rrrr
							get_r_d_10(opcode);
							uint8_t res = vd - vr - avr->sreg[S_C];
							STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
							if (res)
								avr->sreg[S_Z] = 0;
							avr->sreg[S_H] = get_compare_carry(res, vd, vr, 3);
							avr->sreg[S_V] = get_compare_overflow(res, vd, vr);
							avr->sreg[S_N] = (res >> 7) & 1;
							avr->sreg[S_C] = get_compare_carry(res, vd, vr, 7);
							avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
							SREG();
						}	break;
						case 0x0c00: {	// ADD without carry 0000 11 rd dddd rrrr
							get_r_d_10(opcode);
							uint8_t res = vd + vr;
							if (r == d) {
								STATE("lsl %s[%02x] = %02x\n", avr_regname(d), vd, res & 0xff);
							} else {
								STATE("add %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
							}
							_avr_set_r(avr, d, res);
							avr->sreg[S_Z] = res == 0;
							avr->sreg[S_H] = get_add_carry(res, vd, vr, 3);
							avr->sreg[S_V] = get_add_overflow(res, vd, vr);
							avr->sreg[S_N] = (res >> 7) & 1;
							avr->sreg[S_C] = get_add_carry(res, vd, vr, 7);
							avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
							SREG();
						}	break;
						case 0x0800: {	// SBC substract with carry 0000 10rd dddd rrrr
							get_r_d_10(opcode);
							uint8_t res = vd - vr - avr->sreg[S_C];
							STATE("sbc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), avr->data[d], avr_regname(r), avr->data[r], res);
							_avr_set_r(avr, d, res);
							if (res)
								avr->sreg[S_Z] = 0;
							avr->sreg[S_H] = get_sub_carry(res, vd, vr, 3);
							avr->sreg[S_V] = get_sub_overflow(res, vd, vr);
							avr->sreg[S_N] = (res >> 7) & 1;
							avr->sreg[S_C] = get_sub_carry(res, vd, vr, 7);
							avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
							SREG();
						}	break;
						default:
							switch (opcode & 0xff00) {
								case 0x0100: {	// MOVW – Copy Register Word 0000 0001 dddd rrrr
									uint8_t d = ((opcode >> 4) & 0xf) << 1;
									uint8_t r = ((opcode) & 0xf) << 1;
									STATE("movw %s:%s, %s:%s[%02x%02x]\n", avr_regname(d), avr_regname(d+1), avr_regname(r), avr_regname(r+1), avr->data[r+1], avr->data[r]);
									_avr_set_r(avr, d, avr->data[r]);
									_avr_set_r(avr, d+1, avr->data[r+1]);
								}	break;
								case 0x0200: {	// MULS – Multiply Signed 0000 0010 dddd rrrr
									int8_t r = opcode & 0xf;
									int8_t d = (opcode >> 4) & 0xf;
									int16_t res = ((int8_t)avr->data[r]) * ((int8_t)avr->data[d]);
									STATE("muls %s[%d], %s[%02x] = %d\n", avr_regname(d), ((int8_t)avr->data[d]), avr_regname(r), ((int8_t)avr->data[r]), res);
									_avr_set_r(avr, 0, res);
									_avr_set_r(avr, 1, res >> 8);
									avr->sreg[S_C] = (res >> 15) & 1;
									avr->sreg[S_Z] = res == 0;
									SREG();
								}	break;
								case 0x0300: {	// multiplications
									int8_t r = 16 + (opcode & 0x7);
									int8_t d = 16 + ((opcode >> 4) & 0x7);
									int16_t res = 0;
									uint8_t c = 0;
									const char * name = "";
									switch (opcode & 0x88) {
										case 0x00: 	// MULSU – Multiply Signed Unsigned 0000 0011 0ddd 0rrr
											res = ((uint8_t)avr->data[r]) * ((int8_t)avr->data[d]);
											c = (res >> 15) & 1;
											name = "mulsu";
											break;
										case 0x08: 	// FMUL Fractional Multiply Unsigned 0000 0011 0ddd 1rrr
											res = ((uint8_t)avr->data[r]) * ((uint8_t)avr->data[d]);
											c = (res >> 15) & 1;
											res <<= 1;
											name = "fmul";
											break;
										case 0x80: 	// FMULS – Multiply Signed  0000 0011 1ddd 0rrr
											res = ((int8_t)avr->data[r]) * ((int8_t)avr->data[d]);
											c = (res >> 15) & 1;
											res <<= 1;
											name = "fmuls";
											break;
										case 0x88: 	// FMULSU – Multiply Signed Unsigned 0000 0011 1ddd 0rrr
											res = ((uint8_t)avr->data[r]) * ((int8_t)avr->data[d]);
											c = (res >> 15) & 1;
											res <<= 1;
											name = "fmulsu";
											break;
									}
									cycle++;
									STATE("%s %s[%d], %s[%02x] = %d\n", name, avr_regname(d), ((int8_t)avr->data[d]), avr_regname(r), ((int8_t)avr->data[r]), res);
									_avr_set_r(avr, 0, res);
									_avr_set_r(avr, 1, res >> 8);
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
					get_r_d_10(opcode);
					uint8_t res = vd - vr;
					STATE("sub %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
					_avr_set_r(avr, d, res);
					avr->sreg[S_Z] = res == 0;
					avr->sreg[S_H] = get_sub_carry(res, vd, vr, 3);
					avr->sreg[S_V] = get_sub_overflow(res, vd, vr);
					avr->sreg[S_N] = (res >> 7) & 1;
					avr->sreg[S_C] = get_sub_carry(res, vd, vr, 7);
					avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
					SREG();
				}	break;
				case 0x1000: {	// CPSE Compare, skip if equal 0000 10 rd dddd rrrr
					get_r_d_10(opcode);
					uint16_t res = vd == vr;
					STATE("cpse %s[%02x], %s[%02x]\t; Will%s skip\n", avr_regname(d), avr->data[d], avr_regname(r), avr->data[r], res ? "":"not ");
					if (res) {
						if (_avr_is_instruction_32_bits(avr, new_pc)) {
							new_pc += 4; cycle += 2;
						} else {
							new_pc += 2; cycle++;
						}
					}
				}	break;
				case 0x1400: {	// CP Compare 0000 10 rd dddd rrrr
					get_r_d_10(opcode);
					uint8_t res = vd - vr;
					STATE("cp %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
					avr->sreg[S_Z] = res == 0;
					avr->sreg[S_H] = get_compare_carry(res, vd, vr, 3);
					avr->sreg[S_V] = get_compare_overflow(res, vd, vr);
					avr->sreg[S_N] = res >> 7;
					avr->sreg[S_C] = get_compare_carry(res, vd, vr, 7);
					avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
					SREG();
				}	break;
				case 0x1c00: {	// ADD with carry 0001 11 rd dddd rrrr
					get_r_d_10(opcode);
					uint8_t res = vd + vr + avr->sreg[S_C];
					if (r == d) {
						STATE("rol %s[%02x] = %02x\n", avr_regname(d), avr->data[d], res);
					} else {
						STATE("addc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), avr->data[d], avr_regname(r), avr->data[r], res);
					}
					_avr_set_r(avr, d, res);
					avr->sreg[S_Z] = res == 0;
					avr->sreg[S_H] = get_add_carry(res, vd, vr, 3);
					avr->sreg[S_V] = get_add_overflow(res, vd, vr);
					avr->sreg[S_N] = (res >> 7) & 1;
					avr->sreg[S_C] = get_add_carry(res, vd, vr, 7);
					avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
					SREG();
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0x2000: {
			switch (opcode & 0xfc00) {
				case 0x2000: {	// AND	0010 00rd dddd rrrr
					get_r_d_10(opcode);
					uint8_t res = vd & vr;
					if (r == d) {
						STATE("tst %s[%02x]\n", avr_regname(d), avr->data[d]);
					} else {
						STATE("and %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
					}
					_avr_set_r(avr, d, res);
					avr->sreg[S_Z] = res == 0;
					avr->sreg[S_N] = (res >> 7) & 1;
					avr->sreg[S_V] = 0;
					avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
					SREG();
				}	break;
				case 0x2400: {	// EOR	0010 01rd dddd rrrr
					get_r_d_10(opcode);
					uint8_t res = vd ^ vr;
					if (r==d) {
						STATE("clr %s[%02x]\n", avr_regname(d), avr->data[d]);
					} else {
						STATE("eor %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
					}
					_avr_set_r(avr, d, res);
					avr->sreg[S_Z] = res == 0;
					avr->sreg[S_N] = (res >> 7) & 1;
					avr->sreg[S_V] = 0;
					avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
					SREG();
				}	break;
				case 0x2800: {	// OR Logical OR	0010 10rd dddd rrrr
					get_r_d_10(opcode);
					uint8_t res = vd | vr;
					STATE("or %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
					_avr_set_r(avr, d, res);
					avr->sreg[S_Z] = res == 0;
					avr->sreg[S_N] = (res >> 7) & 1;
					avr->sreg[S_V] = 0;
					avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
					SREG();
				}	break;
				case 0x2c00: {	// MOV	0010 11rd dddd rrrr
					get_r_d_10(opcode);
					uint8_t res = vr;
					STATE("mov %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);
					_avr_set_r(avr, d, res);
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0x3000: {	// CPI 0011 KKKK rrrr KKKK
			get_k_r16(opcode);
			uint8_t vr = avr->data[r];
			uint8_t res = vr - k;
			STATE("cpi %s[%02x], 0x%02x\n", avr_regname(r), vr, k);

			avr->sreg[S_Z] = res == 0;
			avr->sreg[S_H] = get_compare_carry(res, vr, k, 3);
			avr->sreg[S_V] = get_compare_overflow(res, vr, k);
			avr->sreg[S_N] = (res >> 7) & 1;
			avr->sreg[S_C] = get_compare_carry(res, vr, k, 7);
			avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
			SREG();
		}	break;

		case 0x4000: {	// SBCI Subtract Immediate With Carry 0101 10 kkkk dddd kkkk
			get_k_r16(opcode);
			uint8_t vr = avr->data[r];
			uint8_t res = vr - k - avr->sreg[S_C];
			STATE("sbci %s[%02x], 0x%02x = %02x\n", avr_regname(r), avr->data[r], k, res);
			_avr_set_r(avr, r, res);
			avr->sreg[S_Z] = res  == 0;
			avr->sreg[S_N] = (res >> 7) & 1;
			avr->sreg[S_C] = (k + avr->sreg[S_C]) > vr;
			avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
			SREG();
		}	break;

		case 0x5000: {	// SUB Subtract Immediate 0101 10 kkkk dddd kkkk
			get_k_r16(opcode);
			uint8_t vr = avr->data[r];
			uint8_t res = vr - k;
			STATE("subi %s[%02x], 0x%02x = %02x\n", avr_regname(r), avr->data[r], k, res);
			_avr_set_r(avr, r, res);
			avr->sreg[S_Z] = res  == 0;
			avr->sreg[S_N] = (res >> 7) & 1;
			avr->sreg[S_C] = k > vr;
			avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
			SREG();
		}	break;

		case 0x6000: {	// ORI aka SBR	Logical AND with Immediate	0110 kkkk dddd kkkk
			get_k_r16(opcode);
			uint8_t res = avr->data[r] | k;
			STATE("ori %s[%02x], 0x%02x\n", avr_regname(r), avr->data[r], k);
			_avr_set_r(avr, r, res);
			avr->sreg[S_Z] = res == 0;
			avr->sreg[S_N] = (res >> 7) & 1;
			avr->sreg[S_V] = 0;
			avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
			SREG();
		}	break;

		case 0x7000: {	// ANDI	Logical AND with Immediate	0111 kkkk dddd kkkk
			get_k_r16(opcode);
			uint8_t res = avr->data[r] & k;
			STATE("andi %s[%02x], 0x%02x\n", avr_regname(r), avr->data[r], k);
			_avr_set_r(avr, r, res);
			avr->sreg[S_Z] = res == 0;
			avr->sreg[S_N] = (res >> 7) & 1;
			avr->sreg[S_V] = 0;
			avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
			SREG();
		}	break;

		case 0xa000:
		case 0x8000: {
			switch (opcode & 0xd008) {
				case 0xa000:
				case 0x8000: {	// LD (LDD) – Load Indirect using Z 10q0 qq0r rrrr 0qqq
					uint16_t v = avr->data[R_ZL] | (avr->data[R_ZH] << 8);
					uint8_t r = (opcode >> 4) & 0x1f;
					uint8_t q = ((opcode & 0x2000) >> 8) | ((opcode & 0x0c00) >> 7) | (opcode & 0x7);

					if (opcode & 0x0200) {
						STATE("st (Z+%d[%04x]), %s[%02x]\n", q, v+q, avr_regname(r), avr->data[r]);
						_avr_set_ram(avr, v+q, avr->data[r]);
					} else {
						STATE("ld %s, (Z+%d[%04x])=[%02x]\n", avr_regname(r), q, v+q, avr->data[v+q]);
						_avr_set_r(avr, r, _avr_get_ram(avr, v+q));
					}
					cycle += 2;
				}	break;
				case 0xa008:
				case 0x8008: {	// LD (LDD) – Load Indirect using Y 10q0 qq0r rrrr 1qqq
					uint16_t v = avr->data[R_YL] | (avr->data[R_YH] << 8);
					uint8_t r = (opcode >> 4) & 0x1f;
					uint8_t q = ((opcode & 0x2000) >> 8) | ((opcode & 0x0c00) >> 7) | (opcode & 0x7);

					if (opcode & 0x0200) {
						STATE("st (Y+%d[%04x]), %s[%02x]\n", q, v+q, avr_regname(r), avr->data[r]);
						_avr_set_ram(avr, v+q, avr->data[r]);
					} else {
						STATE("ld %s, (Y+%d[%04x])=[%02x]\n", avr_regname(r), q, v+q, avr->data[v+q]);
						_avr_set_r(avr, r, _avr_get_ram(avr, v+q));
					}
					cycle += 2;
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0x9000: {
			/* this is an annoying special case, but at least these lines handle all the SREG set/clear opcodes */
			if ((opcode & 0xff0f) == 0x9408) {
				uint8_t b = (opcode >> 4) & 7;
				STATE("%s%c\n", opcode & 0x0080 ? "cl" : "se", _sreg_bit_name[b]);
				avr->sreg[b] = (opcode & 0x0080) == 0;
				SREG();
			} else switch (opcode) {
				case 0x9588: { // SLEEP
					STATE("sleep\n");
					avr->state = cpu_Sleeping;
				}	break;
				case 0x9598: { // BREAK
					STATE("break\n");
				}	break;
				case 0x95a8: { // WDR
					STATE("wdr\n");
				}	break;
				case 0x9409: { // IJMP Indirect jump
					uint16_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8);
					STATE("ijmp Z[%04x]\n", z << 1);
					new_pc = z << 1;
					cycle++;
					TRACE_JUMP();
				}	break;
				case 0x9509: { // ICALL Indirect Call to Subroutine
					uint16_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8);
					STATE("icall Z[%04x]\n", z << 1);
					_avr_push16(avr, new_pc >> 1);
					new_pc = z << 1;
					cycle += 2;
					TRACE_JUMP();
					STACK_FRAME_PUSH();
				}	break;
				case 0x9518: 	// RETI
				case 0x9508: {	// RET
					new_pc = _avr_pop16(avr) << 1;
					if (opcode & 0x10)	// reti
						avr->sreg[S_I] = 1;
					cycle += 3;
					STATE("ret%s\n", opcode & 0x10 ? "i" : "");
					TRACE_JUMP();
					STACK_FRAME_POP();
				}	break;
				case 0x95c8: {	// LPM Load Program Memory R0 <- (Z)
					uint16_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8);
					STATE("lpm %s, (Z[%04x])\n", avr_regname(0), z);
					_avr_set_r(avr, 0, avr->flash[z]);
				}	break;
				case 0x9408:case 0x9418:case 0x9428:case 0x9438:case 0x9448:case 0x9458:case 0x9468:
				case 0x9478:
				{	// BSET 1001 0100 0ddd 1000
					uint8_t b = (opcode >> 4) & 7;
					avr->sreg[b] = 1;
					STATE("bset %c\n", _sreg_bit_name[b]);
					SREG();
				}	break;
				case 0x9488:case 0x9498:case 0x94a8:case 0x94b8:case 0x94c8:case 0x94d8:case 0x94e8:
				case 0x94f8:
				{	// BSET 1001 0100 0ddd 1000
					uint8_t b = (opcode >> 4) & 7;
					avr->sreg[b] = 0;
					STATE("bclr %c\n", _sreg_bit_name[b]);
					SREG();
				}	break;
				default:  {
					switch (opcode & 0xfe0f) {
						case 0x9000: {	// LDS Load Direct from Data Space, 32 bits
							uint8_t r = (opcode >> 4) & 0x1f;
							uint16_t x = (avr->flash[new_pc+1] << 8) | avr->flash[new_pc];
							new_pc += 2;
							STATE("lds %s[%02x], 0x%04x\n", avr_regname(r), avr->data[r], x);
							_avr_set_r(avr, r, _avr_get_ram(avr, x));
							cycle++;
						}	break;
						case 0x9005:
						case 0x9004: {	// LPM Load Program Memory 1001 000d dddd 01oo
							uint16_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8);
							uint8_t r = (opcode >> 4) & 0x1f;
							int op = opcode & 3;
							STATE("lpm %s, (Z[%04x]%s)\n", avr_regname(r), z, opcode?"+":"");
							_avr_set_r(avr, r, avr->flash[z]);
							if (op == 1) {
								z++;
								_avr_set_r(avr, R_ZH, z >> 8);
								_avr_set_r(avr, R_ZL, z);
							}
							cycle += 2;
						}	break;
						case 0x900c:
						case 0x900d:
						case 0x900e: {	// LD Load Indirect from Data using X 1001 000r rrrr 11oo
							int op = opcode & 3;
							uint8_t r = (opcode >> 4) & 0x1f;
							uint16_t x = (avr->data[R_XH] << 8) | avr->data[R_XL];
							STATE("ld %s, %sX[%04x]%s\n", avr_regname(r), op == 2 ? "--" : "", x, op == 1 ? "++" : "");

							if (op == 2) x--;
							_avr_set_r(avr, r, _avr_get_ram(avr, x));
							if (op == 1) x++;
							_avr_set_r(avr, R_XH, x >> 8);
							_avr_set_r(avr, R_XL, x);
						}	break;
						case 0x920c:
						case 0x920d:
						case 0x920e: {	// ST Store Indirect Data Space X 1001 001r rrrr 11oo
							int op = opcode & 3;
							uint8_t r = (opcode >> 4) & 0x1f;
							uint16_t x = (avr->data[R_XH] << 8) | avr->data[R_XL];
							STATE("st %sX[%04x]%s, %s[%02x] \n", op == 2 ? "--" : "", x, op == 1 ? "++" : "", avr_regname(r), avr->data[r]);
							cycle++;
							if (op == 2) x--;
							_avr_set_ram(avr, x, avr->data[r]);
							if (op == 1) x++;
							_avr_set_r(avr, R_XH, x >> 8);
							_avr_set_r(avr, R_XL, x);
						}	break;
						case 0x9009:
						case 0x900a: {	// LD Load Indirect from Data using Y 1001 000r rrrr 10oo
							int op = opcode & 3;
							uint8_t r = (opcode >> 4) & 0x1f;
							uint16_t y = (avr->data[R_YH] << 8) | avr->data[R_YL];
							STATE("ld %s, %sY[%04x]%s\n", avr_regname(r), op == 2 ? "--" : "", y, op == 1 ? "++" : "");
							cycle++;
							if (op == 2) y--;
							_avr_set_r(avr, r, _avr_get_ram(avr, y));
							if (op == 1) y++;
							_avr_set_r(avr, R_YH, y >> 8);
							_avr_set_r(avr, R_YL, y);
						}	break;
						case 0x9209:
						case 0x920a: {	// ST Store Indirect Data Space Y 1001 001r rrrr 10oo
							int op = opcode & 3;
							uint8_t r = (opcode >> 4) & 0x1f;
							uint16_t y = (avr->data[R_YH] << 8) | avr->data[R_YL];
							STATE("st %sY[%04x]%s, %s[%02x] \n", op == 2 ? "--" : "", y, op == 1 ? "++" : "", avr_regname(r), avr->data[r]);
							cycle++;
							if (op == 2) y--;
							_avr_set_ram(avr, y, avr->data[r]);
							if (op == 1) y++;
							_avr_set_r(avr, R_YH, y >> 8);
							_avr_set_r(avr, R_YL, y);
						}	break;
						case 0x9200: {	// STS ! Store Direct to Data Space, 32 bits
							uint8_t r = (opcode >> 4) & 0x1f;
							uint16_t x = (avr->flash[new_pc+1] << 8) | avr->flash[new_pc];
							new_pc += 2;
							STATE("sts 0x%04x, %s[%02x]\n", x, avr_regname(r), avr->data[r]);
							_avr_set_ram(avr, x, avr->data[r]);
						}	break;
						case 0x9001:
						case 0x9002: {	// LD Load Indirect from Data using Z 1001 001r rrrr 00oo
							int op = opcode & 3;
							uint8_t r = (opcode >> 4) & 0x1f;
							uint16_t z = (avr->data[R_ZH] << 8) | avr->data[R_ZL];
							STATE("ld %s, %sZ[%04x]%s\n", avr_regname(r), op == 2 ? "--" : "", z, op == 1 ? "++" : "");
							if (op == 2) z--;
							_avr_set_r(avr, r, _avr_get_ram(avr, z));
							if (op == 1) z++;
							_avr_set_r(avr, R_ZH, z >> 8);
							_avr_set_r(avr, R_ZL, z);
						}	break;
						case 0x9201:
						case 0x9202: {	// ST Store Indirect Data Space Z 1001 001r rrrr 00oo
							int op = opcode & 3;
							uint8_t r = (opcode >> 4) & 0x1f;
							uint16_t z = (avr->data[R_ZH] << 8) | avr->data[R_ZL];
							STATE("st %sZ[%04x]%s, %s[%02x] \n", op == 2 ? "--" : "", z, op == 1 ? "++" : "", avr_regname(r), avr->data[r]);
							if (op == 2) z--;
							_avr_set_ram(avr, z, avr->data[r]);
							if (op == 1) z++;
							_avr_set_r(avr, R_ZH, z >> 8);
							_avr_set_r(avr, R_ZL, z);
						}	break;
						case 0x900f: {	// POP 1001 000d dddd 1111
							uint8_t r = (opcode >> 4) & 0x1f;
							_avr_set_r(avr, r, _avr_pop8(avr));
							uint16_t sp = _avr_sp_get(avr);
							STATE("pop %s (@%04x)[%02x]\n", avr_regname(r), sp, avr->data[sp]);
							cycle++;
						}	break;
						case 0x920f: {	// PUSH 1001 001d dddd 1111
							uint8_t r = (opcode >> 4) & 0x1f;
							_avr_push8(avr, avr->data[r]);
							uint16_t sp = _avr_sp_get(avr);
							STATE("push %s[%02x] (@%04x)\n", avr_regname(r), avr->data[r], sp);
							cycle++;
						}	break;
						case 0x9400: {	// COM – One’s Complement
							uint8_t r = (opcode >> 4) & 0x1f;
							uint8_t res = 0xff - avr->data[r];
							STATE("com %s[%02x] = %02x\n", avr_regname(r), avr->data[r], res);
							_avr_set_r(avr, r, res);
							avr->sreg[S_Z] = res == 0;
							avr->sreg[S_N] = res >> 7;
							avr->sreg[S_V] = 0;
							avr->sreg[S_C] = 1;
							avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
							SREG();
						}	break;
						case 0x9401: {	// NEG – One’s Complement
							uint8_t r = (opcode >> 4) & 0x1f;
							uint8_t rd = avr->data[r];
							uint8_t res = 0x00 - rd;
							STATE("neg %s[%02x] = %02x\n", avr_regname(r), rd, res);
							_avr_set_r(avr, r, res);
							avr->sreg[S_H] = ((res >> 3) | (rd >> 3)) & 1;
							avr->sreg[S_Z] = res == 0;
							avr->sreg[S_N] = res >> 7;
							avr->sreg[S_V] = res == 0x80;
							avr->sreg[S_C] = res != 0;
							avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
							SREG();
						}	break;
						case 0x9402: {	// SWAP – Swap Nibbles
							uint8_t r = (opcode >> 4) & 0x1f;
							uint8_t res = (avr->data[r] >> 4) | (avr->data[r] << 4) ;
							STATE("swap %s[%02x] = %02x\n", avr_regname(r), avr->data[r], res);
							_avr_set_r(avr, r, res);
						}	break;
						case 0x9403: {	// INC – Increment
							uint8_t r = (opcode >> 4) & 0x1f;
							uint8_t res = avr->data[r] + 1;
							STATE("inc %s[%02x] = %02x\n", avr_regname(r), avr->data[r], res);
							_avr_set_r(avr, r, res);
							avr->sreg[S_Z] = res == 0;
							avr->sreg[S_N] = res >> 7;
							avr->sreg[S_V] = res == 0x7f;
							avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
							SREG();
						}	break;
						case 0x9405: {	// ASR – Arithmetic Shift Right 1001 010d dddd 0101
							uint8_t r = (opcode >> 4) & 0x1f;
							uint8_t vr = avr->data[r];
							uint8_t res = (vr >> 1) | (vr & 0x80);
							STATE("asr %s[%02x]\n", avr_regname(r), vr);
							_avr_set_r(avr, r, res);
							avr->sreg[S_Z] = res == 0;
							avr->sreg[S_C] = vr & 1;
							avr->sreg[S_N] = res >> 7;
							avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
							avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
							SREG();
						}	break;
						case 0x9406: {	// LSR 1001 010d dddd 0110
							uint8_t r = (opcode >> 4) & 0x1f;
							uint8_t vr = avr->data[r];
							uint8_t res = vr >> 1;
							STATE("lsr %s[%02x]\n", avr_regname(r), vr);
							_avr_set_r(avr, r, res);
							avr->sreg[S_Z] = res == 0;
							avr->sreg[S_C] = vr & 1;
							avr->sreg[S_N] = 0;
							avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
							avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
							SREG();
						}	break;
						case 0x9407: {	// ROR 1001 010d dddd 0111
							uint8_t r = (opcode >> 4) & 0x1f;
							uint8_t vr = avr->data[r];
							uint8_t res = (avr->sreg[S_C] ? 0x80 : 0) | vr >> 1;
							STATE("ror %s[%02x]\n", avr_regname(r), vr);
							_avr_set_r(avr, r, res);
							avr->sreg[S_Z] = res == 0;
							avr->sreg[S_C] = vr & 1;
							avr->sreg[S_N] = 0;
							avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
							avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
							SREG();
						}	break;
						case 0x940a: {	// DEC – Decrement
							uint8_t r = (opcode >> 4) & 0x1f;
							uint8_t res = avr->data[r] - 1;
							STATE("dec %s[%02x] = %02x\n", avr_regname(r), avr->data[r], res);
							_avr_set_r(avr, r, res);
							avr->sreg[S_Z] = res == 0;
							avr->sreg[S_N] = res >> 7;
							avr->sreg[S_V] = res == 0x80;
							avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
							SREG();
						}	break;
						case 0x940c:
						case 0x940d: {	// JMP Long Call to sub, 32 bits
							uint32_t a = ((opcode & 0x01f0) >> 3) | (opcode & 1);
							uint16_t x = (avr->flash[new_pc+1] << 8) | avr->flash[new_pc];
							a = (a << 16) | x;
							STATE("jmp 0x%06x\n", a);
							new_pc = a << 1;
							cycle += 2;
							TRACE_JUMP();
						}	break;
						case 0x940e:
						case 0x940f: {	// CALL Long Call to sub, 32 bits
							uint32_t a = ((opcode & 0x01f0) >> 3) | (opcode & 1);
							uint16_t x = (avr->flash[new_pc+1] << 8) | avr->flash[new_pc];
							a = (a << 16) | x;
							STATE("call 0x%06x\n", a);
							new_pc += 2;
							_avr_push16(avr, new_pc >> 1);
							new_pc = a << 1;
							cycle += 3;	// 4 cycles
							TRACE_JUMP();
							STACK_FRAME_PUSH();
						}	break;

						default: {
							switch (opcode & 0xff00) {
								case 0x9600: {	// ADIW - Add Immediate to Word 1001 0110 KKdd KKKK
									uint8_t r = 24 + ((opcode >> 3) & 0x6);
									uint8_t k = ((opcode & 0x00c0) >> 2) | (opcode & 0xf);
									uint8_t rdl = avr->data[r], rdh = avr->data[r+1];
									uint32_t res = rdl | (rdh << 8);
									STATE("adiw %s:%s[%04x], 0x%02x\n", avr_regname(r), avr_regname(r+1), res, k);
									res += k;
									_avr_set_r(avr, r + 1, res >> 8);
									_avr_set_r(avr, r, res);
									avr->sreg[S_V] = ~(rdh >> 7) & ((res >> 15) & 1);
									avr->sreg[S_Z] = (res & 0xffff) == 0;
									avr->sreg[S_N] = (res >> 15) & 1;
									avr->sreg[S_C] = ~((res >> 15) & 1) & (rdh >> 7);
									avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
									SREG();
									cycle++;
								}	break;
								case 0x9700: {	// SBIW - Subtract Immediate from Word 1001 0110 KKdd KKKK
									uint8_t r = 24 + ((opcode >> 3) & 0x6);
									uint8_t k = ((opcode & 0x00c0) >> 2) | (opcode & 0xf);
									uint8_t rdl = avr->data[r], rdh = avr->data[r+1];
									uint32_t res = rdl | (rdh << 8);
									STATE("sbiw %s:%s[%04x], 0x%02x\n", avr_regname(r), avr_regname(r+1), res, k);
									res -= k;
									_avr_set_r(avr, r + 1, res >> 8);
									_avr_set_r(avr, r, res);
									avr->sreg[S_V] = (rdh >> 7) & (~(res >> 15) & 1);
									avr->sreg[S_Z] = (res & 0xffff) == 0;
									avr->sreg[S_N] = (res >> 15) & 1;
									avr->sreg[S_C] = ((res >> 15) & 1) & (~rdh >> 7);
									avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
									SREG();
									cycle++;
								}	break;
								case 0x9800: {	// CBI - Clear Bit in I/O Registe 1001 1000 AAAA Abbb
									uint8_t io = ((opcode >> 3) & 0x1f) + 32;
									uint8_t b = opcode & 0x7;
									uint8_t res = _avr_get_ram(avr, io) & ~(1 << b);
									STATE("cbi %s[%04x], 0x%02x = %02x\n", avr_regname(io), avr->data[io], 1<<b, res);
									_avr_set_ram(avr, io, res);
									cycle++;
								}	break;
								case 0x9900: {	// SBIC - Skip if Bit in I/O Register is Cleared 1001 0111 AAAA Abbb
									uint8_t io = ((opcode >> 3) & 0x1f) + 32;
									uint8_t b = opcode & 0x7;
									uint8_t res = _avr_get_ram(avr, io) & (1 << b);
									STATE("sbic %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), avr->data[io], 1<<b, !res?"":"not ");
									if (!res) {
										if (_avr_is_instruction_32_bits(avr, new_pc)) {
											new_pc += 4; cycle += 2;
										} else {
											new_pc += 2; cycle++;
										}
									}
								}	break;
								case 0x9a00: {	// SBI - Set Bit in I/O Register 1001 1000 AAAA Abbb
									uint8_t io = ((opcode >> 3) & 0x1f) + 32;
									uint8_t b = opcode & 0x7;
									uint8_t res = _avr_get_ram(avr, io) | (1 << b);
									STATE("sbi %s[%04x], 0x%02x = %02x\n", avr_regname(io), avr->data[io], 1<<b, res);
									_avr_set_ram(avr, io, res);
									cycle++;
								}	break;
								case 0x9b00: {	// SBIS - Skip if Bit in I/O Register is Cleared 1001 0111 AAAA Abbb
									uint8_t io = (opcode >> 3) & 0x1f;
									uint8_t b = opcode & 0x7;
									uint8_t res = _avr_get_ram(avr, io + 32) & (1 << b);
									STATE("sbis %s[%04x], 0x%02x\t; Will%s branch\n", avr_regname(io), avr->data[io], 1<<b, res?"":"not ");
									if (res) {
										if (_avr_is_instruction_32_bits(avr, new_pc)) {
											new_pc += 4; cycle += 2;
										} else {
											new_pc += 2; cycle++;
										}
									}
								}	break;
								default:
									switch (opcode & 0xfc00) {
										case 0x9c00: {	// MUL - Multiply Unsigned 1001 11rd dddd rrrr
											get_r_d_10(opcode);
											uint16_t res = vd * vr;
											STATE("mul %s[%02x], %s[%02x] = %04x\n", avr_regname(d), vd, avr_regname(r), vr, res);
											_avr_set_r(avr, 0, res);
											_avr_set_r(avr, 1, res >> 8);
											avr->sreg[S_Z] = res == 0;
											avr->sreg[S_C] = (res >> 15) & 1;
											SREG();
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
					uint8_t r = (opcode >> 4) & 0x1f;
					uint8_t A = ((((opcode >> 9) & 3) << 4) | ((opcode) & 0xf)) + 32;
					STATE("out %s, %s[%02x]\n", avr_regname(A), avr_regname(r), avr->data[r]);
					// todo: store to IO register
					_avr_set_ram(avr, A, avr->data[r]);
				//	avr->data[A] = ;
				}	break;
				case 0xb000: {	// IN Rd,A 1011 0AAr rrrr AAAA
					uint8_t r = (opcode >> 4) & 0x1f;
					uint8_t A = ((((opcode >> 9) & 3) << 4) | ((opcode) & 0xf)) + 32;
					STATE("in %s, %s[%02x]\n", avr_regname(r), avr_regname(A), avr->data[A]);
					// todo: get the IO register
					_avr_set_r(avr, r, _avr_get_ram(avr, A));
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0xc000: {
			// RJMP 1100 kkkk kkkk kkkk
			short o = ((short)(opcode << 4)) >> 4;
			STATE("rjmp .%d [%04x]\n", o, new_pc + (o << 1));
			new_pc = new_pc + (o << 1);
			cycle++;
			TRACE_JUMP();
		}	break;

		case 0xd000: {
			// RCALL 1100 kkkk kkkk kkkk
			short o = ((short)(opcode << 4)) >> 4;
			STATE("rcall .%d [%04x]\n", o, new_pc + (o << 1));
			_avr_push16(avr, new_pc >> 1);
			new_pc = new_pc + (o << 1);
			cycle += 2;
			// 'rcall .1' is used as a cheap "push 16 bits of room on the stack"
			if (o != 0) {
				TRACE_JUMP();
				STACK_FRAME_PUSH();
			}
		}	break;

		case 0xe000: {	// LDI Rd, K 1110 KKKK RRRR KKKK -- aka SER (LDI r, 0xff)
			uint8_t d = 16 + ((opcode >> 4) & 0xf);
			uint8_t k = ((opcode & 0x0f00) >> 4) | (opcode & 0xf);
			STATE("ldi %s, 0x%02x\n", avr_regname(d), k);
			_avr_set_r(avr, d, k);
		}	break;

		case 0xf000: {
			switch (opcode & 0xfe00) {
				case 0xf000:
				case 0xf200:
				case 0xf400:
				case 0xf600: {	// All the SREG branches
					short o = ((short)(opcode << 6)) >> 9; // offset
					uint8_t s = opcode & 7;
					int set = (opcode & 0x0400) == 0;		// this bit means BRXC otherwise BRXS
					int branch = (avr->sreg[s] && set) || (!avr->sreg[s] && !set);
					const char *names[2][8] = {
							{ "brcc", "brne", "brpl", "brvc", NULL, "brhc", "brtc", "brid"},
							{ "brcs", "breq", "brmi", "brvs", NULL, "brhs", "brts", "brie"},
					};
					if (names[set][s]) {
						STATE("%s .%d [%04x]\t; Will%s branch\n", names[set][s], o, new_pc + (o << 1), branch ? "":" not");
					} else {
						STATE("%s%c .%d [%04x]\t; Will%s branch\n", set ? "brbs" : "brbc", _sreg_bit_name[s], o, new_pc + (o << 1), branch ? "":" not");
					}
					if (branch) {
						cycle++;
						new_pc = new_pc + (o << 1);
					}
				}	break;
				case 0xf800:
				case 0xf900: {	// BLD – Bit Store from T into a Bit in Register 1111 100r rrrr 0bbb
					uint8_t r = (opcode >> 4) & 0x1f; // register index
					uint8_t s = opcode & 7;
					uint8_t v = avr->data[r] | (avr->sreg[S_T] ? (1 << s) : 0);
					STATE("bld %s[%02x], 0x%02x = %02x\n", avr_regname(r), avr->data[r], 1 << s, v);
					_avr_set_r(avr, r, v);
				}	break;
				case 0xfa00:
				case 0xfb00:{	// BST – Bit Store into T from bit in Register 1111 100r rrrr 0bbb
					uint8_t r = (opcode >> 4) & 0x1f; // register index
					uint8_t s = opcode & 7;
					STATE("bst %s[%02x], 0x%02x\n", avr_regname(r), avr->data[r], 1 << s);
					avr->sreg[S_T] = (avr->data[r] >> s) & 1;
					SREG();
				}	break;
				case 0xfc00:
				case 0xfe00: {	// SBRS/SBRC – Skip if Bit in Register is Set/Clear 1111 11sr rrrr 0bbb
					uint8_t r = (opcode >> 4) & 0x1f; // register index
					uint8_t s = opcode & 7;
					int set = (opcode & 0x0200) != 0;
					int branch = ((avr->data[r] & (1 << s)) && set) || (!(avr->data[r] & (1 << s)) && !set);
					STATE("%s %s[%02x], 0x%02x\t; Will%s branch\n", set ? "sbrs" : "sbrc", avr_regname(r), avr->data[r], 1 << s, branch ? "":" not");
					if (branch)
						new_pc = new_pc + 2;
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		default: _avr_invalid_opcode(avr);

	}
	avr->cycle += cycle;
	return new_pc;
}


