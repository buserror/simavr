#ifndef __AYAB_DISPLAY_INIT__
#define __AYAB_DISPLAY_INIT__

#include "machine.h"
#include "shield.h"

void ayab_display(int argc, char *argv[], void *(*avr_run_thread)(void *), machine_t *machine, shield_t *shield);

#endif
