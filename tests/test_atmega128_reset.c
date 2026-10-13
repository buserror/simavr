#include <stdlib.h>

#include "tests.h"

int main(int argc, char **argv) {
	avr_t *avr;

	tests_init(argc, argv);
	avr = avr_make_mcu_by_name("atmega128");
	if (!avr)
		fail("Could not create ATmega128");
	if (avr_init(avr))
		fail("Could not initialize ATmega128");

	avr_reset(avr);

	avr_terminate(avr);
	free(avr);
	tests_success();
	return 0;
}
