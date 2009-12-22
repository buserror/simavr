/*
	sim_core_declare.h

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
#ifndef __SIM_CORE_DECLARE_H__
#define __SIM_CORE_DECLARE_H__

/*
 * The macros "fake" the ones in the real avrlib
 */
#define _SFR_IO8(v) ((v)+32)
#define _SFR_MEM8(v) (v)
#define _BV(v) (v)
#define _VECTOR(v) (v)

/*
 * This declares a typical AVR core, using constants what appears
 * to be in every io*.h file...
 */
#ifdef SIGNATURE_0
#define DEFAULT_CORE(_vector_size) \
	.ramend = RAMEND, \
	.flashend = FLASHEND, \
	.e2end = E2END, \
	.vector_size = _vector_size, \
	.fuse = { LFUSE_DEFAULT, HFUSE_DEFAULT, EFUSE_DEFAULT }, \
	.signature = { SIGNATURE_0,SIGNATURE_1,SIGNATURE_2 }
#else
// Disable signature for now, for ubuntu, gentoo and other using old avr toolchain
#define DEFAULT_CORE(_vector_size) \
	.ramend = RAMEND, \
	.flashend = FLASHEND, \
	.e2end = E2END, \
	.vector_size = _vector_size
#endif
#endif /* __SIM_CORE_DECLARE_H__ */
