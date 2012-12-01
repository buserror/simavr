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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "sim_avr.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "avr_flash.h"
#include "avr_watchdog.h"

#include "sim_core_v2.h"
#include "sim_core_v3.h"

/*
 * Called when an invalid opcode is decoded
 */
static inline void _avr_invalid_opcode(avr_t * avr)
{
#if CONFIG_SIMAVR_TRACE
	printf( FONT_RED "*** %04x: %-25s Invalid Opcode SP=%04x O=%04x \n" FONT_DEFAULT,
			avr->pc, avr->trace_data->codeline[avr->pc>>1]->symbol, _avr_sp_get(avr), _avr_flash_read16le(avr, avr->pc));
//			avr->pc, avr->trace_data->codeline[avr->pc>>1]->symbol, _avr_sp_get(avr), avr->flash[avr->pc] | (avr->flash[avr->pc+1]<<8));
#else
	AVR_LOG(avr, LOG_ERROR, FONT_RED "CORE: *** %04x: Invalid Opcode SP=%04x O=%04x \n" FONT_DEFAULT,
			avr->pc, _avr_sp_get(avr), _avr_flash_read16le(avr, avr->pc));
//			avr->pc, _avr_sp_get(avr), avr->flash[avr->pc] | (avr->flash[avr->pc+1]<<8));
#endif
}


#define get_r_d_10(o) \
		const uint8_t r = ((o >> 5) & 0x10) | (o & 0xf); \
		const uint8_t d = (o >> 4) & 0x1f;\
		const uint8_t vd = avr->data[d], vr = avr->data[r];
#define get_r_dd_10(o) \
		const uint8_t r = ((o >> 5) & 0x10) | (o & 0xf); \
		const uint8_t d = (o >> 4) & 0x1f;\
		const uint8_t vr = avr->data[r];
#define get_k_r16(o) \
		const uint8_t r = 16 + ((o >> 4) & 0xf); \
		const uint8_t k = ((o & 0x0f00) >> 4) | (o & 0xf);

#include "sim_core_v2_flag_ops.h"

#include "sim_core_v3_inst_ops.h"

#include "sim_core_v2_inst_ops.h"

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
avr_flashaddr_t avr_run_one_v2(avr_t* avr)
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

//	uint32_t		opcode = (avr->flash[avr->pc + 1] << 8) | avr->flash[avr->pc];
	uint16_t		opcode = _avr_flash_read16le(avr, avr->pc);

#if CONFIG_SIMAVR_CORE_V3
	if(OPCODEop(uFlashRead(avr, avr->pc))) {
		printf("A-0x%04x: opcode: 0x%04x, 0x%04x, 0x%04x, 0x%04x [0x%02x]\n", 
			avr->pc, opcode, opcode&0xf000, opcode&0xfc00, opcode&0xfe0f, OPCODEop(uFlashRead(avr, avr->pc)));
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
							_avr_inst_d5r5_add(avr, &new_pc, &cycle, opcode);
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
					_avr_inst_d5r5_cp(avr, &new_pc, &cycle, opcode);
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
			_avr_inst_h4k8_cpi(avr, &new_pc, &cycle, opcode);
		}	break;

		case 0x4000: {	// SBCI Subtract Immediate With Carry 0101 10 kkkk dddd kkkk
			_avr_inst_h4k8_sbci(avr, &new_pc, &cycle, opcode);
		}	break;

		case 0x5000: {	// SUB Subtract Immediate 0101 10 kkkk dddd kkkk
			_avr_inst_h4k8_subi(avr, &new_pc, &cycle, opcode);
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
				case 0x95c8: {	// LPM Load Program Memory R0 <- (Z)
//					uint16_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8);
					uint16_t z = _avr_data_read16le(avr, R_ZL);
					STATE("lpm %s, (Z[%04x])\n", avr_regname(0), z);
					cycle += 2; // 3 cycles
//					_avr_set_r(avr, 0, avr->flash[z]);
					_avr_data_write(avr, 0, avr->flash[z]);
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
							_avr_inst_d5_lpm_z0(avr, &new_pc, &cycle, opcode);
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
					_avr_inst_d5a6_in(avr, &new_pc, &cycle, opcode);
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
			_avr_inst_h4k8_ldi(avr, &new_pc, &cycle, opcode);
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


