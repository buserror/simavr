/*
	test_boot_section.c

	Boot section start and reset vector from BOOTSZ and BOOTRST, against
	the datasheets' "Boot Size Configuration" tables.

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

static const struct {
	const char *	mmcu;
	uint8_t			fuse;		// with BOOTRST and BOOTSZ
	uint32_t		start[4];	// for BOOTSZ = 00, 01, 10, 11
} parts[] = {
	{ "atmega88",	AVR_FUSE_EXT, { 0x1800, 0x1c00, 0x1e00, 0x1f00 } },
	{ "atmega168",	AVR_FUSE_EXT, { 0x3800, 0x3c00, 0x3e00, 0x3f00 } },
	{ "atmega328p",	AVR_FUSE_HIGH, { 0x7000, 0x7800, 0x7c00, 0x7e00 } },
	{ "atmega328pb", AVR_FUSE_HIGH, { 0x7000, 0x7800, 0x7c00, 0x7e00 } },
	{ "at90usb82",	AVR_FUSE_HIGH, { 0x1000, 0x1800, 0x1c00, 0x1e00 } },
	{ "atmega8u2",	AVR_FUSE_HIGH, { 0x1000, 0x1800, 0x1c00, 0x1e00 } },
	{ "at90usb162",	AVR_FUSE_HIGH, { 0x3000, 0x3800, 0x3c00, 0x3e00 } },
	{ "atmega16u2",	AVR_FUSE_HIGH, { 0x3000, 0x3800, 0x3c00, 0x3e00 } },
	{ "atmega32u2",	AVR_FUSE_HIGH, { 0x7000, 0x7800, 0x7c00, 0x7e00 } },
	{ "atmega16u4",	AVR_FUSE_HIGH, { 0x3000, 0x3800, 0x3c00, 0x3e00 } },
	{ "atmega32u4",	AVR_FUSE_HIGH, { 0x7000, 0x7800, 0x7c00, 0x7e00 } },
	{ "atmega2560",	AVR_FUSE_HIGH, { 0x3e000, 0x3f000, 0x3f800, 0x3fc00 } },
	{ "atmega8",	AVR_FUSE_HIGH, { 0x1800, 0x1c00, 0x1e00, 0x1f00 } },
	{ "atmega16",	AVR_FUSE_HIGH, { 0x3800, 0x3c00, 0x3e00, 0x3f00 } },
	{ "atmega32",	AVR_FUSE_HIGH, { 0x7000, 0x7800, 0x7c00, 0x7e00 } },
	{ "atmega164",	AVR_FUSE_HIGH, { 0x3800, 0x3c00, 0x3e00, 0x3f00 } },
	{ "atmega324",	AVR_FUSE_HIGH, { 0x7000, 0x7800, 0x7c00, 0x7e00 } },
	{ "atmega644",	AVR_FUSE_HIGH, { 0xe000, 0xf000, 0xf800, 0xfc00 } },
	{ "atmega1284",	AVR_FUSE_HIGH, { 0x1e000, 0x1f000, 0x1f800, 0x1fc00 } },
	{ "atmega128",	AVR_FUSE_HIGH, { 0x1e000, 0x1f000, 0x1f800, 0x1fc00 } },
	{ "atmega1280",	AVR_FUSE_HIGH, { 0x1e000, 0x1f000, 0x1f800, 0x1fc00 } },
	{ "atmega1281",	AVR_FUSE_HIGH, { 0x1e000, 0x1f000, 0x1f800, 0x1fc00 } },
	{ "atmega128rfa1", AVR_FUSE_HIGH, { 0x1e000, 0x1f000, 0x1f800, 0x1fc00 } },
	{ "atmega128rfr2", AVR_FUSE_HIGH, { 0x1e000, 0x1f000, 0x1f800, 0x1fc00 } },
	{ "atmega16m1",	AVR_FUSE_HIGH, { 0x3000, 0x3800, 0x3c00, 0x3e00 } },
	{ "atmega64m1",	AVR_FUSE_HIGH, { 0xe000, 0xf000, 0xf800, 0xfc00 } },
};

int main(int argc, char **argv) {
	tests_init(argc, argv);
	for (int i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
		avr_t * avr = avr_make_mcu_by_name(parts[i].mmcu);
		avr_init(avr);
		if (avr->pc)	// the default fuses leave BOOTRST unprogrammed
			fail("%s: reset at %04x", parts[i].mmcu, avr->pc);
		for (int bootsz = 0; bootsz < 4; bootsz++)
			for (int bootrst = 0; bootrst < 2; bootrst++) {	// 0 is programmed
				uint8_t * fuse = &avr->fuse[parts[i].fuse];
				*fuse &= ~(AVR_FUSE_BOOTRST | AVR_FUSE_BOOTSZ);
				*fuse |= bootsz * AVR_FUSE_BOOTSZ0;
				if (bootrst)
					*fuse |= AVR_FUSE_BOOTRST;
				avr_reset(avr);
				uint32_t start = parts[i].start[bootsz];
				if (avr->boot_start != start || avr->pc != (bootrst ? 0 : start))
					fail("%s: BOOTSZ %d BOOTRST %d, boot section %05x, reset at %05x",
							parts[i].mmcu, bootsz, bootrst, avr->boot_start, avr->pc);
			}
		avr_terminate(avr);
	}
	tests_success();
	return 0;
}
