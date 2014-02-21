/*
	sim_fast_core_profiler.c

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

#ifndef __SIM_FAST_CORE_PROFILER_C
#define __SIM_FAST_CORE_PROFILER_C

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "sim_fast_core_profiler.h"

#define KHz(hz) ((hz)*1000ULL)
#define MHz(hz) KHz(KHz(hz))

static uint64_t _avr_fast_core_profiler_calibrate_get_dtime_loop(void)
{
   	uint64_t start, elapsedTime;

	start = avr_fast_core_profiler_get_dtime();
	elapsedTime = avr_fast_core_profiler_get_dtime() - start;

	int i;
	for(i=2; i<=1024; i++) {
		start = avr_fast_core_profiler_get_dtime();
		elapsedTime += avr_fast_core_profiler_get_dtime() - start;
	}
		
	return(elapsedTime / i);
	
}

static uint64_t _avr_fast_core_profiler_calibrate_get_dtime_sleep(void)
{
   	uint64_t start = avr_fast_core_profiler_get_dtime();
	
	sleep(1);
		
	return(avr_fast_core_profiler_get_dtime() - start);
}

extern uint64_t avr_fast_core_profiler_dtime_calibrate(void)
{
	uint64_t cycleTime = _avr_fast_core_profiler_calibrate_get_dtime_loop();
	uint64_t elapsedTime, ecdt;
	double emhz;

	printf("%s: calibrate_get_dtime_cycles(%016llu)\n", __FUNCTION__, cycleTime);

	elapsedTime = 0;

	for(int i = 1; i <= 3; i++) {
		elapsedTime += _avr_fast_core_profiler_calibrate_get_dtime_sleep() - cycleTime;

		ecdt = elapsedTime / i;
		emhz = ecdt / MHz(1);
		printf("%s: elapsed time: %016llu  ecdt: %016llu  estMHz: %010.4f\n", __FUNCTION__, elapsedTime, ecdt, emhz);
	}
	return(ecdt);
}

#ifdef CONFIG_AVR_FAST_CORE_UINST_PROFILING

#define AVERAGE(x) ((double)avr_fast_core_profiler_core_profile.x.elapsed / \
	(double)avr_fast_core_profiler_core_profile.x.count)

#define PERCENTAGE(x, y) (((double)avr_fast_core_profiler_core_profile.x.elapsed / \
	(double)avr_fast_core_profiler_core_profile.y.elapsed) * 100.0)

#define MAXixy(x, y, ix, iy) ((x > y) ? ix : iy)
#define MINixy(x, y, ix, iy) ((x < y) ? ix : iy)

struct avr_fast_core_profiler_core_profile_t avr_fast_core_profiler_core_profile;

extern void avr_fast_core_profiler_generate_report(void)
{
	avr_fast_core_profiler_profile_p sorted_list0[256];
	avr_fast_core_profiler_profile_p sorted_list1[256];
	avr_fast_core_profiler_core_profile_p core_profile = &avr_fast_core_profiler_core_profile;

	printf("\n\n>> fast core profile report\n");
	printf(">> raw profile list\n");

	const char * name_count_elapsed_average_status_line = 
		"name: %55s -- count: %012llu, elapsed: %016llu, avg: %012.4f\n";
	
	for(int i = 0; i < 256; i++) {
		sorted_list0[i] = 0;
		
		avr_fast_core_profiler_profile_p uopd = &core_profile->uinst[i];
				
		if(uopd && uopd->name && uopd->count) {
			core_profile->uinst_total.count += uopd->count;
			core_profile->uinst_total.elapsed += uopd->elapsed;
			
			uopd->average = (double)uopd->elapsed/(double)uopd->count;
			printf(name_count_elapsed_average_status_line, 
				uopd->name, uopd->count, uopd->elapsed, uopd->average);

		/* sort next for count */
			int inserti;
			for(inserti = 0; sorted_list0[inserti] && sorted_list0[inserti]->count > uopd->count; inserti++)
				;

			for(int x = 255; x > inserti; x--)
				sorted_list0[x] = sorted_list0[x - 1];

			sorted_list0[inserti] = uopd;
		}
	}

	printf("\n\n>> instructions not called\n");
	
	for(int i = 0; i < 256; i++) {
		avr_fast_core_profiler_profile_p uopd = &core_profile->uinst[i];

		if(uopd && uopd->name) {
			if(!uopd->count)
				printf(name_count_elapsed_average_status_line, 
					uopd->name, uopd->count, uopd->elapsed, uopd->average);
			else
				uopd->percentage = PERCENTAGE(uinst[i], uinst_total);
		}
	}
	
	printf("\n\n>> sorted by count\n");
	
	const char * name_count_elapsed_average_percentage_status_line = 
		"name: %55s -- count: %012llu, elapsed: %016llu, avg: %012.4f, pctg: %%%08.4f\n";
		
	for(int i = 0; i < 256; i++) {
		sorted_list1[i] = 0;
		avr_fast_core_profiler_profile_p uopd = sorted_list0[i];
		if(uopd && uopd->name && uopd->count) {
			printf(name_count_elapsed_average_percentage_status_line, 
				uopd->name, uopd->count, uopd->elapsed, uopd->average, uopd->percentage);

		/* sort next for average */
			int inserti;
			for(inserti = 0; sorted_list1[inserti] && sorted_list1[inserti]->average > uopd->average; inserti++)
				;

			for(int x = 255; x > inserti; x--)
				sorted_list1[x] = sorted_list1[x - 1];

			sorted_list1[inserti] = uopd;
		}
	}

	printf("\n\n>> sorted by average\n");
	
	for(int i = 0; i < 256; i++) {
		sorted_list0[i] = 0;
		avr_fast_core_profiler_profile_p uopd = sorted_list1[i];
		if(uopd && uopd->name && uopd->count) {
			printf(name_count_elapsed_average_percentage_status_line, 
				uopd->name, uopd->count, uopd->elapsed, uopd->average, uopd->percentage);
				
		/* sort next for percentage */
			int inserti;
			for(inserti = 0; sorted_list0[inserti] && sorted_list0[inserti]->percentage > uopd->percentage; inserti++)
				;

			for(int x = 255; x > inserti; x--)
				sorted_list0[x] = sorted_list0[x - 1];

			sorted_list0[inserti] = uopd;
		}
	}

	printf("\n\n>> sorted by percentage impact\n");
	
	for(int i = 0; i < 256; i++) {
		avr_fast_core_profiler_profile_p uopd = sorted_list0[i];
		if(uopd && uopd->name && uopd->count) {
			printf(name_count_elapsed_average_percentage_status_line, 
				uopd->name, uopd->count, uopd->elapsed, uopd->average, uopd->percentage);
		}
	}
	
	
	printf("\n\n>> instruction vs io, interrupt and cycle timer usage statistics\n");

	const char *status_line = "%32s -- cycles: %016llu count: %016llu average: %012.4f\n";
	const char *percentage_status_line = "%32s -- cycles: %016llu count: %016llu average: %012.4f %%%09.4f\n";

	printf(status_line, "core loop", core_profile->core_loop.elapsed, core_profile->core_loop.count, AVERAGE(core_loop));
	printf(percentage_status_line, "instruction <<", core_profile->uinst_total.elapsed,  core_profile->uinst_total.count,
		AVERAGE(uinst_total), PERCENTAGE(uinst_total, core_loop));
	printf(percentage_status_line, "isr <<", core_profile->isr.elapsed, core_profile->isr.count,
		AVERAGE(isr), PERCENTAGE(isr, core_loop));
	printf(percentage_status_line, "timer <<", core_profile->timer.elapsed, core_profile->timer.count,
		AVERAGE(timer), PERCENTAGE(timer, core_loop));

	printf("\n");

	printf(status_line, "instruction", core_profile->uinst_total.elapsed,  core_profile->uinst_total.count,
		AVERAGE(uinst_total));
	printf(percentage_status_line, "io write <<", core_profile->iow.elapsed, core_profile->iow.count,
		AVERAGE(iow), PERCENTAGE(iow, uinst_total));
	printf(percentage_status_line, "io read <<", core_profile->ior.elapsed, core_profile->ior.count,
		AVERAGE(ior), PERCENTAGE(ior, uinst_total));
		
	printf("\n");

	printf(status_line, "io read", core_profile->ior.elapsed, core_profile->ior.count,
		AVERAGE(ior));
	if(core_profile->ior_rc.count) {
		printf(percentage_status_line, "callback no irq <<",
			core_profile->ior_rc.elapsed, core_profile->ior_rc.count,
			AVERAGE(ior_rc), PERCENTAGE(ior_rc, ior));
	}
	if(core_profile->ior_rc_irq.count) {
		printf(percentage_status_line, "callback and irq <<",
			core_profile->ior_rc_irq.elapsed, core_profile->ior_rc_irq.count,
			AVERAGE(ior_rc_irq), PERCENTAGE(ior_rc_irq, ior));
	}
	if(core_profile->ior_data.count) {
		printf(percentage_status_line, "data <<",
			core_profile->ior_data.elapsed, core_profile->ior_data.count,
			AVERAGE(ior_data), PERCENTAGE(ior_data, ior));
	}
	if(core_profile->ior_irq.count) {
		printf(percentage_status_line, "irq <<",
			core_profile->ior_irq.elapsed, core_profile->ior_irq.count,
			AVERAGE(ior_irq), PERCENTAGE(ior_irq, ior));
	}
	if(core_profile->ior_sreg.count) {
		printf(percentage_status_line, "sreg <<",
			core_profile->ior_sreg.elapsed, core_profile->ior_sreg.count,
			AVERAGE(ior_sreg), PERCENTAGE(ior_sreg, ior));
	}

	printf("\n");

	printf(status_line, "io write", core_profile->iow.elapsed, core_profile->iow.count,
		AVERAGE(iow));
	if(core_profile->iow_wc.count) {
		printf(percentage_status_line, "callback no irq <<",
			core_profile->iow_wc.elapsed, core_profile->iow_wc.count,
			AVERAGE(iow_wc), PERCENTAGE(iow_wc, iow));
	}
	if(core_profile->iow_wc_irq.count) {
		printf(percentage_status_line, "callback and irq <<",
			core_profile->iow_wc_irq.elapsed, core_profile->iow_wc_irq.count,
			AVERAGE(iow_wc_irq), PERCENTAGE(iow_wc_irq, iow));
	}
	if(core_profile->iow_data.count) {
		printf(percentage_status_line, "data <<",
			core_profile->iow_data.elapsed, core_profile->iow_data.count,
			AVERAGE(iow_data), PERCENTAGE(iow_data, iow));
	}
	if(core_profile->iow_irq.count) {
		printf(percentage_status_line, "irq <<",
			core_profile->iow_irq.elapsed, core_profile->iow_irq.count,
			AVERAGE(iow_irq), PERCENTAGE(iow_irq, iow));
	}
	if(core_profile->iow_sreg.count) {
		printf(percentage_status_line, "sreg <<",
			core_profile->iow_sreg.elapsed, core_profile->iow_sreg.count,
			AVERAGE(iow_sreg), PERCENTAGE(iow_sreg, iow));
	}

	printf("\n\n>> top instruction sequence combinations\n");

	for(int x = 0; x < 256; x++) {
		if(0 == core_profile->iseq_flush[x]) {
			int ix = x << 8;
			for(int  y = 0; y < 256; y++) {
				core_profile->iseq_profile[ix++] = 0;
			}
		}
	}	

	for(int  i = 0; i<50; i++) {
		int maxiy = 0;
		for(int  ix = 0; ix < 65536; ix++) {
			maxiy = MAXixy(core_profile->iseq_profile[ix], core_profile->iseq_profile[maxiy], ix, maxiy);
		}
		
		avr_fast_core_profiler_profile_t *from = &core_profile->uinst[maxiy >> 8];
		avr_fast_core_profiler_profile_t *tooo = &core_profile->uinst[maxiy & 0xff];
		printf("%55s -- %55s count: 0x%016llu %04x\n", from->name, tooo->name, core_profile->iseq_profile[maxiy], maxiy);
		core_profile->iseq_profile[maxiy] = 0;
	}
}

#define CLEAR_PROFILE_DATA(x) \
	core_profile->x.count = 0; \
	core_profile->x.elapsed = 0; \
	core_profile->x.average =0; \
	core_profile->x.percentage =0; \

extern void avr_fast_core_profiler_init(const char *uinst_op_names[256])
{
	avr_fast_core_profiler_core_profile_p core_profile = &avr_fast_core_profiler_core_profile;

	for(int i = 0; i < 256 ; i++) {
		core_profile->uinst[i].name = uinst_op_names[i];
		CLEAR_PROFILE_DATA(uinst[i]);
		core_profile->iseq_flush[i] = 1;
	}
	
	CLEAR_PROFILE_DATA(core_loop);
	
	CLEAR_PROFILE_DATA(isr);

	CLEAR_PROFILE_DATA(ior);
	CLEAR_PROFILE_DATA(ior_data);
	CLEAR_PROFILE_DATA(ior_irq);
	CLEAR_PROFILE_DATA(ior_rc);
	CLEAR_PROFILE_DATA(ior_rc_irq);
	CLEAR_PROFILE_DATA(ior_sreg);

	CLEAR_PROFILE_DATA(iow);
	CLEAR_PROFILE_DATA(iow_data);
	CLEAR_PROFILE_DATA(iow_irq);
	CLEAR_PROFILE_DATA(iow_wc);
	CLEAR_PROFILE_DATA(iow_wc_irq);
	CLEAR_PROFILE_DATA(iow_sreg);

	CLEAR_PROFILE_DATA(timer);
}

#endif /* #ifdef CONFIG_AVR_FAST_CORE_UINST_PROFILING */
#endif /* #ifndef __SIM_FAST_CORE_PROFILER_C */
