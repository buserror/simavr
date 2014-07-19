#include <stdlib.h>
#include <string.h>
#include "tests.h"

static int success = 1;

static void
notify(
		avr_t * avr,
		avr_cycle_counter_t * counter,
		avr_cycle_count_t cycles,
		void * param)
{
	if(strcmp(counter->name, "Outer") == 0 && cycles != 60 && counter->overhead != 31)
		success = 0;

	if(strcmp(counter->name, "Inner") == 0 && cycles != 3 && counter->overhead != 1)
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
