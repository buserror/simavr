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

//#define CONFIG_AVR_FAST_CORE_UINST_PROFILING

#ifndef __SIM_FAST_CORE_PROFILER_H
#define __SIM_FAST_CORE_PROFILER_H

#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

uint64_t avr_fast_core_profiler_dtime_calibrate(void);

/* CONFIG_SIMAVR_FAST_CORE_DTIME_RDTSC
 *	platform specific option, set in makefile if desired. */
//#define CONFIG_SIMAVR_FAST_CORE_DTIME_RDTSC
#ifdef CONFIG_SIMAVR_FAST_CORE_DTIME_RDTSC
static inline uint64_t avr_fast_core_profiler_get_dtime(void) {
   	uint32_t hi, lo;
   	
	__asm__ __volatile__ ("xorl %%eax,%%edx" : : : "%eax", "%edx");
	__asm__ __volatile__ ("xorl %%edx,%%eax" : : : "%eax", "%edx");
	__asm__ __volatile__ ("xorl %%eax,%%edx" : : : "%eax", "%edx");
	__asm__ __volatile__ ("xorl %%edx,%%eax" : : : "%eax", "%edx");
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	
	return(((uint64_t)hi << 32) | (uint64_t)lo);
}
#else
static inline uint64_t avr_fast_core_profiler_get_dtime(void) {
	struct timeval	t;
	uint64_t	dsec;
	uint64_t	dusec;

	gettimeofday(&t, (struct timezone *)NULL);
	dsec = t.tv_sec * 1000ULL * 1000ULL;
	dusec = ((uint64_t)t.tv_usec);

	return(dsec + dusec);
}
#endif


#ifndef CONFIG_AVR_FAST_CORE_UINST_PROFILING


#define AVR_FAST_CORE_PROFILER_INIT(_avr_fast_core_uinst_op_names)

#define AVR_FAST_CORE_PROFILER_PROFILE(x,y) y

#define AVR_FAST_CORE_PROFILER_PROFILE_START(x)
#define AVR_FAST_CORE_PROFILER_PROFILE_STOP(x)

#define AVR_FAST_CORE_PROFILER_PROFILE_IPS()

#define AVR_FAST_CORE_PROFILER_PROFILE_ISEQ(u_opcode)
#define AVR_FAST_CORE_PROFILER_PROFILE_ISEQ_FLUSH(name)


#else


#define AVR_FAST_CORE_PROFILER_INIT(_avr_fast_core_uinst_op_names) \
	avr_fast_core_profiler_init(_avr_fast_core_uinst_op_names)

#define AVR_FAST_CORE_PROFILER_PROFILE_IPS() avr_fast_core_profiler_core_profile.ips.count++

#define AVR_FAST_CORE_PROFILER_PROFILE_ISEQ(_iseq_opcode_op) { \
	uint16_t _iseq_cache_op = (avr_fast_core_profiler_core_profile.iseq_cache << 8) + _iseq_opcode_op; \
	avr_fast_core_profiler_core_profile.iseq_profile[_iseq_cache_op]++; \
	avr_fast_core_profiler_core_profile.iseq_cache = _iseq_opcode_op; }
	
#define AVR_FAST_CORE_PROFILER_PROFILE_ISEQ_FLUSH(op) avr_fast_core_profiler_iseq_flush(_avr_fast_core_uinst_##op##_k)

#define AVR_FAST_CORE_PROFILER_PROFILE(x, y) { \
	AVR_FAST_CORE_PROFILER_PROFILE_START(x); \
	y; AVR_FAST_CORE_PROFILER_PROFILE_STOP(x); }

#define AVR_FAST_CORE_PROFILER_PROFILE_START(x) \
	avr_fast_core_profiler_core_profile.x.count++; \
	uint64_t start = avr_fast_core_profiler_get_dtime();
	
#define AVR_FAST_CORE_PROFILER_PROFILE_STOP(x) \
	avr_fast_core_profiler_core_profile.x.elapsed += (avr_fast_core_profiler_get_dtime() - start);


typedef struct avr_fast_core_profiler_profile_t * avr_fast_core_profiler_profile_p;
typedef struct avr_fast_core_profiler_profile_t {
	const char * name;
	uint64_t count;
	uint64_t elapsed;
	double average;
	double percentage;
}avr_fast_core_profiler_profile_t;

typedef struct avr_fast_core_profiler_core_profile_t * avr_fast_core_profiler_core_profile_p;
typedef struct avr_fast_core_profiler_core_profile_t {
	avr_fast_core_profiler_profile_t uinst[256];

	avr_fast_core_profiler_profile_t core_loop;
	avr_fast_core_profiler_profile_t uinst_total;

	avr_fast_core_profiler_profile_t isr;

	/* io read */
	avr_fast_core_profiler_profile_t ior;
	avr_fast_core_profiler_profile_t ior_data;
	avr_fast_core_profiler_profile_t ior_irq;
	avr_fast_core_profiler_profile_t ior_rc;
	avr_fast_core_profiler_profile_t ior_rc_irq;
	avr_fast_core_profiler_profile_t ior_sreg;

	/* io write */
	avr_fast_core_profiler_profile_t iow;
	avr_fast_core_profiler_profile_t iow_data;
	avr_fast_core_profiler_profile_t iow_irq;
	avr_fast_core_profiler_profile_t iow_wc;
	avr_fast_core_profiler_profile_t iow_wc_irq;
	avr_fast_core_profiler_profile_t iow_sreg;

	avr_fast_core_profiler_profile_t timer;

	/* instructions per second */
	avr_fast_core_profiler_profile_t ips;
	
	/* instruction sequence */
	uint64_t iseq_profile[65536];
	uint8_t iseq_flush[256];
	uint8_t iseq_cache;
}avr_fast_core_profiler_core_profile_t;

extern struct avr_fast_core_profiler_core_profile_t avr_fast_core_profiler_core_profile;

void avr_fast_core_profiler_init(const char *uinst_op_names[256]);
void avr_fast_core_profiler_generate_report(void);

static inline void avr_fast_core_profiler_iseq_flush(uint8_t op) {
	avr_fast_core_profiler_core_profile.iseq_flush[op] = 0;
}

#ifdef __cplusplus
extern "C" {
#endif

#endif /* #ifdef CONFIG_AVR_FAST_CORE_UINST_PROFILING */
#endif /* #ifndef __SIM_FAST_CORE_PROFILER_H */

