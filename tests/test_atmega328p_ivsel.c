/*
	test_atmega328p_ivsel.c

	IVSEL and the boot section: the bootloader's fuses give its reset
	vector, and its interrupt goes through its own vectors, whether the
	reset (BOOTRST) or the application started it.

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

#include "tests.h"
#include "sim_avr.h"
#include "sim_elf.h"

static avr_t *
load(void)
{
	avr_t * avr = tests_init_avr("atmega328p_ivsel.axf");
	elf_firmware_t fw = { 0 };

	if (elf_read_firmware("atmega328p_ivsel_boot.axf", &fw))
		fail("can't load the bootloader");
	avr_load_firmware(avr, &fw);
	return avr;
}

int main(int argc, char **argv) {
	avr_t * avr;

	tests_init(argc, argv);

	avr = load();
	if (avr->pc != 0x7800)
		fail("reset at %04x, the bootloader's fuses give 7800", avr->pc);
	tests_assert_uart_receive_avr(avr, 100000, "BbAa\n", '0');

	avr = load();
	avr->fuse[AVR_FUSE_HIGH] |= AVR_FUSE_BOOTRST;	// unprogrammed
	avr_reset(avr);
	tests_assert_uart_receive_avr(avr, 100000, "ABbAa\n", '0');

	tests_success();
	return 0;
}
