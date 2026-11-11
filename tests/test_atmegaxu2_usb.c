#include "test_usb_host.h"

/*
 * AT90USB82/162 and ATmega8U2/16U2/32U2: same USB controller, 176 bytes
 * of DPRAM, no VBUS pad. Only the memory sizes differ, so only one part of
 * each line is tested. The other entries need an atmegaXXX_usb.c firmware
 * file like atmega32u2_usb.c.
 */
static const struct usb_test parts[] = {
//	{ "at90usb82_usb.axf", "at90usb82", 0x1fff, 0x2ff, 0x1ff,
//		{ 0x1e, 0x93, 0x82 }, 0, 64, 0xc8 },
	{ "at90usb162_usb.axf", "at90usb162", 0x3fff, 0x2ff, 0x1ff,
		{ 0x1e, 0x94, 0x82 }, 0, 64, 0xc8 },
//	{ "atmega8u2_usb.axf", "atmega8u2", 0x1fff, 0x2ff, 0x1ff,
//		{ 0x1e, 0x93, 0x89 }, 0, 64, 0xc8 },
//	{ "atmega16u2_usb.axf", "atmega16u2", 0x3fff, 0x2ff, 0x1ff,
//		{ 0x1e, 0x94, 0x89 }, 0, 64, 0xc8 },
	{ "atmega32u2_usb.axf", "atmega32u2", 0x7fff, 0x4ff, 0x3ff,
		{ 0x1e, 0x95, 0x8a }, 0, 64, 0xc8 },
};

int main(int argc, char **argv) {
	tests_init(argc, argv);
	for (unsigned i = 0; i < sizeof(parts) / sizeof(parts[0]); i++)
		run_usb_test(&parts[i]);
	tests_success();
	return 0;
}
