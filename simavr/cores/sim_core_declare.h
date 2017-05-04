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

/* we have to declare this, as none of the distro but debian has a modern
 * toolchain and avr-libc. This affects a lot of names, like MCUSR etc
 */
#define __AVR_LIBC_DEPRECATED_ENABLE__

/*
 * The macros "fake" the ones in the real avrlib
 */
#define _SFR_IO8(v) ((v)+32)
#define _SFR_IO16(v) ((v)+32)
#define _SFR_MEM8(v) (v)
#define _BV(v) (v)
#define _VECTOR(v) (v)

/*
 * This declares a typical AVR core, using constants what appears
 * to be in every io*.h file...
 */
#if FUSE_MEMORY_SIZE == 6
# ifndef FUSE0_DEFAULT
#  define FUSE0_DEFAULT 0xFF
# endif
# ifndef FUSE1_DEFAULT
#  define FUSE1_DEFAULT 0xFF
# endif
# ifndef FUSE2_DEFAULT
#  define FUSE2_DEFAULT 0xFF
# endif
# ifndef FUSE3_DEFAULT
#  define FUSE3_DEFAULT 0xFF
# endif
# ifndef FUSE4_DEFAULT
#  define FUSE4_DEFAULT 0xFF
# endif
# ifndef FUSE5_DEFAULT
#  define FUSE5_DEFAULT 0xFF
# endif

# define _FUSE_HELPER { FUSE1_DEFAULT, FUSE1_DEFAULT, FUSE2_DEFAULT, \
	FUSE3_DEFAULT, FUSE4_DEFAULT, FUSE5_DEFAULT }
#elif FUSE_MEMORY_SIZE == 3
# define _FUSE_HELPER { LFUSE_DEFAULT, HFUSE_DEFAULT, EFUSE_DEFAULT }
#elif FUSE_MEMORY_SIZE == 2
# define _FUSE_HELPER { LFUSE_DEFAULT, HFUSE_DEFAULT }
#elif FUSE_MEMORY_SIZE == 1
# define _FUSE_HELPER { FUSE_DEFAULT }
#else
# define _FUSE_HELPER { 0 }
#endif

#ifdef MCUSR
# define MCU_STATUS_REG MCUSR
#else
# define MCU_STATUS_REG MCUCSR
#endif

#ifdef SIGNATURE_0
#define DEFAULT_CORE(_vector_size) \
	.ioend  = RAMSTART - 1, \
	.ramend = RAMEND, \
	.flashend = FLASHEND, \
	.e2end = E2END, \
	.vector_size = _vector_size, \
	.fuse = _FUSE_HELPER, \
	.signature = { SIGNATURE_0,SIGNATURE_1,SIGNATURE_2 }, \
	.lockbits = 0xFF, \
	.reset_flags = {\
		.porf = AVR_IO_REGBIT(MCU_STATUS_REG, PORF),\
		.extrf = AVR_IO_REGBIT(MCU_STATUS_REG, EXTRF),\
		.borf = AVR_IO_REGBIT(MCU_STATUS_REG, BORF),\
		.wdrf = AVR_IO_REGBIT(MCU_STATUS_REG, WDRF)\
	}
#else
// Disable signature when using an old avr toolchain
#define DEFAULT_CORE(_vector_size) \
	.ioend  = RAMSTART - 1, \
	.ramend = RAMEND, \
	.flashend = FLASHEND, \
	.e2end = E2END, \
	.vector_size = _vector_size
#endif
#endif /* __SIM_CORE_DECLARE_H__ */
