/*
 * avr_run() stops the simulation at "rjmp .-2" or SLEEP with interrupts
 * off -- unless the watchdog is armed for a reset, which ends either.
 * Through avr_run() itself, since the tests' own loop has no such rule.
 */
#include <string.h>
#include "tests.h"

static char out[64];
static int nout;

static void usidr(struct avr_t *avr, avr_io_addr_t addr, uint8_t v, void *param)
{
	if (nout < (int)sizeof out - 1)
		out[nout++] = v;
}

int main(int argc, char **argv) {
	tests_init(argc, argv);
	avr_t *avr = tests_init_avr("attiny85_watchdog_spin.axf");
	avr_register_io_write(avr, 0x2f, usidr, NULL);

	int state = cpu_Running;
	for (long steps = 0; steps < 10000000 &&
			state != cpu_Done && state != cpu_Crashed; steps++)
		state = avr_run(avr);
	if (strcmp(out, "spin, sleep, done"))
		fail("expected \"spin, sleep, done\", got \"%s\"", out);
	if (state != cpu_Done)
		fail("the simulation did not end (state %d)", state);
	tests_success();
	return 0;
}
