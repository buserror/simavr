
#include <string.h>

#include "tests.h"

/* Modified version of test_atmega88_example.c that uses a .hex file. */

int main(int argc, char **argv) {
	tests_init(argc, argv);

	elf_firmware_t fw = {0};
	strncpy(fw.mmcu, "atmega88", sizeof fw.mmcu);
	fw.frequency = 8 * 1000 * 1000;

	static const char *expected =
		"Read from eeprom 0xdeadbeef -- should be 0xdeadbeef\r\n"
		"Read from eeprom 0xcafef00d -- should be 0xcafef00d\r\n";
	tests_assert_uart_receive_fw(&fw, "atmega88_example.hex", 100000,
								 expected, '0');

	tests_success();
	return 0;
}
