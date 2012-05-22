/* vim: set sts=4:sw=4:ts=4:noexpandtab
	vhci_usb.c

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

/*
	this avrsim part is an usb connection between an usb AVR and the virtual
	host controller interface vhci-usb - making your sim-avr connect as a real
	usb device to the developer machine.

	You'll need vhci-usb and libusb_vhci to make it work.
	http://sourceforge.net/projects/usb-vhci/
*/

/* TODO iso endpoint support */

#include "vhci_usb.h"
#include "libusb_vhci.h"

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include "avr_usb.h"

static void
vhci_usb_attach_hook(
        struct avr_irq_t * irq,
        uint32_t value,
        void * param)
{
	struct vhci_usb_t * p = (struct vhci_usb_t*) param;
	p->attached = !!value;
	printf("avr attached: %d\n", p->attached);
}

struct usbsetup {
uint8_t reqtype; uint8_t req; uint16_t wValue; uint16_t wIndex; uint16_t wLength;
}__attribute__((__packed__));

struct _ep {
	uint8_t epnum;
	uint8_t epsz;
};

char * setuprequests[] =
	{ "GET_STATUS", "CLEAR_FEAT", "", "SET_FEAT", "", "SET_ADDR", "GET_DESCR",
	        "SET_DESCR", "GET_CONF", "SET_CONF" };

static int
control_read(
        struct vhci_usb_t * p,
        struct _ep * ep,
        uint8_t reqtype,
        uint8_t req,
        uint16_t wValue,
        uint16_t wIndex,
        uint16_t wLength,
        uint8_t * data)
{
	assert(reqtype&0x80);
	int ret;
	struct usbsetup buf =
		{ reqtype, req, wValue, wIndex, wLength };
	struct avr_io_usb pkt =
		{ ep->epnum, sizeof(struct usbsetup), (uint8_t*) &buf };

	avr_ioctl(p->avr, AVR_IOCTL_USB_SETUP, &pkt);

	pkt.sz = wLength;
	pkt.buf = data;
	while (wLength) {
		usleep(1000);
		ret = avr_ioctl(p->avr, AVR_IOCTL_USB_READ, &pkt);
		if (ret == AVR_IOCTL_USB_NAK) {
			printf(" NAK\n");
			usleep(50000);
			continue;
		}
		if (ret == AVR_IOCTL_USB_STALL) {
			printf(" STALL\n");
			return ret;
		}
		assert(ret==0);
		pkt.buf += pkt.sz;
		if (ep->epsz != pkt.sz)
			break;
		wLength -= pkt.sz;
		pkt.sz = wLength;
	}
	wLength = pkt.buf - data;

	usleep(1000);
	pkt.sz = 0;
	while ((ret = avr_ioctl(p->avr, AVR_IOCTL_USB_WRITE, &pkt))
	        == AVR_IOCTL_USB_NAK) {
		usleep(50000);
	}
	assert(ret==0);
	return wLength;
}

static int
control_write(
        struct vhci_usb_t * p,
        struct _ep * ep,
        uint8_t reqtype,
        uint8_t req,
        uint16_t wValue,
        uint16_t wIndex,
        uint16_t wLength,
        uint8_t * data)
{
	assert((reqtype&0x80)==0);
	int ret;
	struct usbsetup buf =
		{ reqtype, req, wValue, wIndex, wLength };
	struct avr_io_usb pkt =
		{ ep->epnum, sizeof(struct usbsetup), (uint8_t*) &buf };

	avr_ioctl(p->avr, AVR_IOCTL_USB_SETUP, &pkt);
	usleep(10000);

	if (wLength > 0) {
		pkt.sz = (wLength > ep->epsz ? ep->epsz : wLength);
		pkt.buf = data;
		while ((ret = avr_ioctl(p->avr, AVR_IOCTL_USB_WRITE, &pkt)) != 0) {
			if (ret == AVR_IOCTL_USB_NAK) {
				usleep(50000);
				continue;
			}
			if (ret == AVR_IOCTL_USB_STALL) {
				printf(" STALL\n");
				return ret;
			}
			assert(ret==0);
			if (pkt.sz != ep->epsz)
				break;
			pkt.buf += pkt.sz;
			wLength -= pkt.sz;
			pkt.sz = (wLength > ep->epsz ? ep->epsz : wLength);
		}
	}

	pkt.sz = 0;
	while ((ret = avr_ioctl(p->avr, AVR_IOCTL_USB_READ, &pkt))
	        == AVR_IOCTL_USB_NAK) {
		usleep(50000);
	}
	return ret;
}

static void
handle_status_change(
        struct vhci_usb_t * p,
        struct usb_vhci_port_stat*prev,
        struct usb_vhci_port_stat*curr)
{
	if (~prev->status & USB_VHCI_PORT_STAT_POWER
	        && curr->status & USB_VHCI_PORT_STAT_POWER) {
		avr_ioctl(p->avr, AVR_IOCTL_USB_VBUS, (void*) 1);
		if (p->attached) {
			if (usb_vhci_port_connect(p->fd, 1, USB_VHCI_DATA_RATE_FULL) < 0) {
				perror("port_connect");
				abort();
			}
		}
	}
	if (prev->status & USB_VHCI_PORT_STAT_POWER
	        && ~curr->status & USB_VHCI_PORT_STAT_POWER)
		avr_ioctl(p->avr, AVR_IOCTL_USB_VBUS, 0);

	if (curr->change & USB_VHCI_PORT_STAT_C_RESET
	        && ~curr->status & USB_VHCI_PORT_STAT_RESET
	        && curr->status & USB_VHCI_PORT_STAT_ENABLE) {
//         printf("END OF RESET\n");
	}
	if (~prev->status & USB_VHCI_PORT_STAT_RESET
	        && curr->status & USB_VHCI_PORT_STAT_RESET) {
		avr_ioctl(p->avr, AVR_IOCTL_USB_RESET, NULL);
		usleep(50000);
		if (curr->status & USB_VHCI_PORT_STAT_CONNECTION) {
			if (usb_vhci_port_reset_done(p->fd, 1, 1) < 0) {
				perror("reset_done");
				abort();
			}
		}
	}
	if (~prev->flags & USB_VHCI_PORT_STAT_FLAG_RESUMING
	        && curr->flags & USB_VHCI_PORT_STAT_FLAG_RESUMING) {
		printf("port resuming\n");
		if (curr->status & USB_VHCI_PORT_STAT_CONNECTION) {
			printf("  completing\n");
			if (usb_vhci_port_resumed(p->fd, 1) < 0) {
				perror("resumed");
				abort();
			}
		}
	}
	if (~prev->status & USB_VHCI_PORT_STAT_SUSPEND
	        && curr->status & USB_VHCI_PORT_STAT_SUSPEND)
		printf("port suspedning\n");
	if (prev->status & USB_VHCI_PORT_STAT_ENABLE
	        && ~curr->status & USB_VHCI_PORT_STAT_ENABLE)
		printf("port disabled\n");

	*prev = *curr;
}

static int
get_ep0_size(
		struct vhci_usb_t * p)
{
	struct _ep ep0 =
		{ 0, 8 };
	uint8_t data[8];

	int res = control_read(p, &ep0, 0x80, 6, 1 << 8, 0, 8, data);
	assert(res==8);
	return data[7];
}

static void
handle_ep0_control(
        struct vhci_usb_t * p,
        struct _ep * ep0,
        struct usb_vhci_urb * urb)
{
	int res;
	if (urb->bmRequestType &0x80) {
		res = control_read(p,ep0,
				urb->bmRequestType,
				urb->bRequest,
				urb->wValue,
				urb->wIndex,
				urb->wLength,
				urb->buffer);
			if (res>=0) {
				urb->buffer_actual=res;
				res=0;
			}
	}
	else
		res = control_write(p,ep0,
			urb->bmRequestType,
			urb->bRequest,
			urb->wValue,
			urb->wIndex,
			urb->wLength,
			urb->buffer);

	if (res==AVR_IOCTL_USB_STALL)
		urb->status = USB_VHCI_STATUS_STALL;
	else
		urb->status = USB_VHCI_STATUS_SUCCESS;
}

static void *
vhci_usb_thread(
		void * param)
{
	struct vhci_usb_t * p = (struct vhci_usb_t*) param;
	struct _ep ep0 =
		{ 0, 0 };
	struct usb_vhci_port_stat port_status;
	int id, busnum;
	char*busid;
	p->fd = usb_vhci_open(1, &id, &busnum, &busid);

	if (p->fd < 0) {
		perror("open vhci failed");
		printf("driver loaded, and access bits ok?\n");
		abort();
	}
	printf("Created virtual usb host with 1 port at %s (bus# %d)\n", busid,
	        busnum);
	memset(&port_status, 0, sizeof port_status);

	bool avrattached = false;

	for (unsigned cycle = 0;; cycle++) {
		struct usb_vhci_work wrk;

		int res = usb_vhci_fetch_work(p->fd, &wrk);

		if (p->attached != avrattached) {
			if (p->attached && port_status.status & USB_VHCI_PORT_STAT_POWER) {
				if (usb_vhci_port_connect(p->fd, 1, USB_VHCI_DATA_RATE_FULL)
				        < 0) {
					perror("port_connect");
					abort();
				}
			}
			if (!p->attached) {
				ep0.epsz = 0;
				//disconnect
			}
			avrattached = p->attached;
		}

		if (res < 0) {
			if (errno == ETIMEDOUT || errno == EINTR || errno == ENODATA)
				continue;
			perror("fetch work failed");
			abort();
		}

		switch (wrk.type) {
			case USB_VHCI_WORK_TYPE_PORT_STAT:
				handle_status_change(p, &port_status, &wrk.work.port_stat);
				break;
			case USB_VHCI_WORK_TYPE_PROCESS_URB:
				if (!ep0.epsz)
					ep0.epsz = get_ep0_size(p);

				wrk.work.urb.buffer = 0;
				wrk.work.urb.iso_packets = 0;
				if (wrk.work.urb.buffer_length)
					wrk.work.urb.buffer = malloc(wrk.work.urb.buffer_length);
				if (wrk.work.urb.packet_count)
					wrk.work.urb.iso_packets = malloc(
					        wrk.work.urb.packet_count
					                * sizeof(struct usb_vhci_iso_packet));
				if (res) {
					if (usb_vhci_fetch_data(p->fd, &wrk.work.urb) < 0) {
						if (errno != ECANCELED)
							perror("fetch_data");
						free(wrk.work.urb.buffer);
						free(wrk.work.urb.iso_packets);
						usb_vhci_giveback(p->fd, &wrk.work.urb);
						break;
					}
				}

				if (usb_vhci_is_control(wrk.work.urb.type)
				        && !(wrk.work.urb.epadr & 0x7f)) {
					handle_ep0_control(p, &ep0, &wrk.work.urb);

				} else {
					struct avr_io_usb pkt =
						{ wrk.work.urb.epadr, wrk.work.urb.buffer_actual,
						        wrk.work.urb.buffer };
					if (usb_vhci_is_out(wrk.work.urb.epadr))
						res = avr_ioctl(p->avr, AVR_IOCTL_USB_WRITE, &pkt);
					else {
						pkt.sz = wrk.work.urb.buffer_length;
						res = avr_ioctl(p->avr, AVR_IOCTL_USB_READ, &pkt);
						wrk.work.urb.buffer_actual = pkt.sz;
					}
					if (res == AVR_IOCTL_USB_STALL)
						wrk.work.urb.status = USB_VHCI_STATUS_STALL;
					else if (res == AVR_IOCTL_USB_NAK)
						wrk.work.urb.status = USB_VHCI_STATUS_TIMEDOUT;
					else
						wrk.work.urb.status = USB_VHCI_STATUS_SUCCESS;
				}
				if (usb_vhci_giveback(p->fd, &wrk.work.urb) < 0)
					perror("giveback");
				free(wrk.work.urb.buffer);
				free(wrk.work.urb.iso_packets);
				break;
			case USB_VHCI_WORK_TYPE_CANCEL_URB:
				printf("cancel urb\n");
				break;
			default:
				printf("illegal work type\n");
				abort();
		}

	}
}

void
vhci_usb_init(
		struct avr_t * avr,
		struct vhci_usb_t * p)
{
	p->avr = avr;
	pthread_t thread;

	pthread_create(&thread, NULL, vhci_usb_thread, p);

}

void
vhci_usb_connect(
		struct vhci_usb_t * p,
		char uart)
{
	avr_irq_t * t = avr_io_getirq(p->avr, AVR_IOCTL_USB_GETIRQ(),
	        USB_IRQ_ATTACH);
	avr_irq_register_notify(t, vhci_usb_attach_hook, p);
}

