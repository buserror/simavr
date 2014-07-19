#include <stdlib.h>
#include <string.h>
#include "tests.h"

enum {
	OUTER_CYCLE_COUNTER,
	INNER_CYCLE_COUNTER,
};

static int success = 1;

static void
notify(
		avr_t * avr,
		avr_cycle_counter_t * counter,
		avr_cycle_count_t cycles,
		void * param)
{
	if(counter->id == OUTER_CYCLE_COUNTER && cycles != 60 && counter->overhead != 31)
		success = 0;

	if(counter->id == INNER_CYCLE_COUNTER && cycles != 3 && counter->overhead != 1)
		success = 0;
}

int main(int argc, char **argv)
{
	avr_t * avr;

	tests_init(argc, argv);

	avr = tests_init_avr("atmega88_cycle_counters.axf");
	avr_cycle_counter_register_notify(avr, &notify, NULL);
	tests_run_test(avr, 1000);

	if(success)
		tests_success();

	return 0;
}
