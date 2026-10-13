#include <stdio.h>
#include <stdlib.h>
#include "tests.h"
#include "sim_avr.h"
#include "sim_core.h"

static void write_io_reg(avr_t *avr, avr_io_addr_t addr, uint8_t v) {
	avr_io_addr_t io = AVR_DATA_TO_IO(addr);
	if (avr->io[io].w.c) {
		avr->io[io].w.c(avr, addr, v, avr->io[io].w.param);
	} else {
		avr_core_watch_write(avr, addr, v);
	}
}

int main(int argc, char **argv) {
	tests_init(argc, argv);

	avr_t * avr = avr_make_mcu_by_name("atmega324p");
	if (!avr) {
		fail("Failed to create atmega324p MCU");
	}
	avr_init(avr);

	// GTCCR address in data space (0x23 IO address -> 0x43 data address)
	avr_io_addr_t gtccr_addr = 0x23 + 0x20;

	// Initially GTCCR should be 0
	if (avr->data[gtccr_addr] != 0) {
		fail("GTCCR initial value expected 0, got 0x%02x", avr->data[gtccr_addr]);
	}

	// 1. Direct IO write test
	write_io_reg(avr, gtccr_addr, (1 << 1)); // PSRASY
	if ((avr->data[gtccr_addr] & (1 << 1)) != 0) {
		fail("GTCCR PSRASY bit was not cleared on direct write, got 0x%02x", avr->data[gtccr_addr]);
	}

	write_io_reg(avr, gtccr_addr, (1 << 0)); // PSRSYNC
	if ((avr->data[gtccr_addr] & (1 << 0)) != 0) {
		fail("GTCCR PSRSYNC bit was not cleared on direct write, got 0x%02x", avr->data[gtccr_addr]);
	}

	// 2. CPU OUT instruction execution test: "out 0x23, r16" (opcode 0xbd03)
	avr->flash[0] = 0x03;
	avr->flash[1] = 0xbd;
	avr->data[16] = (1 << 1); // r16 = PSRASY (0x02)
	avr->pc = 0;

	avr_run_one(avr);

	if ((avr->data[gtccr_addr] & (1 << 1)) != 0) {
		fail("GTCCR PSRASY bit was not cleared after OUT instruction, got 0x%02x", avr->data[gtccr_addr]);
	}

	// 3. Assembly sequence test: IN r24, 0x23; ORI r24, 2; OUT 0x23, r24; IN r25, 0x23
	avr->flash[0] = 0x83; avr->flash[1] = 0xb5; // in r24, 0x23
	avr->flash[2] = 0x82; avr->flash[3] = 0x60; // ori r24, 0x02
	avr->flash[4] = 0x83; avr->flash[5] = 0xbd; // out 0x23, r24
	avr->flash[6] = 0x93; avr->flash[7] = 0xb5; // in r25, 0x23

	avr->pc = 0;
	avr_run_one(avr); // IN r24, 0x23
	avr_run_one(avr); // ORI r24, 0x02
	avr_run_one(avr); // OUT 0x23, r24
	avr_run_one(avr); // IN r25, 0x23

	if (avr->data[25] != 0) {
		fail("GTCCR was read into r25 as 0x%02x after OUT 0x23, r24 (expected 0)", avr->data[25]);
	}

	tests_success();
	return 0;
}
