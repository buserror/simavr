#include "test_usb_host.h"

/*
 * ATmega16U4/32U4: same USB controller, 832 bytes of DPRAM, VBUS pad.
 * Only the memory sizes differ, so only the 32U4 is tested. The 16U4
 * entry needs an atmega16u4_usb.c firmware file like atmega32u4_usb.c.
 */
static const struct usb_test parts[] = {
//	{ "atmega16u4_usb.axf", "atmega16u4", 0x3fff, 0x5ff, 0x1ff,
//		{ 0x1e, 0x94, 0x88 }, 1, 256, 0xfd },
	{ "atmega32u4_usb.axf", "atmega32u4", 0x7fff, 0xaff, 0x3ff,
		{ 0x1e, 0x95, 0x87 }, 1, 256, 0xfd },
};

int main(int argc, char **argv) {
	tests_init(argc, argv);
	for (unsigned i = 0; i < sizeof(parts) / sizeof(parts[0]); i++)
		run_usb_test(&parts[i]);
	tests_success();
	return 0;
}
