/*
	usbip_usb.h

	USB/IP server for the simavr USB device model.

	The simulated AVR is exported as a USB/IP device, so that a USB/IP
	client attaches it to the host's USB stack, e.g. on Linux, with the
	mainline vhci-hcd driver:

		modprobe vhci-hcd
		usbip list -r 127.0.0.1
		usbip attach -r 127.0.0.1 -b 1-1

	Everything runs in the simulation thread, from a cycle timer.

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

#ifndef __USBIP_USB_H__
#define __USBIP_USB_H__

#include <stdint.h>

struct avr_t;
struct usbip_usb_t;

#define USBIP_USB_DEFAULT_PORT	3240
#define USBIP_USB_BUSID			"1-1"

/*
 * Starts listening on addr:port and asserts VBUS, as if the device were
 * plugged into a powered port. addr NULL listens on 127.0.0.1 only,
 * "0.0.0.0" on all interfaces, which lets other machines attach the
 * device. Port 0 is the default 3240. Returns NULL if the socket can't
 * be set up. avr_terminate() closes the sockets and frees it.
 */
struct usbip_usb_t *
usbip_usb_init(
		struct avr_t * avr,
		const char * addr,
		uint16_t port);

#endif /* __USBIP_USB_H__ */
