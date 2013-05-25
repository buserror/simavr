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

#ifndef SIM_CORE_V2_FLAG_OPS_H
#define SIM_CORE_V2_FLAG_OPS_H

inline void _avr_flags_zc16(avr_t* avr, const uint16_t res) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = (res >> 15) & 1;
}

inline void _avr_flags_zcn0vs(avr_t* avr, const uint8_t res, const uint8_t vr) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vr & 1;
	avr->sreg[S_N] = 0;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

inline void _avr_flags_zns(avr_t* avr, const uint8_t res) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

inline void _avr_flags_Zns(avr_t* avr, const uint8_t res) {
	if (res)
		avr->sreg[S_Z] = 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

inline void _avr_flags_zns16(avr_t* avr, const uint16_t res) {
	avr->sreg[S_Z] = (res & 0xffff) == 0;
	avr->sreg[S_N] = (res >> 15) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

inline void _avr_flags_znv0s(avr_t* avr, const uint8_t res) {
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_V] = 0;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static inline void _avr_flags_add_carry(avr_t* avr, const uint8_t res, const uint8_t rd, const uint8_t rr) {
	uint8_t result = ((rd & rr) | (rr & ~res) | (~res & rd));

	avr->sreg[S_H] = ((result & 0x04) >> 3);
	avr->sreg[S_C] = ((result & 0x80) >> 7);
}

static inline void _avr_flags_add_overflow(avr_t* avr, const uint8_t res, const uint8_t rd, const uint8_t rr) {
	uint8_t result = (rd & rr & res) | (~rd & ~rr & res);

	avr->sreg[S_V] = (result & 0x80) >> 7;
}

static inline uint8_t _get_sub_carry(const uint8_t res, const uint8_t rd, const uint8_t rr) {
	return((~rd & rr) | (rr & res) | (res & ~rd));
}

static inline void _avr_flags_sub_carry(avr_t* avr, const uint8_t res, const uint8_t rd, const uint8_t rr) {
	uint8_t result = _get_sub_carry(res, rd, rr);

	avr->sreg[S_H] = ((result & 0x04) >> 3);
	avr->sreg[S_C] = ((result & 0x80) >> 7);
}

static inline uint8_t _get_sub_overflow(const uint8_t res, const uint8_t rd, const uint8_t rr) {
	return((rd & ~rr & ~res) | (~rd & rr & res));
}
static inline void _avr_flags_sub_overflow(avr_t* avr, const uint8_t res, const uint8_t rd, const uint8_t rr) {
	uint8_t result = _get_sub_overflow(res, rd, rr);

	avr->sreg[S_V] = (result & 0x80) >> 7;
}
#endif

