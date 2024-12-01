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
#include "sim_avr.h"
#include "sim_core.h"
#include "sim_gdb.h"
#include "avr_flash.h"
#include "avr_watchdog.h"

// SREG bit names
const char * _sreg_bit_name = "cznvshti";

/*
 * Handle "touching" registers, marking them changed.
 * This is used only for debugging purposes to be able to
 * print the effects of each instructions on registers
 */
#if CONFIG_SIMAVR_TRACE

#define T(w) w

#define REG_TOUCH(a, r) (a)->trace_data->touched[(r) >> 5] |= (1 << ((r) & 0x1f))
#define REG_ISTOUCHED(a, r) ((a)->trace_data->touched[(r) >> 5] & (1 << ((r) & 0x1f)))

//#define RESTRICT_TRACE
#ifdef RESTRICT_TRACE
/*
 * This allows a "special case" to skip instruction tracing when in these
 * symbols since printf() is useful to have, but generates a lot of cycles.
 */
int dont_trace(const char * name)
{
	return (
		!strncmp(name, "uart_putchar", 12) ||
		!strcmp(name, "fputc") ||
		!strcmp(name, "puts") ||
		!strcmp(name, "printf") ||
		!strcmp(name, "vfprintf") ||
		!strcmp(name, "__ultoa_invert") ||
		!strcmp(name, "__prologue_saves__") ||
		!strcmp(name, "__epilogue_restores__"));
}
#endif

/* Get symbol or line number for a flash addess.
 * Returns NULL only if in a tracing-restricted function.
 * Show registed values when restriction changes.
 */

static int donttrace;

static const char *where(avr_t *avr)
{
	avr_flashaddr_t  pc;
	const char      *s;

	pc = avr->pc >> 1; // Words
	if (avr->trace_data->codeline &&
		pc < avr->trace_data->codeline_size) {
		s = avr->trace_data->codeline[pc];
#ifdef RESTRICT_TRACE
		int	dont = dont_trace(s);
		if (dont) {
			if (!donttrace) {
				printf("\nCalling restricted function %s\n", s);
				DUMP_REG();
			}
		} else if (donttrace) {
			DUMP_REG();
		}
		donttrace = dont;
		if (donttrace)
			return NULL;
#endif
		if (s)
			return s;
	}
	return "";
}

#define STATE(_f, argsf ...)	if (avr->trace) {				\
	const char *symn = where(avr);							\
	if (symn)												\
		printf("%04x: %-25s " _f, avr->pc, symn, ## argsf);	\
}

#define SREG() if (avr->trace && donttrace == 0) {	  \
	printf("%04x: \t\t\t\t\t\t\t\tSREG = ", avr->pc); \
	for (int _sbi = 0; _sbi < 8; _sbi++)\
		printf("%c", avr->sreg[_sbi] ? toupper(_sreg_bit_name[_sbi]) : '.');\
	printf("\n");\
}

static const char *get_data_address_string(avr_t* avr, uint16_t addr)
{
	if (addr <= avr->ioend) {
		return avr_regname(avr, addr);
	} else if (addr < avr->trace_data->data_names_size) {
		return avr->data_names[addr];
	} else if (addr > _avr_sp_get(avr)) {
		return "[Stack]";
	}
	return "[Heap]";
}

#define DAS(addr) get_data_address_string(avr, addr)
#define FAS(addr) (((addr >> 1) > avr->trace_data->codeline_size) ? \
                   "[not loaded]" : avr->trace_data->codeline[addr >> 1])

void crash(avr_t* avr)
{
	DUMP_REG();
	printf("*** CYCLE %" PRI_avr_cycle_count " PC %04x\n", avr->cycle, avr->pc);

	for (int i = OLD_PC_SIZE-1; i > 0; i--) {
		int pci = (avr->trace_data->old_pci + i) & 0xf;
		printf(FONT_RED "*** %04x: %-25s RESET -%d; sp %04x\n" FONT_DEFAULT,
                       avr->trace_data->old[pci].pc,
                       avr->trace_data->codeline ?
                           avr->trace_data->codeline[avr->trace_data->old[pci].pc>>1] :
                           "unknown",
                       OLD_PC_SIZE-i,
                       avr->trace_data->old[pci].sp);
	}

	printf("Stack Ptr %04x/%04x = %d \n", _avr_sp_get(avr), avr->ramend, avr->ramend - _avr_sp_get(avr));
	DUMP_STACK();

	avr_sadly_crashed(avr, 0);
}
#else
#define T(w)
#define REG_TOUCH(a, r)
#define STATE(_f, args...)
#define SREG()

void crash(avr_t* avr)
{
	avr_sadly_crashed(avr, 0);

}
#endif

static inline uint16_t
_avr_flash_read16le(
	avr_t * avr,
	avr_flashaddr_t addr)
{
	return(avr->flash[addr] | (avr->flash[addr + 1] << 8));
}

static inline void _call_register_irqs(avr_t * avr, uint16_t addr)
{
	if (addr > 31 && addr < 31 + MAX_IOs) {
		avr_io_addr_t io = AVR_DATA_TO_IO(addr);

		if (avr->io[io].irq) {
			uint8_t v = avr->data[addr];
			avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
			for (int i = 0; i < 8; i++)
				avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);
		}
	}
}

void _call_sram_irqs(avr_t *avr, uint16_t addr) {
	for(int tracepoint=0; tracepoint < avr->sram_tracepoint_count; tracepoint++) {

		if (avr->sram_tracepoint[tracepoint].width == 16) {
			// 16 bits trace (LSB/addr or MSB/addr+1 may be accessed)
			if (avr->sram_tracepoint[tracepoint].addr == addr) {
				uint16_t v = (avr->data[addr+1] << 8) | avr->data[addr]; // LSB
				avr_raise_irq(avr->sram_tracepoint[tracepoint].irq, v);
			} else if (avr->sram_tracepoint[tracepoint].addr == addr - 1) {
				uint16_t v = (avr->data[addr] << 8) | avr->data[addr-1]; // MSB
				avr_raise_irq(avr->sram_tracepoint[tracepoint].irq, v);
			}
		} else {
			// 8 bits trace
			if (avr->sram_tracepoint[tracepoint].addr == addr) {
				uint8_t v = avr->data[addr];
				avr_raise_irq(avr->sram_tracepoint[tracepoint].irq, v);
			}
		}
	}
}

void avr_core_watch_write(avr_t *avr, uint16_t addr, uint8_t v)
{
	if (addr > avr->ramend) {
		AVR_LOG(avr, LOG_WARNING,
				"CORE: *** Wrapping write address "
				"PC=%04x SP=%04x O=%04x v=%02x Address %04x %% %04x --> %04x\n",
				avr->pc, _avr_sp_get(avr), _avr_flash_read16le(avr, avr->pc), v, addr, (avr->ramend + 1), addr % (avr->ramend + 1));
		addr = addr % (avr->ramend + 1);
	}
	if (addr < 32) {
		AVR_LOG(avr, LOG_ERROR, FONT_RED
				"CORE: *** Invalid write address PC=%04x SP=%04x O=%04x Address %04x=%02x low registers\n"
				FONT_DEFAULT,
				avr->pc, _avr_sp_get(avr), _avr_flash_read16le(avr, avr->pc), addr, v);
		crash(avr);
	}
#if AVR_STACK_WATCH
	/*
	 * this checks that the current "function" is not doctoring the stack frame that is located
	 * higher on the stack than it should be. It's a sign of code that has overrun it's stack
	 * frame and is munching on it's own return address.
	 */
	if (avr->trace_data->stack_frame_index > 1 && addr > avr->trace_data->stack_frame[avr->trace_data->stack_frame_index-2].sp) {
		printf( FONT_RED "%04x : munching stack "
				"SP %04x, A=%04x <= %02x\n" FONT_DEFAULT,
				avr->pc, _avr_sp_get(avr), addr, v);
	}
#endif

	if (avr->gdb) {
		avr_gdb_handle_watchpoints(avr, addr, AVR_GDB_WATCH_WRITE);
	}

	avr->data[addr] = v;
	_call_register_irqs(avr, addr);
	_call_sram_irqs(avr, addr);
}

uint8_t avr_core_watch_read(avr_t *avr, uint16_t addr)
{
	if (addr > avr->ramend) {
		AVR_LOG(avr, LOG_WARNING,
				"CORE: *** Wrapping read address "
				"PC=%04x SP=%04x O=%04x Address %04x %% %04x --> %04x\n"
				FONT_DEFAULT,
				avr->pc, _avr_sp_get(avr), _avr_flash_read16le(avr, avr->pc),
				addr, (avr->ramend + 1), addr % (avr->ramend + 1));
		addr = addr % (avr->ramend + 1);
	}

	if (avr->gdb) {
		avr_gdb_handle_watchpoints(avr, addr, AVR_GDB_WATCH_READ);
	}

//	_call_register_irqs(avr, addr);
	return avr->data[addr];
}

/*
 * Set a register (r < 256)
 * if it's an IO register (> 31) also (try to) call any callback that was
 * registered to track changes to that register.
 */
static inline void _avr_set_r(avr_t * avr, uint16_t r, uint8_t v)
{
	REG_TOUCH(avr, r);

	if (r == R_SREG) {
		avr->data[R_SREG] = v;
		// unsplit the SREG
		SET_SREG_FROM(avr, v);
		SREG();
	}
	if (r > 31) {
		avr_io_addr_t io = AVR_DATA_TO_IO(r);
		if (avr->io[io].w.c) {
			avr->io[io].w.c(avr, r, v, avr->io[io].w.param);
		} else {
			avr->data[r] = v;
			if (avr->io[io].irq) {
				avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
				for (int i = 0; i < 8; i++)
					avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);
			}
		}
        // _call_register_irqs(avr, r); // else section above could then be removed ?
		_call_sram_irqs(avr, r); // Only for io region
	} else
		avr->data[r] = v;
}

static inline void
_avr_set_r16le(
	avr_t * avr,
	uint16_t r,
	uint16_t v)
{
	_avr_set_r(avr, r, v);
	_avr_set_r(avr, r + 1, v >> 8);
}

static inline void
_avr_set_r16le_hl(
	avr_t * avr,
	uint16_t r,
	uint16_t v)
{
	_avr_set_r(avr, r + 1, v >> 8);
	_avr_set_r(avr, r , v);
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
	_avr_set_r16le(avr, R_SPL, sp);
}

/*
 * Set any address to a value; split between registers and SRAM
 */
static inline void _avr_set_ram(avr_t * avr, uint16_t addr, uint8_t v)
{
	if (addr <= avr->ioend)
		_avr_set_r(avr, addr, v);
	else
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

	} else if (addr > 31 && addr < 31 + MAX_IOs) {
		avr_io_addr_t io = AVR_DATA_TO_IO(addr);

		if (avr->io[io].r.c)
			avr->data[addr] = avr->io[io].r.c(avr, addr, avr->io[io].r.param);
#if 0
		if (avr->io[io].irq) {
			uint8_t v = avr->data[addr];
			avr_raise_irq(avr->io[io].irq + AVR_IOMEM_IRQ_ALL, v);
			for (int i = 0; i < 8; i++)
				avr_raise_irq(avr->io[io].irq + i, (v >> i) & 1);
		}
#endif
	}
	return avr_core_watch_read(avr, addr);
}

/*
 * Stack push accessors.
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

int _avr_push_addr(avr_t * avr, avr_flashaddr_t addr)
{
	uint16_t sp = _avr_sp_get(avr);
	addr >>= 1;
	for (int i = 0; i < avr->address_size; i++, addr >>= 8, sp--) {
		_avr_set_ram(avr, sp, addr);
	}
	_avr_sp_set(avr, sp);
	return avr->address_size;
}

avr_flashaddr_t _avr_pop_addr(avr_t * avr)
{
	uint16_t sp = _avr_sp_get(avr) + 1;
	avr_flashaddr_t res = 0;
	for (int i = 0; i < avr->address_size; i++, sp++) {
		res = (res << 8) | _avr_get_ram(avr, sp);
	}
	res <<= 1;
	_avr_sp_set(avr, sp -1);
	return res;
}

const char * avr_regname(avr_t * avr, unsigned int reg)
{
        if (reg > avr->ioend)
		return NULL;
	if (!avr->data_names[reg]) {
		static const char pairs[] = {'X', 'Y', 'Z'};
		char tt[16];
		if (reg < 26)
			sprintf(tt, "r%d", reg);
		else {
			if (reg < 32)
				sprintf(tt, "%c%c",
					pairs[(reg - 26) >> 1],
					(reg & 1) ? 'H' : 'L');
			else if (((reg + 1) & ~1) == R_SPH)
				sprintf(tt, "SP%c", (reg & 1) ? 'L' : 'H');
			else if (reg == R_SREG)
				sprintf(tt, "SREG");
			else
				sprintf(tt, "io:%02x", reg);
		}
		avr->data_names[reg] = strdup(tt);
	}
	return avr->data_names[reg];
}

/*
 * Called when an invalid opcode is decoded
 */
static void _avr_invalid_opcode(avr_t * avr)
{
#if CONFIG_SIMAVR_TRACE
	printf( FONT_RED "*** %04x: %-25s Invalid Opcode SP=%04x O=%04x \n" FONT_DEFAULT,
                avr->pc,
                avr->trace_data->codeline[avr->pc>>1],
                _avr_sp_get(avr),
                _avr_flash_read16le(avr, avr->pc));
#else
	AVR_LOG(avr, LOG_ERROR, FONT_RED "CORE: *** %04x: Invalid Opcode SP=%04x O=%04x \n" FONT_DEFAULT,
			avr->pc, _avr_sp_get(avr), _avr_flash_read16le(avr, avr->pc));
#endif
}

#if CONFIG_SIMAVR_TRACE
/*
 * Dump changed registers when tracing
 */
void avr_dump_state(avr_t * avr)
{
	if (!avr->trace || donttrace)
		return;

	int doit = 0;

	for (int r = 0; r < 3 && !doit; r++)
		if (avr->trace_data->touched[r])
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
			printf("%s=%02x ", AVR_REGNAME(i), avr->data[i]);
		}
	printf("\n");
}
#endif

#define get_d5(o) \
		const uint8_t d = (o >> 4) & 0x1f;

#define get_vd5(o) \
		get_d5(o) \
		const uint8_t vd = avr->data[d];

#define get_r5(o) \
		const uint8_t r = ((o >> 5) & 0x10) | (o & 0xf);

#define get_d5_a6(o) \
		get_d5(o); \
		const uint8_t A = ((((o >> 9) & 3) << 4) | ((o) & 0xf)) + 32;

#define get_vd5_s3(o) \
		get_vd5(o); \
		const uint8_t s = o & 7;

#define get_vd5_s3_mask(o) \
		get_vd5_s3(o); \
		const uint8_t mask = 1 << s;

#define get_vd5_vr5(o) \
		get_r5(o); \
		get_d5(o); \
		const uint8_t vd = avr->data[d], vr = avr->data[r];

#define get_d5_vr5(o) \
		get_d5(o); \
		get_r5(o); \
		const uint8_t vr = avr->data[r];

#define get_h4_k8(o) \
		const uint8_t h = 16 + ((o >> 4) & 0xf); \
		const uint8_t k = ((o & 0x0f00) >> 4) | (o & 0xf);

#define get_vh4_k8(o) \
		get_h4_k8(o) \
		const uint8_t vh = avr->data[h];

#define get_d5_q6(o) \
		get_d5(o) \
		const uint8_t q = ((o & 0x2000) >> 8) | ((o & 0x0c00) >> 7) | (o & 0x7);

#define get_io5(o) \
		const uint8_t io = ((o >> 3) & 0x1f) + 32;

#define get_io5_b3(o) \
		get_io5(o); \
		const uint8_t b = o & 0x7;

#define get_io5_b3mask(o) \
		get_io5(o); \
		const uint8_t mask = 1 << (o & 0x7);

//	const int16_t o = ((int16_t)(op << 4)) >> 3; // CLANG BUG!
#define get_o12(op) \
		const int16_t o = ((int16_t)((op << 4) & 0xffff)) >> 3;

#define get_vp2_k6(o) \
		const uint8_t p = 24 + ((o >> 3) & 0x6); \
		const uint8_t k = ((o & 0x00c0) >> 2) | (o & 0xf); \
		const uint16_t vp = avr->data[p] | (avr->data[p + 1] << 8);

#define get_sreg_bit(o) \
		const uint8_t b = (o >> 4) & 7;

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

/****************************************************************************\
 *
 * Helper functions for calculating the status register bit values.
 * See the Atmel data sheet for the instruction set for more info.
 *
\****************************************************************************/

static  void
_avr_flags_zns (struct avr_t * avr, uint8_t res)
{
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static  void
_avr_flags_zns16 (struct avr_t * avr, uint16_t res)
{
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_N] = (res >> 15) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static  void
_avr_flags_add_zns (struct avr_t * avr, uint8_t res, uint8_t rd, uint8_t rr)
{
	/* carry & half carry */
	uint8_t add_carry = (rd & rr) | (rr & ~res) | (~res & rd);
	avr->sreg[S_H] = (add_carry >> 3) & 1;
	avr->sreg[S_C] = (add_carry >> 7) & 1;

	/* overflow */
	avr->sreg[S_V] = (((rd & rr & ~res) | (~rd & ~rr & res)) >> 7) & 1;

	/* zns */
	_avr_flags_zns(avr, res);
}


static  void
_avr_flags_sub_zns (struct avr_t * avr, uint8_t res, uint8_t rd, uint8_t rr)
{
	/* carry & half carry */
	uint8_t sub_carry = (~rd & rr) | (rr & res) | (res & ~rd);
	avr->sreg[S_H] = (sub_carry >> 3) & 1;
	avr->sreg[S_C] = (sub_carry >> 7) & 1;

	/* overflow */
	avr->sreg[S_V] = (((rd & ~rr & ~res) | (~rd & rr & res)) >> 7) & 1;

	/* zns */
	_avr_flags_zns(avr, res);
}

static  void
_avr_flags_Rzns (struct avr_t * avr, uint8_t res)
{
	if (res)
		avr->sreg[S_Z] = 0;
	avr->sreg[S_N] = (res >> 7) & 1;
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static  void
_avr_flags_sub_Rzns (struct avr_t * avr, uint8_t res, uint8_t rd, uint8_t rr)
{
	/* carry & half carry */
	uint8_t sub_carry = (~rd & rr) | (rr & res) | (res & ~rd);
	avr->sreg[S_H] = (sub_carry >> 3) & 1;
	avr->sreg[S_C] = (sub_carry >> 7) & 1;

	/* overflow */
	avr->sreg[S_V] = (((rd & ~rr & ~res) | (~rd & rr & res)) >> 7) & 1;

	_avr_flags_Rzns(avr, res);
}

static  void
_avr_flags_zcvs (struct avr_t * avr, uint8_t res, uint8_t vr)
{
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vr & 1;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static  void
_avr_flags_zcnvs (struct avr_t * avr, uint8_t res, uint8_t vr)
{
	avr->sreg[S_Z] = res == 0;
	avr->sreg[S_C] = vr & 1;
	avr->sreg[S_N] = res >> 7;
	avr->sreg[S_V] = avr->sreg[S_N] ^ avr->sreg[S_C];
	avr->sreg[S_S] = avr->sreg[S_N] ^ avr->sreg[S_V];
}

static  void
_avr_flags_znv0s (struct avr_t * avr, uint8_t res)
{
	avr->sreg[S_V] = 0;
	_avr_flags_zns(avr, res);
}

static inline int _avr_is_instruction_32_bits(avr_t * avr, avr_flashaddr_t pc)
{
	uint16_t o = _avr_flash_read16le(avr, pc) & 0xfe0f;
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
avr_flashaddr_t avr_run_one(avr_t * avr)
{
run_one_again:
#if CONFIG_SIMAVR_TRACE
	/*
	 * this traces spurious reset or bad jumps
	 */
	if ((avr->pc == 0 && avr->cycle > 0) || avr->pc >= avr->codeend ||
		_avr_sp_get(avr) > avr->ramend) {
//		avr->trace = 1;
		STATE("RESET\n");
//		printf("Bad: %d %d %d %x\n", (avr->pc == 0 && avr->cycle > 0),
//		avr->pc >= avr->codeend, _avr_sp_get(avr) > avr->ramend, avr->pc);
		crash(avr);
	}
	avr->trace_data->touched[0] = avr->trace_data->touched[1] =
		avr->trace_data->touched[2] = 0;
#endif

	/* Ensure we don't crash simavr due to a bad instruction reading past
	 * the end of the flash.
	 */
	if (unlikely(avr->pc >= avr->flashend)) {
		STATE("CRASH\n");
		crash(avr);
		return 0;
	}

	uint32_t		opcode = _avr_flash_read16le(avr, avr->pc);
	avr_flashaddr_t	new_pc = avr->pc + 2;	// future "default" pc
	int 			cycle = 1;

	switch (opcode & 0xf000) {
		case 0x0000: {
			switch (opcode) {
				case 0x0000: {	// NOP
					STATE("nop\n");
				}	break;
				default: {
					switch (opcode & 0xfc00) {
						case 0x0400: {	// CPC -- Compare with carry -- 0000 01rd dddd rrrr
							get_vd5_vr5(opcode);
							uint8_t res = vd - vr - avr->sreg[S_C];
							STATE("cpc %s[%02x], %s[%02x] = %02x\n", AVR_REGNAME(d), vd, AVR_REGNAME(r), vr, res);
							_avr_flags_sub_Rzns(avr, res, vd, vr);
							SREG();
						}	break;
						case 0x0c00: {	// ADD -- Add without carry -- 0000 11rd dddd rrrr
							get_vd5_vr5(opcode);
							uint8_t res = vd + vr;
							if (r == d) {
								STATE("lsl %s[%02x] = %02x\n", AVR_REGNAME(d), vd, res & 0xff);
							} else {
								STATE("add %s[%02x], %s[%02x] = %02x\n", AVR_REGNAME(d), vd, AVR_REGNAME(r), vr, res);
							}
							_avr_set_r(avr, d, res);
							_avr_flags_add_zns(avr, res, vd, vr);
							SREG();
						}	break;
						case 0x0800: {	// SBC -- Subtract with carry -- 0000 10rd dddd rrrr
							get_vd5_vr5(opcode);
							uint8_t res = vd - vr - avr->sreg[S_C];
							STATE("sbc %s[%02x], %s[%02x] = %02x\n", AVR_REGNAME(d), avr->data[d], AVR_REGNAME(r), avr->data[r], res);
							_avr_set_r(avr, d, res);
							_avr_flags_sub_Rzns(avr, res, vd, vr);
							SREG();
						}	break;
						default:
							switch (opcode & 0xff00) {
								case 0x0100: {	// MOVW -- Copy Register Word -- 0000 0001 dddd rrrr
									uint8_t d = ((opcode >> 4) & 0xf) << 1;
									uint8_t r = ((opcode) & 0xf) << 1;
									STATE("movw %s:%s, %s:%s[%02x%02x]\n", AVR_REGNAME(d), AVR_REGNAME(d+1), AVR_REGNAME(r), AVR_REGNAME(r+1), avr->data[r+1], avr->data[r]);
									uint16_t vr = avr->data[r] | (avr->data[r + 1] << 8);
									_avr_set_r16le(avr, d, vr);
								}	break;
								case 0x0200: {	// MULS -- Multiply Signed -- 0000 0010 dddd rrrr
									int8_t r = 16 + (opcode & 0xf);
									int8_t d = 16 + ((opcode >> 4) & 0xf);
									int16_t res = ((int8_t)avr->data[r]) * ((int8_t)avr->data[d]);
									STATE("muls %s[%d], %s[%02x] = %d\n", AVR_REGNAME(d), ((int8_t)avr->data[d]), AVR_REGNAME(r), ((int8_t)avr->data[r]), res);
									_avr_set_r16le(avr, 0, res);
									avr->sreg[S_C] = (res >> 15) & 1;
									avr->sreg[S_Z] = res == 0;
									cycle++;
									SREG();
								}	break;
								case 0x0300: {	// MUL -- Multiply -- 0000 0011 fddd frrr
									int8_t r = 16 + (opcode & 0x7);
									int8_t d = 16 + ((opcode >> 4) & 0x7);
									int16_t res = 0;
									uint8_t c = 0;
									T(const char * name = "";)
									switch (opcode & 0x88) {
										case 0x00: 	// MULSU -- Multiply Signed Unsigned -- 0000 0011 0ddd 0rrr
											res = ((uint8_t)avr->data[r]) * ((int8_t)avr->data[d]);
											c = (res >> 15) & 1;
											T(name = "mulsu";)
											break;
										case 0x08: 	// FMUL -- Fractional Multiply Unsigned -- 0000 0011 0ddd 1rrr
											res = ((uint8_t)avr->data[r]) * ((uint8_t)avr->data[d]);
											c = (res >> 15) & 1;
											res <<= 1;
											T(name = "fmul";)
											break;
										case 0x80: 	// FMULS -- Multiply Signed -- 0000 0011 1ddd 0rrr
											res = ((int8_t)avr->data[r]) * ((int8_t)avr->data[d]);
											c = (res >> 15) & 1;
											res <<= 1;
											T(name = "fmuls";)
											break;
										case 0x88: 	// FMULSU -- Multiply Signed Unsigned -- 0000 0011 1ddd 1rrr
											res = ((uint8_t)avr->data[r]) * ((int8_t)avr->data[d]);
											c = (res >> 15) & 1;
											res <<= 1;
											T(name = "fmulsu";)
											break;
									}
									cycle++;
									STATE("%s %s[%d], %s[%02x] = %d\n", name, AVR_REGNAME(d), ((int8_t)avr->data[d]), AVR_REGNAME(r), ((int8_t)avr->data[r]), res);
									_avr_set_r16le(avr, 0, res);
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
				case 0x1800: {	// SUB -- Subtract without carry -- 0001 10rd dddd rrrr
					get_vd5_vr5(opcode);
					uint8_t res = vd - vr;
					STATE("sub %s[%02x], %s[%02x] = %02x\n", AVR_REGNAME(d), vd, AVR_REGNAME(r), vr, res);
					_avr_set_r(avr, d, res);
					_avr_flags_sub_zns(avr, res, vd, vr);
					SREG();
				}	break;
				case 0x1000: {	// CPSE -- Compare, skip if equal -- 0001 00rd dddd rrrr
					get_vd5_vr5(opcode);
					uint16_t res = vd == vr;
					STATE("cpse %s[%02x], %s[%02x]\t; Will%s skip\n", AVR_REGNAME(d), avr->data[d], AVR_REGNAME(r), avr->data[r], res ? "":" not");
					if (res) {
						if (_avr_is_instruction_32_bits(avr, new_pc)) {
							new_pc += 4; cycle += 2;
						} else {
							new_pc += 2; cycle++;
						}
					}
				}	break;
				case 0x1400: {	// CP -- Compare -- 0001 01rd dddd rrrr
					get_vd5_vr5(opcode);
					uint8_t res = vd - vr;
					STATE("cp %s[%02x], %s[%02x] = %02x\n", AVR_REGNAME(d), vd, AVR_REGNAME(r), vr, res);
					_avr_flags_sub_zns(avr, res, vd, vr);
					SREG();
				}	break;
				case 0x1c00: {	// ADD -- Add with carry -- 0001 11rd dddd rrrr
					get_vd5_vr5(opcode);
					uint8_t res = vd + vr + avr->sreg[S_C];
					if (r == d) {
						STATE("rol %s[%02x] = %02x\n", AVR_REGNAME(d), avr->data[d], res);
					} else {
						STATE("addc %s[%02x], %s[%02x] = %02x\n", AVR_REGNAME(d), avr->data[d], AVR_REGNAME(r), avr->data[r], res);
					}
					_avr_set_r(avr, d, res);
					_avr_flags_add_zns(avr, res, vd, vr);
					SREG();
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0x2000: {
			switch (opcode & 0xfc00) {
				case 0x2000: {	// AND -- Logical AND -- 0010 00rd dddd rrrr
					get_vd5_vr5(opcode);
					uint8_t res = vd & vr;
					if (r == d) {
						STATE("tst %s[%02x]\n", AVR_REGNAME(d), avr->data[d]);
					} else {
						STATE("and %s[%02x], %s[%02x] = %02x\n", AVR_REGNAME(d), vd, AVR_REGNAME(r), vr, res);
					}
					_avr_set_r(avr, d, res);
					_avr_flags_znv0s(avr, res);
					SREG();
				}	break;
				case 0x2400: {	// EOR -- Logical Exclusive OR -- 0010 01rd dddd rrrr
					get_vd5_vr5(opcode);
					uint8_t res = vd ^ vr;
					if (r==d) {
						STATE("clr %s[%02x]\n", AVR_REGNAME(d), avr->data[d]);
					} else {
						STATE("eor %s[%02x], %s[%02x] = %02x\n", AVR_REGNAME(d), vd, AVR_REGNAME(r), vr, res);
					}
					_avr_set_r(avr, d, res);
					_avr_flags_znv0s(avr, res);
					SREG();
				}	break;
				case 0x2800: {	// OR -- Logical OR -- 0010 10rd dddd rrrr
					get_vd5_vr5(opcode);
					uint8_t res = vd | vr;
					STATE("or %s[%02x], %s[%02x] = %02x\n", AVR_REGNAME(d), vd, AVR_REGNAME(r), vr, res);
					_avr_set_r(avr, d, res);
					_avr_flags_znv0s(avr, res);
					SREG();
				}	break;
				case 0x2c00: {	// MOV -- 0010 11rd dddd rrrr
					get_d5_vr5(opcode);
					uint8_t res = vr;
					STATE("mov %s, %s[%02x] = %02x\n", AVR_REGNAME(d), AVR_REGNAME(r), vr, res);
					_avr_set_r(avr, d, res);
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0x3000: {	// CPI -- Compare Immediate -- 0011 kkkk hhhh kkkk
			get_vh4_k8(opcode);
			uint8_t res = vh - k;
			STATE("cpi %s[%02x], 0x%02x\n", AVR_REGNAME(h), vh, k);
			_avr_flags_sub_zns(avr, res, vh, k);
			SREG();
		}	break;

		case 0x4000: {	// SBCI -- Subtract Immediate With Carry -- 0100 kkkk hhhh kkkk
			get_vh4_k8(opcode);
			uint8_t res = vh - k - avr->sreg[S_C];
			STATE("sbci %s[%02x], 0x%02x = %02x\n", AVR_REGNAME(h), vh, k, res);
			_avr_set_r(avr, h, res);
			_avr_flags_sub_Rzns(avr, res, vh, k);
			SREG();
		}	break;

		case 0x5000: {	// SUBI -- Subtract Immediate -- 0101 kkkk hhhh kkkk
			get_vh4_k8(opcode);
			uint8_t res = vh - k;
			STATE("subi %s[%02x], 0x%02x = %02x\n", AVR_REGNAME(h), vh, k, res);
			_avr_set_r(avr, h, res);
			_avr_flags_sub_zns(avr, res, vh, k);
			SREG();
		}	break;

		case 0x6000: {	// ORI aka SBR -- Logical OR with Immediate -- 0110 kkkk hhhh kkkk
			get_vh4_k8(opcode);
			uint8_t res = vh | k;
			STATE("ori %s[%02x], 0x%02x\n", AVR_REGNAME(h), vh, k);
			_avr_set_r(avr, h, res);
			_avr_flags_znv0s(avr, res);
			SREG();
		}	break;

		case 0x7000: {	// ANDI	-- Logical AND with Immediate -- 0111 kkkk hhhh kkkk
			get_vh4_k8(opcode);
			uint8_t res = vh & k;
			STATE("andi %s[%02x], 0x%02x\n", AVR_REGNAME(h), vh, k);
			_avr_set_r(avr, h, res);
			_avr_flags_znv0s(avr, res);
			SREG();
		}	break;

		case 0xa000:
		case 0x8000: {
			/*
			 * Load (LDD/STD) store instructions
			 *
			 * 10q0 qqsd dddd yqqq
			 * s = 0 = load, 1 = store
			 * y = 16 bits register index, 1 = Y, 0 = X
			 * q = 6 bit displacement
			 */
			switch (opcode & 0xd008) {
				case 0xa000:
				case 0x8000: {	// LD (LDD) -- Load Indirect using Z -- 10q0 qqsd dddd yqqq
					uint16_t v = avr->data[R_ZL] | (avr->data[R_ZH] << 8);
					get_d5_q6(opcode);
					if (opcode & 0x0200) {
						STATE("st (Z+%d[%04x]), %s[%02x]  \t%s\n",
						      q, v+q, AVR_REGNAME(d), avr->data[d], DAS(v + q));
						_avr_set_ram(avr, v+q, avr->data[d]);
					} else {
						STATE("ld %s, (Z+%d[%04x])=[%02x]  \t%s\n",
						      AVR_REGNAME(d), q, v+q, avr->data[v+q], DAS(v + q));
						_avr_set_r(avr, d, _avr_get_ram(avr, v+q));
					}
					cycle += 1; // 2 cycles, 3 for tinyavr
				}	break;
				case 0xa008:
				case 0x8008: {	// LD (LDD) -- Load Indirect using Y -- 10q0 qqsd dddd yqqq
					uint16_t v = avr->data[R_YL] | (avr->data[R_YH] << 8);
					get_d5_q6(opcode);
					if (opcode & 0x0200) {
						STATE("st (Y+%d[%04x]), %s[%02x]  \t%s\n",
						      q, v+q, AVR_REGNAME(d), avr->data[d], DAS(v + q));
						_avr_set_ram(avr, v+q, avr->data[d]);
					} else {
						STATE("ld %s, (Y+%d[%04x])=[%02x]  \t%s\n",
						      AVR_REGNAME(d), q, v+q, avr->data[d+q], DAS(v + q));
						_avr_set_r(avr, d, _avr_get_ram(avr, v+q));
					}
					cycle += 1; // 2 cycles, 3 for tinyavr
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0x9000: {
			/* this is an annoying special case, but at least these lines handle all the SREG set/clear opcodes */
			if ((opcode & 0xff0f) == 0x9408) {
				get_sreg_bit(opcode);
				STATE("%s%c\n", opcode & 0x0080 ? "cl" : "se", _sreg_bit_name[b]);
				avr_sreg_set(avr, b, (opcode & 0x0080) == 0);
				SREG();
			} else switch (opcode) {
				case 0x9588: { // SLEEP -- 1001 0101 1000 1000
					STATE("sleep\n");
					/* Don't sleep if there are interrupts about to be serviced.
					 * Without this check, it was possible to incorrectly enter a state
					 * in which the cpu was sleeping and interrupts were disabled. For more
					 * details, see the commit message. */
					if (!avr_has_pending_interrupts(avr) || !avr->sreg[S_I])
						avr->state = cpu_Sleeping;
				}	break;
				case 0x9598: { // BREAK -- 1001 0101 1001 1000
					STATE("break\n");
					if (avr->gdb) {
						// if gdb is on, break here.
						avr->state = cpu_Stopped;
						avr_gdb_handle_break(avr);
					}
				}	break;
				case 0x95a8: { // WDR -- Watchdog Reset -- 1001 0101 1010 1000
					STATE("wdr\n");
					avr_ioctl(avr, AVR_IOCTL_WATCHDOG_RESET, 0);
				}	break;
				case 0x95e8: { // SPM -- Store Program Memory -- 1001 0101 1110 1000
					STATE("spm\n");
					avr_ioctl(avr, AVR_IOCTL_FLASH_SPM, 0);
				}	break;
				case 0x9409:   // IJMP -- Indirect jump -- 1001 0100 0000 1001
				case 0x9419:   // EIJMP -- Indirect jump -- 1001 0100 0001 1001   bit 4 is "indirect"
				case 0x9509:   // ICALL -- Indirect Call to Subroutine -- 1001 0101 0000 1001
				case 0x9519: { // EICALL -- Indirect Call to Subroutine -- 1001 0101 0001 1001   bit 8 is "push pc"
					int e = opcode & 0x10;
					int p = opcode & 0x100;
					if (e && !avr->eind)
						_avr_invalid_opcode(avr);
					uint32_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8);
					if (e)
						z |= avr->data[avr->eind] << 16;
					STATE("%si%s Z[%04x]\n", e?"e":"", p?"call":"jmp", z << 1);
					if (p)
						cycle += _avr_push_addr(avr, new_pc) - 1;
					new_pc = z << 1;
					cycle++;
					TRACE_JUMP();
				}	break;
				case 0x9518: 	// RETI -- Return from Interrupt -- 1001 0101 0001 1000
					avr_sreg_set(avr, S_I, 1);
					avr_interrupt_reti(avr);
					FALLTHROUGH
				case 0x9508: {	// RET -- Return -- 1001 0101 0000 1000
					new_pc = _avr_pop_addr(avr);
					cycle += 1 + avr->address_size;
					STATE("ret%s\n", opcode & 0x10 ? "i" : "");
					TRACE_JUMP();
					STACK_FRAME_POP();
				}	break;
				case 0x95c8: {	// LPM -- Load Program Memory R0 <- (Z) -- 1001 0101 1100 1000
					uint16_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8);
					STATE("lpm %s, (Z[%04x]) \t%s\n",
					      AVR_REGNAME(0), z, FAS(z));
					uint8_t v = avr->flash[z];
					avr_ioctl(avr, AVR_IOCTL_FLASH_LPM, &v);
					_avr_set_r(avr, 0, v);
					cycle += 2; // 3 cycles
				}	break;
				case 0x95d8: {	// ELPM -- Load Program Memory R0 <- (Z) -- 1001 0101 1101 1000
					if (!avr->rampz)
						_avr_invalid_opcode(avr);
					uint32_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8) | (avr->data[avr->rampz] << 16);
					STATE("elpm %s, (Z[%02x:%04x] \t%s)\n",
					      AVR_REGNAME(0), z >> 16,
					      z & 0xffff, FAS(z));
					uint8_t v = avr->flash[z];
					avr_ioctl(avr, AVR_IOCTL_FLASH_LPM, &v);
					_avr_set_r(avr, 0, v);
					cycle += 2; // 3 cycles
				}	break;
				default:  {
					switch (opcode & 0xfe0f) {
						case 0x9000: {	// LDS -- Load Direct from Data Space, 32 bits -- 1001 0000 0000 0000
							get_d5(opcode);
							uint16_t x = _avr_flash_read16le(avr, new_pc);
							new_pc += 2;
							STATE("lds %s[%02x], 0x%04x\t\t%s\n",
							      AVR_REGNAME(d), avr->data[d], x, DAS(x));
							_avr_set_r(avr, d, _avr_get_ram(avr, x));
							cycle++; // 2 cycles
						}	break;
						case 0x9005:
						case 0x9004: {	// LPM -- Load Program Memory -- 1001 000d dddd 01oo
							get_d5(opcode);
							uint16_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8);
							int op = opcode & 1;
							STATE("lpm %s, (Z[%04x]%s)\t\t%s\n",
							      AVR_REGNAME(d), z, op ? "+" : "", FAS(z));
							uint8_t v = avr->flash[z];
							avr_ioctl(avr, AVR_IOCTL_FLASH_LPM, &v);
							_avr_set_r(avr, d, v);
							if (op) {
								z++;
								_avr_set_r16le_hl(avr, R_ZL, z);
							}
							cycle += 2; // 3 cycles
						}	break;
						case 0x9006:
						case 0x9007: {	// ELPM -- Extended Load Program Memory -- 1001 000d dddd 01oo
							if (!avr->rampz)
								_avr_invalid_opcode(avr);
							uint32_t z = avr->data[R_ZL] | (avr->data[R_ZH] << 8) | (avr->data[avr->rampz] << 16);
							get_d5(opcode);
							int op = opcode & 1;
							STATE("elpm %s, (Z[%02x:%04x]%s)\t\t%s\n",
							      AVR_REGNAME(d), z >> 16, z & 0xffff, op ? "+" : "", FAS(z));
							uint8_t v = avr->flash[z];
							avr_ioctl(avr, AVR_IOCTL_FLASH_LPM, &v);
							_avr_set_r(avr, d, v);
							if (op) {
								z++;
								_avr_set_r(avr, avr->rampz, z >> 16);
								_avr_set_r16le_hl(avr, R_ZL, z);
							}
							cycle += 2; // 3 cycles
						}	break;
						/*
						 * Load store instructions
						 *
						 * 1001 00sr rrrr iioo
						 * s = 0 = load, 1 = store
						 * ii = 16 bits register index, 11 = X, 10 = Y, 00 = Z
						 * oo = 1) post increment, 2) pre-decrement
						 */
						case 0x900c:
						case 0x900d:
						case 0x900e: {	// LD -- Load Indirect from Data using X -- 1001 000d dddd 11oo
							int op = opcode & 3;
							get_d5(opcode);
							uint16_t x = (avr->data[R_XH] << 8) | avr->data[R_XL];
							STATE("ld %s, %sX[%04x]%s \t\t%s\n",
							      AVR_REGNAME(d),
							      op == 2 ? "--" : "", x, op == 1 ? "++" : "", DAS(x));
							cycle++; // 2 cycles (1 for tinyavr, except with inc/dec 2)
							if (op == 2) x--;
							uint8_t vd = _avr_get_ram(avr, x);
							if (op == 1) x++;
							_avr_set_r16le_hl(avr, R_XL, x);
							_avr_set_r(avr, d, vd);
						}	break;
						case 0x920c:
						case 0x920d:
						case 0x920e: {	// ST -- Store Indirect Data Space X -- 1001 001d dddd 11oo
							int op = opcode & 3;
							get_vd5(opcode);
							uint16_t x = (avr->data[R_XH] << 8) | avr->data[R_XL];
							STATE("st %sX[%04x]%s, %s[%02x] \t\t%s\n",
							      op == 2 ? "--" : "", x, op == 1 ? "++" : "",
							      AVR_REGNAME(d), vd, DAS(x));
							cycle++; // 2 cycles, except tinyavr
							if (op == 2) x--;
							_avr_set_ram(avr, x, vd);
							if (op == 1) x++;
							_avr_set_r16le_hl(avr, R_XL, x);
						}	break;
						case 0x9009:
						case 0x900a: {	// LD -- Load Indirect from Data using Y -- 1001 000d dddd 10oo
							int op = opcode & 3;
							get_d5(opcode);
							uint16_t y = (avr->data[R_YH] << 8) | avr->data[R_YL];
							STATE("ld %s, %sY[%04x]%s \t\t%s\n",
							      AVR_REGNAME(d),
							      op == 2 ? "--" : "", y, op == 1 ? "++" : "",
							      DAS(y));
							cycle++; // 2 cycles, except tinyavr
							if (op == 2) y--;
							uint8_t vd = _avr_get_ram(avr, y);
							if (op == 1) y++;
							_avr_set_r16le_hl(avr, R_YL, y);
							_avr_set_r(avr, d, vd);
						}	break;
						case 0x9209:
						case 0x920a: {	// ST -- Store Indirect Data Space Y -- 1001 001d dddd 10oo
							int op = opcode & 3;
							get_vd5(opcode);
							uint16_t y = (avr->data[R_YH] << 8) | avr->data[R_YL];
							STATE("st %sY[%04x]%s, %s[%02x] \t\t%s\n",
							      op == 2 ? "--" : "", y, op == 1 ? "++" : "",
							      AVR_REGNAME(d), vd, DAS(y));
							cycle++;
							if (op == 2) y--;
							_avr_set_ram(avr, y, vd);
							if (op == 1) y++;
							_avr_set_r16le_hl(avr, R_YL, y);
						}	break;
						case 0x9200: {	// STS -- Store Direct to Data Space, 32 bits -- 1001 0010 0000 0000
							get_vd5(opcode);
							uint16_t x = _avr_flash_read16le(avr, new_pc);
							new_pc += 2;
							STATE("sts 0x%04x, %s[%02x]\t\t%s\n",
							      x, AVR_REGNAME(d), vd, DAS(x));
							cycle++;
							_avr_set_ram(avr, x, vd);
						}	break;
						case 0x9001:
						case 0x9002: {	// LD -- Load Indirect from Data using Z -- 1001 000d dddd 00oo
							int op = opcode & 3;
							get_d5(opcode);
							uint16_t z = (avr->data[R_ZH] << 8) | avr->data[R_ZL];
							STATE("ld %s, %sZ[%04x]%s \t\t%s\n", AVR_REGNAME(d),
							      op == 2 ? "--" : "", z, op == 1 ? "++" : "", DAS(z));
							cycle++;; // 2 cycles, except tinyavr
							if (op == 2) z--;
							uint8_t vd = _avr_get_ram(avr, z);
							if (op == 1) z++;
							_avr_set_r16le_hl(avr, R_ZL, z);
							_avr_set_r(avr, d, vd);
						}	break;
						case 0x9201:
						case 0x9202: {	// ST -- Store Indirect Data Space Z -- 1001 001d dddd 00oo
							int op = opcode & 3;
							get_vd5(opcode);
							uint16_t z = (avr->data[R_ZH] << 8) | avr->data[R_ZL];
							STATE("st %sZ[%04x]%s, %s[%02x] \t\t%s\n",
							      op == 2 ? "--" : "", z, op == 1 ? "++" : "",
							      AVR_REGNAME(d), vd, DAS(z));
							cycle++; // 2 cycles, except tinyavr
							if (op == 2) z--;
							_avr_set_ram(avr, z, vd);
							if (op == 1) z++;
							_avr_set_r16le_hl(avr, R_ZL, z);
						}	break;
						case 0x900f: {	// POP -- 1001 000d dddd 1111
							get_d5(opcode);
							_avr_set_r(avr, d, _avr_pop8(avr));
							T(uint16_t sp = _avr_sp_get(avr);)
							STATE("pop %s (@%04x)[%02x]\n", AVR_REGNAME(d), sp, avr->data[sp]);
							cycle++;
						}	break;
						case 0x920f: {	// PUSH -- 1001 001d dddd 1111
							get_vd5(opcode);
							_avr_push8(avr, vd);
							T(uint16_t sp = _avr_sp_get(avr);)
							STATE("push %s[%02x] (@%04x)\n", AVR_REGNAME(d), vd, sp);
							cycle++;
						}	break;
						case 0x9400: {	// COM -- One's Complement -- 1001 010d dddd 0000
							get_vd5(opcode);
							uint8_t res = 0xff - vd;
							STATE("com %s[%02x] = %02x\n", AVR_REGNAME(d), vd, res);
							_avr_set_r(avr, d, res);
							_avr_flags_znv0s(avr, res);
							avr->sreg[S_C] = 1;
							SREG();
						}	break;
						case 0x9401: {	// NEG -- Two's Complement -- 1001 010d dddd 0001
							get_vd5(opcode);
							uint8_t res = 0x00 - vd;
							STATE("neg %s[%02x] = %02x\n", AVR_REGNAME(d), vd, res);
							_avr_set_r(avr, d, res);
							avr->sreg[S_H] = ((res >> 3) | (vd >> 3)) & 1;
							avr->sreg[S_V] = res == 0x80;
							avr->sreg[S_C] = res != 0;
							_avr_flags_zns(avr, res);
							SREG();
						}	break;
						case 0x9402: {	// SWAP -- Swap Nibbles -- 1001 010d dddd 0010
							get_vd5(opcode);
							uint8_t res = (vd >> 4) | (vd << 4) ;
							STATE("swap %s[%02x] = %02x\n", AVR_REGNAME(d), vd, res);
							_avr_set_r(avr, d, res);
						}	break;
						case 0x9403: {	// INC -- Increment -- 1001 010d dddd 0011
							get_vd5(opcode);
							uint8_t res = vd + 1;
							STATE("inc %s[%02x] = %02x\n", AVR_REGNAME(d), vd, res);
							_avr_set_r(avr, d, res);
							avr->sreg[S_V] = res == 0x80;
							_avr_flags_zns(avr, res);
							SREG();
						}	break;
						case 0x9405: {	// ASR -- Arithmetic Shift Right -- 1001 010d dddd 0101
							get_vd5(opcode);
							uint8_t res = (vd >> 1) | (vd & 0x80);
							STATE("asr %s[%02x]\n", AVR_REGNAME(d), vd);
							_avr_set_r(avr, d, res);
							_avr_flags_zcnvs(avr, res, vd);
							SREG();
						}	break;
						case 0x9406: {	// LSR -- Logical Shift Right -- 1001 010d dddd 0110
							get_vd5(opcode);
							uint8_t res = vd >> 1;
							STATE("lsr %s[%02x]\n", AVR_REGNAME(d), vd);
							_avr_set_r(avr, d, res);
							avr->sreg[S_N] = 0;
							_avr_flags_zcvs(avr, res, vd);
							SREG();
						}	break;
						case 0x9407: {	// ROR -- Rotate Right -- 1001 010d dddd 0111
							get_vd5(opcode);
							uint8_t res = (avr->sreg[S_C] ? 0x80 : 0) | vd >> 1;
							STATE("ror %s[%02x]\n", AVR_REGNAME(d), vd);
							_avr_set_r(avr, d, res);
							_avr_flags_zcnvs(avr, res, vd);
							SREG();
						}	break;
						case 0x940a: {	// DEC -- Decrement -- 1001 010d dddd 1010
							get_vd5(opcode);
							uint8_t res = vd - 1;
							STATE("dec %s[%02x] = %02x\n", AVR_REGNAME(d), vd, res);
							_avr_set_r(avr, d, res);
							avr->sreg[S_V] = res == 0x7f;
							_avr_flags_zns(avr, res);
							SREG();
						}	break;
						case 0x940c:
						case 0x940d: {	// JMP -- Long Call to sub, 32 bits -- 1001 010a aaaa 110a
							avr_flashaddr_t a = ((opcode & 0x01f0) >> 3) | (opcode & 1);
							uint16_t x = _avr_flash_read16le(avr, new_pc);
							a = (a << 16) | x;
							STATE("jmp 0x%06x\n", a);
							new_pc = a << 1;
							cycle += 2;
							TRACE_JUMP();
						}	break;
						case 0x940e:
						case 0x940f: {	// CALL -- Long Call to sub, 32 bits -- 1001 010a aaaa 111a
							avr_flashaddr_t a = ((opcode & 0x01f0) >> 3) | (opcode & 1);
							uint16_t x = _avr_flash_read16le(avr, new_pc);
							a = (a << 16) | x;
							STATE("call 0x%06x\n", a);
							new_pc += 2;
							cycle += 1 + _avr_push_addr(avr, new_pc);
							new_pc = a << 1;
							TRACE_JUMP();
							STACK_FRAME_PUSH();
						}	break;

						default: {
							switch (opcode & 0xff00) {
								case 0x9600: {	// ADIW -- Add Immediate to Word -- 1001 0110 KKpp KKKK
									get_vp2_k6(opcode);
									uint16_t res = vp + k;
									STATE("adiw %s:%s[%04x], 0x%02x\n", AVR_REGNAME(p), AVR_REGNAME(p + 1), vp, k);
									_avr_set_r16le_hl(avr, p, res);
									avr->sreg[S_V] = ((~vp & res) >> 15) & 1;
									avr->sreg[S_C] = ((~res & vp) >> 15) & 1;
									_avr_flags_zns16(avr, res);
									SREG();
									cycle++;
								}	break;
								case 0x9700: {	// SBIW -- Subtract Immediate from Word -- 1001 0111 KKpp KKKK
									get_vp2_k6(opcode);
									uint16_t res = vp - k;
									STATE("sbiw %s:%s[%04x], 0x%02x\n", AVR_REGNAME(p), AVR_REGNAME(p + 1), vp, k);
									_avr_set_r16le_hl(avr, p, res);
									avr->sreg[S_V] = ((vp & ~res) >> 15) & 1;
									avr->sreg[S_C] = ((res & ~vp) >> 15) & 1;
									_avr_flags_zns16(avr, res);
									SREG();
									cycle++;
								}	break;
								case 0x9800: {	// CBI -- Clear Bit in I/O Register -- 1001 1000 AAAA Abbb
									get_io5_b3mask(opcode);
									uint8_t res = _avr_get_ram(avr, io) & ~mask;
									STATE("cbi %s[%04x], 0x%02x = %02x\n", AVR_REGNAME(io), avr->data[io], mask, res);
									_avr_set_ram(avr, io, res);
									cycle++;
								}	break;
								case 0x9900: {	// SBIC -- Skip if Bit in I/O Register is Cleared -- 1001 1001 AAAA Abbb
									get_io5_b3mask(opcode);
									uint8_t res = _avr_get_ram(avr, io) & mask;
									STATE("sbic %s[%04x], 0x%02x\t; Will%s branch\n", AVR_REGNAME(io), avr->data[io], mask, !res?"":" not");
									if (!res) {
										if (_avr_is_instruction_32_bits(avr, new_pc)) {
											new_pc += 4; cycle += 2;
										} else {
											new_pc += 2; cycle++;
										}
									}
								}	break;
								case 0x9a00: {	// SBI -- Set Bit in I/O Register -- 1001 1010 AAAA Abbb
									get_io5_b3mask(opcode);
									uint8_t res = _avr_get_ram(avr, io) | mask;
									STATE("sbi %s[%04x], 0x%02x = %02x\n", AVR_REGNAME(io), avr->data[io], mask, res);
									_avr_set_ram(avr, io, res);
									cycle++;
								}	break;
								case 0x9b00: {	// SBIS -- Skip if Bit in I/O Register is Set -- 1001 1011 AAAA Abbb
									get_io5_b3mask(opcode);
									uint8_t res = _avr_get_ram(avr, io) & mask;
									STATE("sbis %s[%04x], 0x%02x\t; Will%s branch\n", AVR_REGNAME(io), avr->data[io], mask, res?"":" not");
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
										case 0x9c00: {	// MUL -- Multiply Unsigned -- 1001 11rd dddd rrrr
											get_vd5_vr5(opcode);
											uint16_t res = vd * vr;
											STATE("mul %s[%02x], %s[%02x] = %04x\n", AVR_REGNAME(d), vd, AVR_REGNAME(r), vr, res);
											cycle++;
											_avr_set_r16le(avr, 0, res);
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
				case 0xb800: {	// OUT A,Rr -- 1011 1AAd dddd AAAA
					get_d5_a6(opcode);
					STATE("out %s, %s[%02x]\n", AVR_REGNAME(A), AVR_REGNAME(d), avr->data[d]);
					_avr_set_ram(avr, A, avr->data[d]);
				}	break;
				case 0xb000: {	// IN Rd,A -- 1011 0AAd dddd AAAA
					get_d5_a6(opcode);
					STATE("in %s, %s[%02x]\n", AVR_REGNAME(d), AVR_REGNAME(A), avr->data[A]);
					_avr_set_r(avr, d, _avr_get_ram(avr, A));
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		case 0xc000: {	// RJMP -- 1100 kkkk kkkk kkkk
			get_o12(opcode);
			STATE("rjmp .%d [%04x]\n", o >> 1, new_pc + o);
			new_pc = (new_pc + o) % (avr->flashend+1);
			cycle++;
			TRACE_JUMP();
		}	break;

		case 0xd000: {	// RCALL -- 1101 kkkk kkkk kkkk
			get_o12(opcode);
			STATE("rcall .%d [%04x]\n", o >> 1, new_pc + o);
			cycle += _avr_push_addr(avr, new_pc);
			new_pc = (new_pc + o) % (avr->flashend+1);
			// 'rcall .1' is used as a cheap "push 16 bits of room on the stack"
			if (o != 0) {
				TRACE_JUMP();
				STACK_FRAME_PUSH();
			}
		}	break;

		case 0xe000: {	// LDI Rd, K aka SER (LDI r, 0xff) -- 1110 kkkk dddd kkkk
			get_h4_k8(opcode);
			STATE("ldi %s, 0x%02x\n", AVR_REGNAME(h), k);
			_avr_set_r(avr, h, k);
		}	break;

		case 0xf000: {
			switch (opcode & 0xfe00) {
				case 0xf100: {	/* simavr special opcodes */
					if (opcode == 0xf1f1) { // AVR_OVERFLOW_OPCODE
						printf("FLASH overflow, soft reset\n");
						new_pc = 0;
						TRACE_JUMP();
					}
				}	break;
				case 0xf000:
				case 0xf200:
				case 0xf400:
				case 0xf600: {	// BRXC/BRXS -- All the SREG branches -- 1111 0Boo oooo osss
					int16_t o = ((int16_t)(opcode << 6)) >> 9; // offset
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
						cycle++; // 2 cycles if taken, 1 otherwise
						new_pc = new_pc + (o << 1);
					}
				}	break;
				case 0xf800:
				case 0xf900: {	// BLD -- Bit Store from T into a Bit in Register -- 1111 100d dddd 0bbb
					get_vd5_s3_mask(opcode);
					uint8_t v = (vd & ~mask) | (avr->sreg[S_T] ? mask : 0);
					STATE("bld %s[%02x], 0x%02x = %02x\n", AVR_REGNAME(d), vd, mask, v);
					_avr_set_r(avr, d, v);
				}	break;
				case 0xfa00:
				case 0xfb00:{	// BST -- Bit Store into T from bit in Register -- 1111 101d dddd 0bbb
					get_vd5_s3(opcode)
					STATE("bst %s[%02x], 0x%02x\n", AVR_REGNAME(d), vd, 1 << s);
					avr->sreg[S_T] = (vd >> s) & 1;
					SREG();
				}	break;
				case 0xfc00:
				case 0xfe00: {	// SBRS/SBRC -- Skip if Bit in Register is Set/Clear -- 1111 11sd dddd 0bbb
					get_vd5_s3_mask(opcode)
					int set = (opcode & 0x0200) != 0;
					int branch = ((vd & mask) && set) || (!(vd & mask) && !set);
					STATE("%s %s[%02x], 0x%02x\t; Will%s branch\n", set ? "sbrs" : "sbrc", AVR_REGNAME(d), vd, mask, branch ? "":" not");
					if (branch) {
						if (_avr_is_instruction_32_bits(avr, new_pc)) {
							new_pc += 4; cycle += 2;
						} else {
							new_pc += 2; cycle++;
						}
					}
				}	break;
				default: _avr_invalid_opcode(avr);
			}
		}	break;

		default: _avr_invalid_opcode(avr);

	}
	avr->cycle += cycle;

	if ((avr->state == cpu_Running) &&
		(avr->run_cycle_count > cycle) &&
		(avr->interrupt_state == 0))
	{
		avr->run_cycle_count -= cycle;
		avr->pc = new_pc;
		goto run_one_again;
	}

	return new_pc;
}
