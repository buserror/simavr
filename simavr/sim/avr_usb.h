/* vim: set sts=4:sw=4:ts=4:noexpandtab
	avr_usb.h

	Copyright 2012 Torbjorn Tyridal <ttyridal@gmail.com>

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

#ifndef __AVR_USB_H__
#define __AVR_USB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "sim_avr.h"

enum {
	USB_IRQ_ATTACH = 0,
	USB_IRQ_COUNT
};

/*
 * Host side interface. The code calling these plays the USB host,
 * see doc/Intro.html and tests/test_usb_host.h.
 *
 * WRITE, READ and SETUP take a struct avr_io_usb and return
 * AVR_IOCTL_USB_OK, AVR_IOCTL_USB_NAK (firmware not ready, run it and
 * retry), AVR_IOCTL_USB_STALL, or -1 for an endpoint the part does not
 * have. They NAK while the device is detached or the endpoint is not
 * configured.
 *   SETUP: 8 byte SETUP packet to a control endpoint.
 *   WRITE: OUT data, sz bytes. More than the endpoint size is
 *          truncated and flagged as an overflow to the firmware.
 *   READ:  IN data, sz is the buffer size in, the byte count out.
 *          A bulk endpoint with no data returns OK with sz 0, not NAK.
 *   A zero length READ or WRITE on EP0 is a control status stage.
 * RESET: bus reset, NULL parameter. -1 while detached.
 * VBUS:  VBUS level as the parameter, (void*)1 when the host powers the
 *        port, (void*)0 when it does not. It only changes registers on
 *        parts with a VBUS pad (see 'vbus' below).
 * GETIRQ: base of the USB_IRQ_* IRQs. USB_IRQ_ATTACH is raised with 1
 *        when the firmware connects to the bus, 0 when it disconnects.
 */
#define AVR_IOCTL_USB_WRITE AVR_IOCTL_DEF('u','s','b','w')
#define AVR_IOCTL_USB_READ AVR_IOCTL_DEF('u','s','b','r')
#define AVR_IOCTL_USB_SETUP AVR_IOCTL_DEF('u','s','b','s')
#define AVR_IOCTL_USB_RESET AVR_IOCTL_DEF('u','s','b','R')
#define AVR_IOCTL_USB_VBUS AVR_IOCTL_DEF('u','s','b','V')
#define AVR_IOCTL_USB_GETIRQ() AVR_IOCTL_DEF('u','s','b',' ')

struct avr_io_usb {
	uint8_t pipe;	//[in] endpoint number, the direction bit is ignored
	uint32_t  sz;		//[in/out]
	uint8_t * buf;	//[in/out]
};
#define AVR_IOCTL_USB_NAK -2
#define AVR_IOCTL_USB_STALL -3
#define AVR_IOCTL_USB_OK 0

// EPNUM is 3 bits wide, parts implement up to 7 of these
#define AVR_USB_MAX_ENDPOINTS	8

typedef struct avr_usb_t {
	avr_io_t	io;
	char name;
	avr_regbit_t	disabled;	// bit in the PRR
	avr_regbit_t	usbrf;		// bit in the MCUSR
	avr_io_addr_t	r_usbcon;	// every usb reg is an offset of this.
	avr_io_addr_t	r_pllcsr;


	uint8_t usb_com_vect;
	uint8_t usb_gen_vect;

	/*
	 * Endpoint geometry. Zero values select the AT90USB82/162 layout:
	 * 5 endpoints of up to 64 bytes and no DPRAM allocation check.
	 */
	uint8_t		ep_count;	// implemented endpoints, EP0..ep_count-1
	uint16_t	dpram_size;	// endpoint DPRAM bytes, 0 = not checked
	uint16_t	ep_size_max[AVR_USB_MAX_ENDPOINTS];	// bytes per bank
	uint8_t		ep_banks_max[AVR_USB_MAX_ENDPOINTS];	// 1 or 2

	/*
	 * VBUS pad (ATmega16U4/32U4). Leave reg 0 on parts without one,
	 * they attach as soon as DETACH is cleared.
	 */
	avr_regbit_t	vbus;		// USBSTA.VBUS
	avr_regbit_t	id;			// USBSTA.ID, reads as 1
	avr_regbit_t	vbusti;		// USBINT.VBUSTI
	avr_regbit_t	vbuste;		// USBCON.VBUSTE
	avr_regbit_t	otgpade;	// USBCON.OTGPADE

	struct usb_internal_state * state;
} avr_usb_t;

void avr_usb_init(avr_t * avr, avr_usb_t * port);

#ifdef __cplusplus
};
#endif

#endif /*__AVR_USB_H__*/
