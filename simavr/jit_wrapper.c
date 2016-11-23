
/*
 * Ignore this for now, this tests the minimal bits of C needed to compile the
 * translated code. This file will eventually vanish
 */
#define NULL ((void*)0L)
#define T(w) w
//#define STATE(...)
#define STATE(_f, args...) printf("%04x: " _f, new_pc-2, ## args)
#define SREG()
#define STACK_FRAME_PUSH()
#define STACK_FRAME_POP()

#define _avr_invalid_opcode(a) {}

static const char * _sreg_bit_name = "cznvshti";

enum {
	// SREG bit indexes
	S_C = 0,S_Z,S_N,S_V,S_S,S_H,S_T,S_I,

	// 16 bits register pairs
	R_XL	= 0x1a, R_XH,R_YL,R_YH,R_ZL,R_ZH,
	// stack pointer
	R_SPL	= 32+0x3d, R_SPH,
	// real SREG
	R_SREG	= 32+0x3f,

	// maximum number of IO registers, on normal AVRs
	MAX_IOs	= 280,	// Bigger AVRs need more than 256-32 (mega1280)
};


#define SREG_BIT(_b) 		(avr_data[R_SREG] & (1 << (_b)))
#define SREG_SETBIT(_b, _v) 	avr_data[R_SREG] = (avr_data[R_SREG] & ~(1 << (_b))) | (!!(_v) << (_b));

/*
 * Core states.
 */
enum {
	cpu_Limbo = 0,	// before initialization is finished
	cpu_Stopped,	// all is stopped, timers included

	cpu_Running,	// we're free running

	cpu_Sleeping,	// we're now sleeping until an interrupt

	cpu_Step,		// run ONE instruction, then...
	cpu_StepDone,	// tell gdb it's all OK, and give it registers
	cpu_Done,       // avr software stopped gracefully
	cpu_Crashed,    // avr software crashed (watchdog fired)
};

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef short int16_t;
typedef char int8_t;

typedef unsigned long avr_flashaddr_t;

const char * avr_regname(uint8_t reg);
void _avr_set_r(
	void * avr,
	uint16_t r,
	uint8_t v);
void
avr_core_watch_write(
	void *avr,
	uint16_t addr,
	uint8_t v);
uint8_t
_avr_get_ram(
	void * avr,
	uint16_t addr);
int
avr_has_pending_interrupts(
		void * avr);
void
avr_interrupt_reti(
		void * avr);

typedef struct jit_avr_t {
	void * avr;
	uint8_t * data;
	uint8_t * flash;
} jit_avr_t, *jit_avr_p;

static jit_avr_t * jit_avr = NULL;

#define avr_data jit_avr->data
#define avr_flash jit_avr->flash
#define avr_state *state
#define avr jit_avr->avr

static inline uint16_t
_avr_flash_read16le(
	void * ignore,
	avr_flashaddr_t addr)
{
	return (avr_flash[addr] | (avr_flash[addr + 1] << 8));
}
static inline void
_avr_set_r16le(
	void * ignore,
	uint16_t r,
	uint16_t v)
{
	_avr_set_r(avr, r, v);
	_avr_set_r(avr, r + 1, v >> 8);
}

static inline void
_avr_set_r16le_hl(
	void * ignore,
	uint16_t r,
	uint16_t v)
{
	_avr_set_r(avr, r + 1, v >> 8);
	_avr_set_r(avr, r , v);
}


inline uint16_t _avr_sp_get(void * ignore)
{
	return avr_data[R_SPL] | (avr_data[R_SPH] << 8);
}

inline void _avr_sp_set(void * ignore, uint16_t sp)
{
	_avr_set_r16le(avr, R_SPL, sp);
}

/*
 * Set any address to a value; split between registers and SRAM
 */
static inline void _avr_set_ram(void * ignore, uint16_t addr, uint8_t v)
{
	if (addr < MAX_IOs + 31)
		_avr_set_r(avr, addr, v);
	else
		avr_core_watch_write(avr, addr, v);
}

#define avr_sreg_set(_ignore, flag, ival) \
	if (flag == S_I) { \
		if (ival) { \
			if (!SREG_BIT(S_I)) \
				*is = -2; \
		} else \
			*is = 0; \
	}\
	SREG_SETBIT(flag, ival);

/*
 * Stack push accessors.
 */
static inline void
_avr_push8(
	void * ignore,
	uint16_t v)
{
	uint16_t sp = _avr_sp_get(avr);
	_avr_set_ram(avr, sp, v);
	_avr_sp_set(avr, sp-1);
}

static inline uint8_t
_avr_pop8(
	void * ignore)
{
	uint16_t sp = _avr_sp_get(avr) + 1;
	uint8_t res = _avr_get_ram(avr, sp);
	_avr_sp_set(avr, sp);
	return res;
}

static avr_flashaddr_t
_avr_pop_addr(
	void * ignore)
{
	uint16_t sp = _avr_sp_get(avr) + 1;
	avr_flashaddr_t res = 0;
	for (int i = 0; i < avr_address_size; i++, sp++) {
		res = (res << 8) | _avr_get_ram(avr, sp);
	}
	res <<= 1;
	_avr_sp_set(avr, sp -1);
	return res;
}

int
_avr_push_addr(
	void * ignore,
	avr_flashaddr_t addr)
{
	uint16_t sp = _avr_sp_get(avr);
	addr >>= 1;
	for (int i = 0; i < avr_address_size; i++, addr >>= 8, sp--) {
		_avr_set_ram(avr, sp, addr);
	}
	_avr_sp_set(avr, sp);
	return avr_address_size;
}

/****************************************************************************\
 *
 * Helper functions for calculating the status register bit values.
 * See the Atmel data sheet for the instruction set for more info.
 *
\****************************************************************************/

static  void
_avr_flags_zns (void * ignore, uint8_t res)
{
	SREG_SETBIT(S_Z, res == 0);
	SREG_SETBIT(S_N, (res >> 7) & 1);
	SREG_SETBIT(S_S, SREG_BIT(S_N) ^ SREG_BIT(S_V));
}

static  void
_avr_flags_zns16 (void * ignore, uint16_t res)
{
	SREG_SETBIT(S_Z, res == 0);
	SREG_SETBIT(S_N, (res >> 15) & 1);
	SREG_SETBIT(S_S, SREG_BIT(S_N) ^ SREG_BIT(S_V));
}

static  void
_avr_flags_add_zns (void * ignore, uint8_t res, uint8_t rd, uint8_t rr)
{
	/* carry & half carry */
	uint8_t add_carry = (rd & rr) | (rr & ~res) | (~res & rd);
	SREG_SETBIT(S_H, (add_carry >> 3) & 1);
	SREG_SETBIT(S_C, (add_carry >> 7) & 1);

	/* overflow */
	SREG_SETBIT(S_V, (((rd & rr & ~res) | (~rd & ~rr & res)) >> 7) & 1);

	/* zns */
	_avr_flags_zns(avr, res);
}


static  void
_avr_flags_sub_zns (void * ignore, uint8_t res, uint8_t rd, uint8_t rr)
{
	/* carry & half carry */
	uint8_t sub_carry = (~rd & rr) | (rr & res) | (res & ~rd);
	SREG_SETBIT(S_H, (sub_carry >> 3) & 1);
	SREG_SETBIT(S_C, (sub_carry >> 7) & 1);

	/* overflow */
	SREG_SETBIT(S_V, (((rd & ~rr & ~res) | (~rd & rr & res)) >> 7) & 1);

	/* zns */
	_avr_flags_zns(avr, res);
}

static  void
_avr_flags_Rzns (void * ignore, uint8_t res)
{
	if (res)
		SREG_SETBIT(S_Z, 0);
	SREG_SETBIT(S_N, (res >> 7) & 1);
	SREG_SETBIT(S_S, SREG_BIT(S_N) ^ SREG_BIT(S_V));
}

static  void
_avr_flags_sub_Rzns (void * ignore, uint8_t res, uint8_t rd, uint8_t rr)
{
	/* carry & half carry */
	uint8_t sub_carry = (~rd & rr) | (rr & res) | (res & ~rd);
	SREG_SETBIT(S_H, (sub_carry >> 3) & 1);
	SREG_SETBIT(S_C, (sub_carry >> 7) & 1);
	/* overflow */
	SREG_SETBIT(S_V, (((rd & ~rr & ~res) | (~rd & rr & res)) >> 7) & 1);
	_avr_flags_Rzns(avr, res);
}

static  void
_avr_flags_zcvs (void * ignore, uint8_t res, uint8_t vr)
{
	SREG_SETBIT(S_Z, res == 0);
	SREG_SETBIT(S_C, vr & 1);
	SREG_SETBIT(S_V, SREG_BIT(S_N) ^ SREG_BIT(S_C));
	SREG_SETBIT(S_S, SREG_BIT(S_N) ^ SREG_BIT(S_V));
}

static  void
_avr_flags_zcnvs (void * ignore, uint8_t res, uint8_t vr)
{
	SREG_SETBIT(S_Z, res == 0);
	SREG_SETBIT(S_C, vr & 1);
	SREG_SETBIT(S_N, res >> 7);
	SREG_SETBIT(S_V, SREG_BIT(S_N) ^ SREG_BIT(S_C));
	SREG_SETBIT(S_S, SREG_BIT(S_N) ^ SREG_BIT(S_V));
}

static  void
_avr_flags_znv0s (void * ignore, uint8_t res)
{
	SREG_SETBIT(S_V, 0);
	_avr_flags_zns(avr, res);
}


#define get_d5(o) \
		const uint8_t d = (o >> 4) & 0x1f

#define get_vd5(o) \
		get_d5(o); \
		const uint8_t vd = avr_data[d]

#define get_r5(o) \
		const uint8_t r = ((o >> 5) & 0x10) | (o & 0xf)

#define get_d5_a6(o) \
		get_d5(o); \
		const uint8_t A = ((((o >> 9) & 3) << 4) | ((o) & 0xf)) + 32

#define get_vd5_s3(o) \
		get_vd5(o); \
		const uint8_t s = o & 7

#define get_vd5_s3_mask(o) \
		get_vd5_s3(o); \
		const uint8_t mask = 1 << s

#define get_vd5_vr5(o) \
		get_r5(o); \
		get_d5(o); \
		const uint8_t vd = avr_data[d], vr = avr_data[r]

#define get_d5_vr5(o) \
		get_d5(o); \
		get_r5(o); \
		const uint8_t vr = avr_data[r]

#define get_h4_k8(o) \
		const uint8_t h = 16 + ((o >> 4) & 0xf); \
		const uint8_t k = ((o & 0x0f00) >> 4) | (o & 0xf)

#define get_vh4_k8(o) \
		get_h4_k8(o); \
		const uint8_t vh = avr_data[h]

#define get_d5_q6(o) \
		get_d5(o); \
		const uint8_t q = ((o & 0x2000) >> 8) | ((o & 0x0c00) >> 7) | (o & 0x7)

#define get_io5(o) \
		const uint8_t io = ((o >> 3) & 0x1f) + 32

#define get_io5_b3(o) \
		get_io5(o); \
		const uint8_t b = o & 0x7

#define get_io5_b3mask(o) \
		get_io5(o); \
		const uint8_t mask = 1 << (o & 0x7)

//	const int16_t o = ((int16_t)(op << 4)) >> 3; // CLANG BUG!
#define get_o12(op) \
		const int16_t o = ((int16_t)((op << 4) & 0xffff)) >> 3

#define get_vp2_k6(o) \
		const uint8_t p = 24 + ((o >> 3) & 0x6); \
		const uint8_t k = ((o & 0x00c0) >> 2) | (o & 0xf); \
		const uint16_t vp = avr_data[p] | (avr_data[p + 1] << 8)

#define get_sreg_bit(o) \
		const uint8_t b = (o >> 4) & 7

#define TRACE_JUMP()	{ \
		if (*is || cycle >= howLong) goto exit;\
		goto *jt[new_pc/2]; \
	}
#define CORE_SLEEP() goto exit

avr_flashaddr_t
firmware(
	jit_avr_t * _jit_avr,
	avr_flashaddr_t pc,
	int * state,
	int8_t * is,
	int * cycles,
	int howLong )
{
	uint16_t new_pc = pc;
	int cycle = 0;

	jit_avr = _jit_avr;

//	printf("Hi There %p %p\n", avr_flash, avr_data);

#include "jit_code.c"

exit:
	*cycles += cycle;
	return new_pc;
}
