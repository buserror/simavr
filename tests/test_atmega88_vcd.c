#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_vcd_file.h"

#include "tests.h"

int main(int argc, char **argv) {
	tests_init(argc, argv);


	avr_t *avr = tests_init_avr("atmega88_vcd.axf");

	avr_vcd_t vcd_file;
	avr_vcd_init(avr, "atmega88_vcd.vcd", &vcd_file, 10000);
	avr_vcd_add_signal(&vcd_file, avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), IOPORT_IRQ_PIN0), 1, "PB0" );
	avr_vcd_start(&vcd_file);

	tests_run_test(avr, 10000);

	avr_vcd_stop(&vcd_file);
	tests_success();
	return 0;
}
