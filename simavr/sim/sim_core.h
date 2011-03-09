/*
	sim_core.h

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

#ifndef SIM_CORE_H_
#define SIM_CORE_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Instruction decoder, run ONE instruction
 */
uint16_t avr_run_one(avr_t * avr);

/*
 * These are for internal access to the stack (for interrupts)
 */
uint16_t _avr_sp_get(avr_t * avr);
void _avr_sp_set(avr_t * avr, uint16_t sp);
void _avr_push16(avr_t * avr, uint16_t v);

#if CONFIG_SIMAVR_TRACE

/*
 * Get a "pretty" register name
 */
const char * avr_regname(uint8_t reg);

/* 
 * DEBUG bits follow 
 * These will diseapear when gdb arrives
 */
void avr_dump_state(avr_t * avr);

#define DUMP_REG() { \
				for (int i = 0; i < 32; i++) printf("%s=%02x%c", avr_regname(i), avr->data[i],i==15?'\n':' ');\
				printf("\n");\
				uint16_t y = avr->data[R_YL] | (avr->data[R_YH]<<8);\
				for (int i = 0; i < 20; i++) printf("Y+%02d=%02x ", i, avr->data[y+i]);\
				printf("\n");\
		}


#if AVR_STACK_WATCH
#define DUMP_STACK() \
		for (int i = avr->trace_data->stack_frame_index; i; i--) {\
			int pci = i-1;\
			printf("\e[31m*** %04x: %-25s sp %04x\e[0m\n",\
					avr->trace_data->stack_frame[pci].pc, \
					avr->trace_data->codeline ? avr->trace_data->codeline[avr->trace_data->stack_frame[pci].pc>>1]->symbol : "unknown", \
							avr->trace_data->stack_frame[pci].sp);\
		}
#else
#define DUMP_STACK()
#endif

#define CRASH()  {\
		DUMP_REG();\
		printf("*** CYCLE %" PRI_avr_cycle_count "PC %04x\n", avr->cycle, avr->pc);\
		for (int i = OLD_PC_SIZE-1; i > 0; i--) {\
			int pci = (avr->trace_data->old_pci + i) & 0xf;\
			printf("\e[31m*** %04x: %-25s RESET -%d; sp %04x\e[0m\n",\
					avr->trace_data->old[pci].pc, avr->trace_data->codeline ? avr->trace_data->codeline[avr->trace_data->old[pci].pc>>1]->symbol : "unknown", OLD_PC_SIZE-i, avr->trace_data->old[pci].sp);\
		}\
		printf("Stack Ptr %04x/%04x = %d \n", _avr_sp_get(avr), avr->ramend, avr->ramend - _avr_sp_get(avr));\
		DUMP_STACK();\
		avr_sadly_crashed(avr, 0);\
	}
#else /* CONFIG_SIMAVR_TRACE */

#define CRASH() { \
		avr_sadly_crashed(avr, 0);\
	}
#define DUMP_STACK()
#define DUMP_REG();

#endif 

#ifdef __cplusplus
};
#endif

#endif /* SIM_CORE_H_ */
