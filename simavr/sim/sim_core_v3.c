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

#if CONFIG_SIMAVR_CORE_V3

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

#include "sim_core_v2_flag_ops.h"
#include "sim_core_v3_inst_ops.h"

avr_flashaddr_t avr_run_one_v3(avr_t* avr) {
	avr_flashaddr_t		new_pc = avr->pc + 2;
	int			cycle = 1;

	uint32_t		opcode = uFlashRead(avr, avr->pc);
	uint8_t			soc = OPCODEop(opcode);
	uint8_t			r1 = OPCODEr1(opcode);

	new_pc = avr->pc + 2;

//	printf("B-0x%04x: opcode: 0x%02x [0x%04x]\n", avr->pc, soc, OPCODEop(uFlashRead(avr, avr->pc)));

	if(0x80<=soc) {
		uint8_t r2 = OPCODEr2(opcode);
		switch(soc) {
			case	k_avr_uinst_d5r5_add:
				_avr_uinst_d5r5_add(avr, &new_pc, &cycle, r1, r2);
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
					case	k_avr_uinst_d5rXYZop_ld:
						_avr_uinst_d5rXYZop_ld(avr, &new_pc, &cycle, r1, r2, r3);
						break;
					case	k_avr_uinst_d5rXYZq6_ldd:
						_avr_uinst_d5rXYZq6_ldd(avr, &new_pc, &cycle, r1, r2, r3);
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

notFound: /* fall back to core version 2, we'll (most likely) get it on the next run. */
	return(avr_run_one_v2(avr));
}

#endif

