/*
 * history_avr.h
 *
 *  Created on: 18 Oct 2015
 *      Author: michel
 */

#ifndef _HISTORY_AVR_H_
#define _HISTORY_AVR_H_

#include "sim_avr.h"
#include "sim_elf.h"
#include "history.h"
#include "history_cmd.h"

extern elf_firmware_t code;

extern avr_t * avr;

extern int history_redisplay;

void history_avr_init();
void history_avr_idle();

#endif /* _HISTORY_AVR_H_ */
