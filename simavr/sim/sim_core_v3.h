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
static inline void uFlashWrite(avr_t* avr, avr_flashaddr_t addr, uint32_t data) {
	if(addr>((avr->flashend + 1) << 1)) {
		printf("[uFlashWrite] address out of bounds");
		return;
	}

	avr->uflash[(addr>>1)]=data;
}

static inline uint32_t uFlashRead(avr_t* avr, avr_flashaddr_t addr) {
	if(addr>((avr->flashend + 1) << 1)) {
		printf("[uFlashWrite] address out of bounds");

	}
	return(avr->uflash[(addr>>1)]);
}
#else
static inline void uFlashWrite(avr_t* avr, avr_flashaddr_t addr, uint32_t data) {
}
static inline uint32_t uFlashRead(avr_t* avr, avr_flashaddr_t addr) {
	return(0);
}
#endif


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
	k_avr_uinst_d5x16_lds,
	k_avr_uinst_o12_rcall,
	k_avr_uinst_o12_rjmp,
	k_avr_uinst_d5x16_sts,
	k_avr_uinst_d5r5_add=0x80,
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
	k_avr_uinst_d5r5_cpc,
	k_avr_uinst_h4k8_cpi,
	k_avr_uinst_d5r5_cpse,
	k_avr_uinst_d5r5_eor,
	k_avr_uinst_d5a6_in,
	k_avr_uinst_d5rXYZop_ld,
	k_avr_uinst_d5rXYZq6_ldd,
	k_avr_uinst_h4k8_ldi,
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

