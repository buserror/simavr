/*
	test_usb_host.h

	USB host side of the atmegaXXuX_usb tests, drives usb_test_fw.h
	through the avr_usb ioctls.

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

#include <string.h>
#include "tests.h"
#include "sim_time.h"
#include "avr_usb.h"

#define UDADDR_ADDR	0xe3
#define RETRIES		200

struct usb_test {
	const char *	elf;
	const char *	mmcu;
	uint16_t		flashend, ramend, e2end;
	uint8_t			signature[3];
	int				has_vbus;
	uint16_t		ep1_size;
	uint8_t			probe;
};

static int attached = -1;
static const char * part;

#define part_fail(s, ...) fail("%s: " s, part, ## __VA_ARGS__)

static void
attach_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	attached = value;
}

static void
run_us(avr_t * avr, uint32_t us)
{
	avr_cycle_count_t end = avr->cycle + avr_usec_to_cycles(avr, us);

	while (avr->cycle < end) {
		int state = avr_run(avr);
		if (state == cpu_Done || state == cpu_Crashed)
			part_fail("firmware stopped, cpu state %d", state);
	}
}

// ioctl retried while the device NAKs, gives the firmware time in between
static int
usb_io(avr_t * avr, uint32_t ctl, uint8_t pipe, uint8_t * buf, uint32_t * sz)
{
	struct avr_io_usb pkt = { pipe, *sz, buf };
	int ret;

	for (int i = 0; i < RETRIES; i++) {
		pkt.sz = *sz;
		ret = avr_ioctl(avr, ctl, &pkt);
		if (ret != AVR_IOCTL_USB_NAK)
			break;
		run_us(avr, 50);
	}
	*sz = pkt.sz;
	run_us(avr, 50);
	return ret;
}

static int
control_in(avr_t * avr, uint8_t type, uint8_t req, uint16_t value,
		uint8_t * data, uint16_t len)
{
	uint8_t setup[8] = { type, req, value, value >> 8, 0, 0, len, len >> 8 };
	uint32_t sz = 8;
	int ret;

	if ((ret = usb_io(avr, AVR_IOCTL_USB_SETUP, 0, setup, &sz)))
		part_fail("SETUP %02x/%02x: %d", type, req, ret);
	sz = len;
	ret = usb_io(avr, AVR_IOCTL_USB_READ, 0, data, &sz);
	if (ret)
		return ret;
	uint32_t got = sz;
	sz = 0;
	if ((ret = usb_io(avr, AVR_IOCTL_USB_WRITE, 0, NULL, &sz)))
		part_fail("status OUT %02x/%02x: %d", type, req, ret);
	return got;
}

static void
control_out(avr_t * avr, uint8_t type, uint8_t req, uint16_t value)
{
	uint8_t setup[8] = { type, req, value, value >> 8, 0, 0, 0, 0 };
	uint32_t sz = 8;
	int ret;

	if ((ret = usb_io(avr, AVR_IOCTL_USB_SETUP, 0, setup, &sz)))
		part_fail("SETUP %02x/%02x: %d", type, req, ret);
	sz = 0;
	if ((ret = usb_io(avr, AVR_IOCTL_USB_READ, 0, NULL, &sz)) || sz)
		part_fail("status IN %02x/%02x: %d, %u bytes", type, req, ret, sz);
}

static void
wait_attached(avr_t * avr, int want)
{
	for (int i = 0; i < 1000 && attached != want; i++)
		run_us(avr, 100);
	if (attached != want)
		part_fail("attach state %d, expected %d", attached, want);
}

static void
run_usb_test(const struct usb_test * t)
{
	avr_t * avr;
	uint8_t data[256], echo[256];
	uint32_t sz;
	int ret;

	part = t->mmcu;
	avr = tests_init_avr(t->elf);
	if (!avr)
		part_fail("cannot load");
	if (strcmp(avr->mmcu, t->mmcu))
		part_fail("core %s", avr->mmcu);
	if (avr->flashend != t->flashend || avr->ramend != t->ramend ||
			avr->e2end != t->e2end)
		part_fail("flashend %x ramend %x e2end %x",
				avr->flashend, avr->ramend, avr->e2end);
	if (memcmp(avr->signature, t->signature, 3))
		part_fail("signature %02x %02x %02x", avr->signature[0],
				avr->signature[1], avr->signature[2]);

	attached = -1;
	avr_irq_register_notify(
			avr_io_getirq(avr, AVR_IOCTL_USB_GETIRQ(), USB_IRQ_ATTACH),
			attach_hook, NULL);

	// No VBUS yet: a part with a VBUS pad must stay off the bus
	run_us(avr, 2000);
	if (t->has_vbus && attached == 1)
		part_fail("attached without VBUS");
	avr_ioctl(avr, AVR_IOCTL_USB_VBUS, (void*)1);
	wait_attached(avr, 1);

	sz = 8;
	if (avr_ioctl(avr, AVR_IOCTL_USB_RESET, NULL))
		part_fail("bus reset refused");
	run_us(avr, 200);

	ret = control_in(avr, 0x80, 6, 0x0100, data, 18);
	if (ret != 18 || data[0] != 18 || data[1] != 1 || data[7] != 64)
		part_fail("device descriptor: %d bytes, %02x %02x %02x", ret,
				data[0], data[1], data[7]);

	control_out(avr, 0x00, 5, 9);
	if (avr->data[UDADDR_ADDR] != 0x89)
		part_fail("UDADDR %02x, expected 89", avr->data[UDADDR_ADDR]);

	ret = control_in(avr, 0xc0, 1, 0, data, 1);
	if (ret != 1 || data[0] != t->probe)
		part_fail("endpoint probe %02x, expected %02x (%d)", data[0], t->probe, ret);

	// unknown request, the data stage must STALL
	ret = control_in(avr, 0x80, 6, 0x0300, data, 4);
	if (ret != AVR_IOCTL_USB_STALL)
		part_fail("GET_DESCRIPTOR(string) not stalled: %d", ret);

	control_out(avr, 0x00, 9, 1);

	for (int i = 0; i < t->ep1_size; i++)
		data[i] = i * 7 + 1;
	sz = t->ep1_size;
	if ((ret = usb_io(avr, AVR_IOCTL_USB_WRITE, 1, data, &sz)))
		part_fail("EP1 OUT: %d", ret);

	uint32_t got = 0;
	for (int i = 0; i < RETRIES && got < t->ep1_size; i++) {
		sz = 64;
		ret = usb_io(avr, AVR_IOCTL_USB_READ, 0x82, echo + got, &sz);
		if (ret)
			part_fail("EP2 IN: %d", ret);
		got += sz;
	}
	if (got != t->ep1_size)
		part_fail("EP2 IN: %u bytes, expected %u", got, t->ep1_size);
	for (int i = 0; i < t->ep1_size; i++)
		if (echo[i] != (data[i] ^ 0x5a))
			part_fail("echo byte %d: %02x, expected %02x", i, echo[i],
					data[i] ^ 0x5a);

	// endpoints past the last one are refused
	sz = 0;
	if (avr_ioctl(avr, AVR_IOCTL_USB_READ,
			&(struct avr_io_usb){ t->has_vbus ? 7 : 5, 0, data }) != -1)
		part_fail("access to a missing endpoint accepted");

	if (t->has_vbus) {
		avr_ioctl(avr, AVR_IOCTL_USB_VBUS, (void*)0);
		wait_attached(avr, 0);
	}
	avr_terminate(avr);
}
