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

#define UINST(name) \
	static inline void _avr_uinst##name(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle)

#define UINSTarg(name, args...) \
	static inline void _avr_uinst_##name(avr_t* avr, avr_flashaddr_t* new_pc, int* cycle, ##args)

#define UINSTb3(name) \
	UINSTarg(b3##name, const uint8_t b)

#define UINSTd5(name) \
	UINSTarg(d5##name, const uint8_t d)

#if 0
#define UINSTbIO(name)  \
	UINSTarg(bIO##name, const uint8_t a, const uint8_t b)
#endif

#define UINSTbIO(name)  \
	UINSTarg(bIO##name, const uint8_t io, const uint8_t b)

#define UINSTd5a6(name) \
	UINSTarg(d5a6##name, const uint8_t d, const uint8_t a)

#define UINSTd5b3(name) \
	UINSTarg(d5b3##name, const uint8_t d, const uint8_t b)

#define UINSTd4r4(name) \
	UINSTarg(d4r4##name, const uint8_t d, const uint8_t r)

#define UINSTd5r5(name) \
	UINSTarg(d5r5##name, const uint8_t d, const uint8_t r)


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

	avr->sreg[S_H] = get_add_carry(res, vd, vr, 3);
	avr->sreg[S_V] = get_add_overflow(res, vd, vr);
	avr->sreg[S_C] = get_add_carry(res, vd, vr, 7);

	_avr_flags_zns(avr, res);

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

	avr->sreg[S_H] = get_add_carry(res, vd, vr, 3);
	avr->sreg[S_V] = get_add_overflow(res, vd, vr);
	avr->sreg[S_C] = get_add_carry(res, vd, vr, 7);

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
	STATE("bset %c\n", _sreg_bit_name[b]);
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

	avr->sreg[S_H] = get_compare_carry(res, vd, vr, 3);
	avr->sreg[S_V] = get_compare_overflow(res, vd, vr);
	avr->sreg[S_C] = get_compare_carry(res, vd, vr, 7);

	_avr_flags_zns(avr, res);

	SREG();
}

UINSTd5r5(_cpc) {
	uint8_t vd = _avr_get_r(avr, d);
	uint8_t vr = _avr_get_r(avr, r);

	uint8_t res = vd - vr - avr->sreg[S_C];

	STATE("cpc %s[%02x], %s[%02x] = %02x\n", avr_regname(d), vd, avr_regname(r), vr, res);

	avr->sreg[S_H] = get_compare_carry(res, vd, vr, 3);
	avr->sreg[S_V] = get_compare_overflow(res, vd, vr);
	avr->sreg[S_C] = get_compare_carry(res, vd, vr, 7);

	_avr_flags_Zns(avr, res);

	SREG();
}

UINSTh4k8(_cpi) {
	uint8_t vh = _avr_get_r(avr, h);

	uint8_t res = vh - k;

	STATE("cpi %s[%02x], 0x%02x\n", avr_regname(r), vh, k);

	avr->sreg[S_H] = get_compare_carry(res, vh, k, 3);
	avr->sreg[S_V] = get_compare_overflow(res, vh, k);
	avr->sreg[S_C] = get_compare_carry(res, vh, k, 7);

	_avr_flags_zns(avr, res);

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

	STATE("ld %s, %sX[%04x]%s\n", avr_regname(d), op == 2 ? "--" : "", x, op == 1 ? "++" : "");

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
	uint16_t vr;
	_avr_set_r16(avr, d, (vr = _avr_get_r16(avr, r)));

	STATE("movw %s:%s, %s:%s[%04x]\n", avr_regname(d), avr_regname(d+1), avr_regname(r), avr_regname(r+1), vr);
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
	STATE("push %s[%02x] (@%04x)\n", avr_regname(r), vd, sp);

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
	avr->sreg[S_I] = 1;
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

	STATE("ror %s[%02x]\n", avr_regname(r), vd);

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

	avr->sreg[S_H] = get_sub_carry(res, vd, vr, 3);
	avr->sreg[S_V] = get_sub_overflow(res, vd, vr);
	avr->sreg[S_C] = get_sub_carry(res, vd, vr, 7);

	_avr_flags_Zns(avr, res);

	SREG();
}

UINSTbIO(_sbi) {
	uint8_t vio =_avr_get_ram(avr, io);
	uint8_t res = vio | (1 << b);

	STATE("cbi %s[%04x], 0x%02x = %02x\n", avr_regname(io), vio, 1<<b, res);

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

	STATE("sbci %s[%02x], 0x%02x = %02x\n", avr_regname(r), vr, k, res);

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

	STATE("st %sZ[%04x]%s, %s[%02x] \n", op == 2 ? "--" : "", vr, op == 1 ? "++" : "", avr_regname(d), vd);

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

	avr->sreg[S_H] = get_sub_carry(res, vd, vr, 3);
	avr->sreg[S_V] = get_sub_overflow(res, vd, vr);
	avr->sreg[S_C] = get_sub_carry(res, vd, vr, 7);

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

UINSTd5(_swap) {
	uint8_t vd = _avr_get_r(avr, d);
	uint8_t res = ((vd & 0xf0) >> 4) | ((vd & 0x0f) << 4);

	STATE("swap %s[%02x] = %02x\n", avr_regname(r), vd, res);

	_avr_set_r(avr, d, res);
}
