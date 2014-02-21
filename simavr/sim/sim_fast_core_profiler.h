/*
	sim_fast_core_profiler.h

	Copyright 2014 Michael Hughes <squirmyworms@embarqmail.com>

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

//#define FAST_CORE_UINST_PROFILING

#include <sys/time.h>

#define KHz(hz) ((hz)*1000ULL)
#define MHz(hz) KHz(KHz(hz))

#if 0
static inline uint64_t get_cycles(void) {
   	uint32_t hi, lo;
   	
	__asm__ __volatile__ ("xorl %%eax,%%edx" : : : "%eax", "%edx");
	__asm__ __volatile__ ("xorl %%edx,%%eax" : : : "%eax", "%edx");
	__asm__ __volatile__ ("xorl %%eax,%%edx" : : : "%eax", "%edx");
	__asm__ __volatile__ ("xorl %%edx,%%eax" : : : "%eax", "%edx");
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	
	return(((uint64_t)hi << 32)|(uint64_t)lo);
}
static inline uint64_t get_dtime(void) { return(get_cycles()); }
#else
static inline uint64_t get_dtime(void) {
	struct timeval	t;
	uint64_t	dsec;
	uint64_t	dusec;

	gettimeofday(&t, (struct timezone *)NULL);
	dsec=MHz(KHz(t.tv_sec));
	dusec=((uint64_t)t.tv_usec);

	return(dsec + dusec);
}
#endif

extern uint64_t calibrate_get_dtime_cycles(void);
extern uint64_t calibrate_get_dtime(void);
extern uint64_t calibrate(void);

#ifndef FAST_CORE_UINST_PROFILING
#define PROFILE_INIT()
#define PROFILE(x,y) y
#define PROFILE_START(x)
#define PROFILE_STOP(x)
#define PROFILE_IPS()
#define PROFILE_ISEQ(u_opcode)
#define PROFILE_ISEQ_FLUSH(name)
#else

typedef struct profile_t * profile_p;
typedef struct profile_t {
	const char * name;
	uint64_t count;
	uint64_t elapsed;
	double average;
	double percentage;
}profile_t;

typedef struct core_profile_t * core_profile_p;
typedef struct core_profile_t {
	profile_t uinst[256];

	profile_t core_loop;
	profile_t uinst_total;

	profile_t isr;

	profile_t ior;
	profile_t ior_data;
	profile_t ior_irq;
	profile_t ior_rc;
	profile_t ior_rc_irq;
	profile_t ior_sreg;

	profile_t iow;
	profile_t iow_data;
	profile_t iow_irq;
	profile_t iow_wc;
	profile_t iow_wc_irq;
	profile_t iow_sreg;

	profile_t timer;
	
	profile_t ips;
	
	uint64_t iseq_profile[65536];
	uint8_t iseq_flush[256];
	uint8_t iseq_cache;
}core_profile_t;

extern core_profile_t core_profile;
extern const char *uinst_op_profile_names[256];

#define PROFILE_START(x) \
	core_profile.x.count++; \
	uint64_t start = get_dtime();
#define PROFILE_STOP(x) \
	core_profile.x.elapsed += get_dtime() - start;

#define PROFILE(x, y) { \
	PROFILE_START(x); \
	y; PROFILE_STOP(x); }

extern void fast_core_profiler_init(void);
#define PROFILE_INIT() fast_core_profiler_init()

#define PROFILE_IPS() core_profile.ips.count++

#define PROFILE_ISEQ() { \
	UINST_GET_OP(u_opcode_op, u_opcode); \
	core_profile.iseq_profile[(core_profile.iseq_cache << 8) + u_opcode_op]++; \
	core_profile.iseq_cache = u_opcode_op; }

static inline void fast_core_profiler_iseq_flush(uint8_t op) {
	core_profile.iseq_flush[op] = 0;
}

#define PROFILE_ISEQ_FLUSH(op) fast_core_profiler_iseq_flush(k_avr_uinst_##op)

#define AVERAGE(x) ((double)core_profile.x.elapsed/(double)core_profile.x.count)

#define PERCENTAGE(x, y) (((double)core_profile.x.elapsed/(double)core_profile.y.elapsed)*100.0)

#define MAXixy(x, y, ix, iy) ((x > y) ? ix : iy)
#define MINixy(x, y, ix, iy) ((x < y) ? ix : iy)

extern void fast_core_profiler_init(void);
#endif
