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

static inline int16_t _OP_DECODE_o12(uint16_t o) {
//	return(((int16_t)((o << 4)&0xffff)) >> 4);
	return((int16_t)((o & 0x0fff) << 4) >> 3);
}		

static inline uint8_t _OP_DECODE_a6(uint16_t o) {
	return(32+(((o & 0x0600) >> 5)|_OP_DECODE_r4(o)));
}

#define OPCODE(opcode, r1, r2) ((r2<<16)|(r1<<8)|(opcode))
#define OPCODE3(opcode, r1, r2, r3) ((r3<<24)|(r2<<16)|(r1<<8)|(opcode))

#define __INST(name) \
	static inline void _avr_inst##name(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle, uint16_t opcode)

#define __INSTarg(name, args...) \
	static inline void _avr_inst##name(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle, uint16_t opcode, ##args)

#define INST(name) __INST(name) { \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst##name, 0, 0)); \
		_avr_uinst##name(avr, new_pc, cycle); \
	}

#define INSTb3(name) __INST(_b3##name) { \
		const uint8_t b3 = _OP_DECODE_d4(opcode) & 0x7; \
 \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_b3##name, b3, 0)); \
		_avr_uinst_b3##name(avr, new_pc, cycle, b3); \
	}

#define INSTbIO(name) __INST(_bIO##name) { \
		const uint8_t a = 32 + ((opcode & 0x00f8) >> 3); \
		const uint8_t b = (opcode & 0x0007); \
 \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_bIO##name, a, b)); \
		_avr_uinst_bIO##name(avr, new_pc, cycle, a, b); \
	}


#define INSTd4r4(name) __INST(_d4r4##name) {\
		const uint8_t d4 = _OP_DECODE_d4(opcode) << 1; \
		const uint8_t r4 = _OP_DECODE_r4(opcode) << 1; \
 \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d4r4##name, d4, r4)); \
		_avr_uinst_d4r4##name(avr, new_pc, cycle, d4, r4); \
	}

#define INSTd5(name) __INST(_d5##name) { \
	const uint8_t d5 = _OP_DECODE_d5(opcode); \
 \
	uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5##name, d5, 0)); \
	_avr_uinst_d5##name(avr, new_pc, cycle, d5); \
}

#define INSTd5a6(name) __INST(_d5a6##name) { \
	const uint8_t d5 = _OP_DECODE_d5(opcode); \
	const uint8_t a6 = _OP_DECODE_a6(opcode); \
 \
	uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5a6##name, d5, a6)); \
	_avr_uinst_d5a6##name(avr, new_pc, cycle, d5, a6); \
}

#define INSTd5b3(name) __INST(_d5b3##name) {\
	const uint8_t d5 = _OP_DECODE_d5(opcode); \
	const uint8_t b3 = _OP_DECODE_b3(opcode); \
 \
	uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5b3##name, d5, b3)); \
	_avr_uinst_d5b3##name(avr, new_pc, cycle, d5, b3); \
}

#define INSTd5r5(name) __INST(_d5r5##name) { \
	const uint8_t d5 = _OP_DECODE_d5(opcode); \
	const uint8_t r5 = _OP_DECODE_r5(opcode); \
 \
	uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5r5##name, d5, r5)); \
	_avr_uinst_d5r5##name(avr, new_pc, cycle, d5, r5); \
}

#define INSTd5rXYZq6(name) __INSTarg(_d5rXYZq6##name, uint8_t r) { \
		const uint8_t d5 = _OP_DECODE_d5(opcode); \
		const uint8_t q = ((opcode & 0x2000) >> 8) | ((opcode & 0x0c00) >> 7) | (opcode & 0x7); \
 \
		uFlashWrite(avr, avr->pc, OPCODE3(k_avr_uinst_d5rXYZq6##name, d5, r, q)); \
		_avr_uinst_d5rXYZq6##name(avr, new_pc, cycle, d5, r, q); \
	}

#define INSTd5rXYZ(name) __INSTarg(_d5rXYZ##name, uint8_t r) { \
		const uint8_t d5 = _OP_DECODE_d5(opcode); \
		const uint8_t opr = opcode & 0x003; \
 \
		uFlashWrite(avr, avr->pc, OPCODE3(k_avr_uinst_d5rXYZop##name, d5, r, opr)); \
		_avr_uinst_d5rXYZop##name(avr, new_pc, cycle, d5, r, opr); \
	}

#define INSTd16r16(name) __INST(_d16r16##name) {\
		const uint8_t d16 = _OP_DECODE_d16(opcode); \
		const uint8_t r16 = _OP_DECODE_r16(opcode); \
 \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d16r16##name, d16, r16)); \
		_avr_uinst_d16r16##name(avr, new_pc, cycle, d16, r16); \
	}

#define INSTd5x16(name) __INST(_d5x16##name) { \
		const uint8_t d5 = _OP_DECODE_d5(opcode); \
		const uint16_t x16 = _avr_flash_read16le(avr, *new_pc); \
 \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_d5x16##name, d5, x16)); \
		_avr_uinst_d5x16##name(avr, new_pc, cycle, d5, x16); \
	}

#define INSTh4k8(name) __INST(_h4k8##name) { \
		const uint8_t	h4=_OP_DECODE_h4(opcode); \
		const uint8_t	k8=_OP_DECODE_k8(opcode); \
 \
		uFlashWrite(avr, avr->pc,OPCODE(k_avr_uinst_h4k8##name, h4, k8)); \
		_avr_uinst_h4k8##name(avr, new_pc, cycle, h4, k8); \
	}

#define INSTo7b3(name) __INST(_o7b3##name) {\
		const uint8_t o7 = _OP_DECODE_o7(opcode); \
		const uint8_t b3 = _OP_DECODE_b3(opcode); \
 \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_o7b3##name, o7, b3)); \
		_avr_uinst_o7b3##name(avr, new_pc, cycle, o7, b3); \
	}

#define INSTo12(name) __INST(_o12##name) {\
		const int16_t o12 = _OP_DECODE_o12(opcode); \
 \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_o12##name, 0, o12)); \
		_avr_uinst_o12##name(avr, new_pc, cycle, o12); \
	}

#define INSTp2k6(name) __INST(_p2k6##name) { \
		const uint8_t p2 = _OP_DECODE_p2(opcode); \
		const uint8_t k6 = _OP_DECODE_k6(opcode); \
 \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_p2k6##name, p2, k6)); \
		_avr_uinst_p2k6##name(avr, new_pc, cycle, p2, k6); \
	}

#define INSTx24(name) __INST(_x24##name) { \
		const uint8_t x6 = ((_OP_DECODE_d5(opcode) << 1) | (opcode & 0x0001)); \
		const uint16_t x16 = _avr_flash_read16le(avr, *new_pc); \
 \
		uFlashWrite(avr, avr->pc, OPCODE(k_avr_uinst_x24##name, x6, x16)); \
		_avr_uinst_x24##name(avr, new_pc, cycle, x6, x16); \
	}

INSTd5r5(_add)

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

INSTd5r5(_cp)
INSTd5r5(_cpc)

INSTh4k8(_cpi)

INSTd5r5(_cpse)

INSTd5(_dec)

INSTd5r5(_eor)

INSTd5a6(_in)

INSTd5(_inc)

INSTx24(_jmp)

INSTd5rXYZ(_ld)
INSTd5rXYZq6(_ldd)

INSTh4k8(_ldi)

INSTd5x16(_lds)

INSTd5(_lpm_z0)
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

INSTh4k8(_subi)

INSTd5(_swap)

