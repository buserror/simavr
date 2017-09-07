/* vim: set sts=4:sw=4:ts=4:noexpandtab
	usbip.c

	Copyright 2017 Torbjorn Tyridal <ttyridal@gmail.com>

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


	This code is heavily inspired by
	https://github.com/lcgamboa/USBIP-Virtual-USB-Device
	Copyright (c) : 2016  Luis Claudio GambÃ´a Lopes
*/

/*
	this avrsim part will expose your usb-avr as a usbip server.
	To connect it to the host system load modules usbip-core and vhci-hcd,
	then:
	~# usbip -a 127.0.0.1 1-1
*/

#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define USBIP_PORT_NUM 3240

#include "usb_types.h"
#include "usbip_types.h"

#include "avr_usb.h"

#define min(a,b) ((a)<(b)?(a):(b))

struct usbip_t {
	struct avr_t * avr;
	bool attached;
	bool udev_valid;
	struct usbip_usb_device udev;
};

static ssize_t
avr_usb_read(
		const struct usbip_t * p,
		unsigned int ep,
		void * buf,
		size_t blen)
{
	byte * b = buf;
	struct avr_io_usb pkt = { ep, blen, b };
	if (buf) {
		while (blen) {
			switch (avr_ioctl(p->avr, AVR_IOCTL_USB_READ, &pkt)) {
				case AVR_IOCTL_USB_NAK:
					if (pkt.buf - b == 0) return -1;
					else return pkt.buf - b;
				case AVR_IOCTL_USB_STALL:
					printf(" STALL (read)\n");
					return -1;
				case 0:
					if (!pkt.sz)
						blen = 0;
					break;
				default:
					fprintf(stderr, "Unknown avr_ioctl return value\n");
					abort();
			}
			pkt.buf += pkt.sz & 0xff;
			blen -= pkt.sz & 0xff;
			if (pkt.sz & 0xf00) break;
			pkt.sz = blen;
		}
		return pkt.buf - b;
	} else {
		int ret;
		while ((ret = avr_ioctl(p->avr, AVR_IOCTL_USB_READ, &pkt))
				== AVR_IOCTL_USB_NAK) {
			usleep(50000);
		}
		return 0;
	}
}

static int
avr_usb_write(
		const struct usbip_t * p,
		unsigned int ep,
		void * buf,
		size_t blen)
{
	struct avr_io_usb pkt = { ep, blen, buf };
	do {
		switch (avr_ioctl(p->avr, AVR_IOCTL_USB_WRITE, &pkt)) {
			case 0: break;
			case AVR_IOCTL_USB_NAK:
				if (buf) return -1;
				usleep(50000);
				continue;
			case AVR_IOCTL_USB_STALL:
				printf(" STALL (write)\n");
				return -1;
			default:
				fprintf(stderr, "Unknown avr_ioctl return\n");
				abort();
		}
		pkt.buf += pkt.sz;
		blen -= pkt.sz;
		pkt.sz = blen;
		if (!blen) break;
	} while (1);
	return 0;
}

static int
control_read(
		const struct usbip_t * p,
		byte reqtype,
		byte req,
		word wValue,
		word wIndex,
		word wLength,
		byte * data)
{
	int ret;
	struct usb_setup_pkt buf = {
		reqtype | USB_REQTYPE_DIR_DEV_TO_HOST,
		req,
		wValue,
		wIndex,
		wLength };
	const unsigned int ctrlep = 0;
	struct avr_io_usb pkt = { ctrlep, sizeof buf, (uint8_t*) &buf };

	printf("ctrl_read typ:%d req:%d val:%04x  len:%d bytes\n", reqtype, req, wValue, wLength);
	avr_ioctl(p->avr, AVR_IOCTL_USB_SETUP, &pkt);

	ret = avr_usb_read(p, ctrlep, data, wLength);

	avr_usb_write(p, ctrlep, NULL, 0);

	return ret;
}

static bool
get_descriptor(
		const struct usbip_t * p,
		unsigned char descr_type,
		void * buf,
		size_t length)
{
	const unsigned char descr_index = 0;
	return control_read(p,
			USB_REQTYPE_STD + USB_REQTYPE_DEVICE,
			USB_REQUEST_GET_DESCRIPTOR,
			descr_type << 8 | descr_index,
			0,
			length,
			buf) == (int)length;
}

static bool
load_device_and_config_descriptor(
		struct usbip_t * p)
{
	struct usb_device_descriptor dd;
	struct usb_configuration_descriptor cd;
	if (!get_descriptor(p, USB_DESCRIPTOR_DEVICE, &dd, sizeof dd)) {
		fprintf(stderr, "get device descriptor failed\n");
		p->udev_valid = false;
		return false;
	}

	if (!get_descriptor(p, USB_DESCRIPTOR_CONFIGURATION, &cd, sizeof cd)) {
		fprintf(stderr, "get configuration descriptor failed\n");
		p->udev_valid = false;
		return false;
	}

	strcpy(p->udev.path, "/sys/devices/pci0000:00/0000:00:01.2/usb1/1-1");
	strcpy(p->udev.busid, "1-1");
	p->udev.busnum = htonl(1);
	p->udev.devnum = htonl(2);
	p->udev.speed = htonl(2);

	p->udev.idVendor = htons(dd.idVendor);
	p->udev.idProduct = htons(dd.idProduct);
	p->udev.bcdDevice = htons(dd.bcdDevice);

	p->udev.bDeviceClass = dd.bDeviceClass;
	p->udev.bDeviceSubClass = dd.bDeviceSubClass;
	p->udev.bDeviceProtocol = dd.bDeviceProtocol;
	p->udev.bConfigurationValue = cd.bConfigurationValue;
	p->udev.bNumConfigurations = dd.bNumConfigurations;
	p->udev.bNumInterfaces = cd.bNumInterfaces;

	p->udev_valid = true;
	return true;
}


static void
vhci_usb_attach_hook(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	struct usbip_t * p = param;
	p->attached = !!value;
	printf("avr attached: %d\n", p->attached);

	avr_ioctl(p->avr, AVR_IOCTL_USB_VBUS, (void*) 1);

	(void)irq;
}

static int sock_read_exact(
		int sockfd,
		void * buf,
		size_t n)
{
	while (n != 0) {
		ssize_t nb = recv(sockfd, buf, n, 0);
		if (nb == 0) {
			fprintf(stderr, "disconnect\n");
			return 1;
		}
		if (nb < 0) {
			perror("sock_read_exact");
			return -1;
		}
		n -= nb;
	}
	return 0;
}

static int
open_usbip_socket(void)
{
	struct sockaddr_in serv;
	int listenfd;

	if ((listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket error");
		exit (1);
	};

	int reuse = 1;
	if (setsockopt(
			listenfd,
			SOL_SOCKET,
			SO_REUSEADDR,
			(const char*)&reuse,
			sizeof(reuse)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");

	memset(&serv, 0, sizeof serv);
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = htonl(INADDR_ANY);
	serv.sin_port = htons(USBIP_PORT_NUM);

	if (bind(listenfd, (struct sockaddr *)&serv, sizeof serv) < 0) {
		perror("bind error");
		exit (1);
	}

	if (listen(listenfd, SOMAXCONN) < 0) {
		perror("listen error");
		exit (1);
	}

	return listenfd;
}


#define sock_send(sockfd, dta, dta_sz) if(send(sockfd, dta, dta_sz, 0) != dta_sz) { perror("sock send"); break; }
static void
handle_usbip_req_devlist(
		int sockfd,
		struct usbip_t * p)
{
	struct usbip_op_common op_common = {
		htons(USBIP_PROTO_VERSION),
		htons(USBIP_OP_REPLY | USBIP_OP_DEVLIST),
		htonl(USBIP_ST_NA)
	};
	struct usbip_op_devlist_reply devlist = {htonl(1)};


	avr_ioctl(p->avr, AVR_IOCTL_USB_RESET, NULL);
	usleep(2500);

	load_device_and_config_descriptor(p);

	// get config AND interface descriptors
	struct {
		struct usb_configuration_descriptor config;
		struct usb_interface_descriptor interf[p->udev.bNumInterfaces];
	} cd;
	if (!get_descriptor(p, USB_DESCRIPTOR_CONFIGURATION, &cd, sizeof cd)) {
		fprintf(stderr, "get configuration descriptor failed\n");
		if (send(sockfd, &op_common, sizeof op_common, 0) != sizeof op_common)
			perror("sock send");
		return;
	}

	ssize_t devinfo_sz = sizeof (struct usbip_usb_device) +
						 p->udev.bNumInterfaces * sizeof (struct usbip_usb_interface);
	struct usbip_op_devlist_reply_extra * devinfo = malloc(devinfo_sz);

	devinfo->udev = p->udev;

	for (byte i=0; i < devinfo->udev.bNumInterfaces; i++) {
		devinfo->uinf[i].bInterfaceClass = cd.interf[i].bInterfaceClass;
		devinfo->uinf[i].bInterfaceSubClass = cd.interf[i].bInterfaceSubClass;
		devinfo->uinf[i].bInterfaceProtocol = cd.interf[i].bInterfaceProtocol;
		devinfo->uinf[i].padding = 0;
	}

	op_common.status = htonl(USBIP_ST_OK);

	do {
		sock_send(sockfd, &op_common, sizeof op_common)
		sock_send(sockfd, &devlist, sizeof devlist)
		sock_send(sockfd, devinfo, devinfo_sz)
	} while (0);

	free(devinfo);
}

static int
handle_usbip_detached_state(
		int sockfd,
		struct usbip_t * p)
{
	struct usbip_op_common op_common;
	if (sock_read_exact(sockfd, &op_common, sizeof op_common))
		return -1;
	unsigned version = ntohs(op_common.version);
	unsigned op_code = ntohs(op_common.code);

	if (version != USBIP_PROTO_VERSION) {
		fprintf(stderr, "Protocol version mismatch, request: %x, this: %x\n",
				version, USBIP_PROTO_VERSION);
		return -1;
	}
	printf("executing %d\n", op_code);
	switch (op_code) {
		case USBIP_OP_REQUEST | USBIP_OP_DEVLIST:
			handle_usbip_req_devlist(sockfd, p);
			break;
		case USBIP_OP_REQUEST | USBIP_OP_IMPORT: {
			struct usbip_op_import_request req;
			if (recv(sockfd, &req, sizeof req, 0) != sizeof req) {
				fprintf(stderr, "protocol vialation\n");
				return -1;
			}

			avr_ioctl(p->avr, AVR_IOCTL_USB_RESET, NULL);
			usleep(2500);


			if (!load_device_and_config_descriptor(p)) {
				printf("Failed load_Decice_And_config while attach\n");
				op_common.status = USBIP_ST_NA;
				if (send(sockfd, &op_common, sizeof op_common, 0) != sizeof op_common)
					perror("sock send");
			} else {
				op_common.code = htons(USBIP_OP_REPLY | USBIP_OP_IMPORT),
				op_common.status = USBIP_ST_OK;
				struct usbip_op_import_reply reply = { p->udev };
				do {
					sock_send(sockfd, &op_common, sizeof op_common)
					sock_send(sockfd, &reply, sizeof reply)
				} while(0);
			}
			printf("Attached to usbip client\n");
			return 1;
		}
		default:
			fprintf(stderr, "Unknown usbip %s %x\n",
					op_code & USBIP_OP_REQUEST ? "request" : "reply",
					op_code & 0xff);
			return -1;
	}
	return 0;
}

static void
handle_usbip_connection(
		int sockfd,
		struct usbip_t * p)
{
	bool attached = false;
	while (1) {
		if (attached) {
			struct usbip_header cmd;
			if (sock_read_exact(sockfd, &cmd, sizeof cmd.hdr))
				return;

			byte ep = ntohl(cmd.hdr.ep);
			int cmdnum = ntohl(cmd.hdr.command);
			int direction = ntohl(cmd.hdr.direction);

			switch (cmdnum) {
				case USBIP_CMD_SUBMIT: {
					if (sock_read_exact(sockfd, &cmd.u.submit, sizeof cmd.u.submit))
						return;
					ssize_t bl = ntohl(cmd.u.submit.transfer_buffer_length);
					byte buf[bl];

					if (ep == 0) {
						struct avr_io_usb pkt = { ep, sizeof cmd.u.submit.setup, cmd.u.submit.setup };
						if (avr_ioctl(p->avr, AVR_IOCTL_USB_SETUP, &pkt)) {
							printf("FATAL: SETUP packet failed!\n");
						}
					}

					if (direction == USBIP_DIR_IN) {
						if (bl) {
							bl = avr_usb_read(p, ep, buf, bl);
							if (ep==4 && bl < 0) bl = 0; //teensy & linux hack?
						}
						if (ep == 0)
							avr_usb_write(p, ep, NULL, 0);
					} else {
						if (bl && sock_read_exact(sockfd, buf, bl))
							return;
						if (bl)
							bl = avr_usb_write(p, ep, buf, bl);
						if (ep == 0)
							avr_usb_read(p, ep, NULL, 0);
					}


					struct usbip_header ret;
					memset(&ret, 0, sizeof ret);
					ret.hdr.command = htonl(USBIP_RET_SUBMIT);
					ret.hdr.seqnum = cmd.hdr.seqnum;
					ret.u.retsubmit.status = htonl(bl < 0 ? USBIP_ST_NA : USBIP_ST_OK);
					ret.u.retsubmit.actual_length = htonl(bl < 0 ? 0 : bl);
					ret.u.retsubmit.start_frame = 0;
					ret.u.retsubmit.number_of_packets = 0;
					ret.u.retsubmit.error_count = 0;
					memcpy(&ret.u.retsubmit.setup, cmd.u.submit.setup, 8);

//					 printf("return %zd bytes\n", bl);
					if (send(sockfd, &ret, sizeof ret.hdr + sizeof ret.u.retsubmit, 0) != sizeof ret.hdr + sizeof ret.u.retsubmit)
						perror("sock send");
					if (bl>0 && send(sockfd, buf, bl, 0) != bl)
						perror("sock send");

//					 printf("done\n");


					break;
				}
				case USBIP_CMD_UNLINK:
					if (recv(sockfd, &cmd.u.unlink, sizeof cmd.u.unlink, 0) != sizeof cmd.u.unlink) {
						perror("sock_recv");
						return;
					}
					break;
				default:
					fprintf(stderr, "protocol vialation, unknown command %x\n", ntohl(cmd.hdr.command));

			}

		} else {
			switch (handle_usbip_detached_state(sockfd, p)) {
				case -1: return;
				case 1:
					attached = true;
				default:
				break;
			}
		}
	}
}

void *
usbip_main(
		struct usbip_t * p)
{
	int listenfd = open_usbip_socket();
	struct sockaddr_in cli;
	unsigned int clilen = sizeof(cli);

	while (1) {
		int sockfd = accept(listenfd, (struct sockaddr *)&cli,  &clilen);

		if ( sockfd < 0) {
			printf ("accept error : %s \n", strerror (errno));
			exit (1);
		}
		fprintf(stderr, "Connection address:%s\n",inet_ntoa(cli.sin_addr));
		handle_usbip_connection(sockfd, p);
		close(sockfd);
	}

(void)p;
	return NULL;
}


struct usbip_t *
usbip_create(
		struct avr_t * avr)
{
	struct usbip_t * p = malloc(sizeof *p);
	memset(p, 0, sizeof *p);
	p->avr = avr;

	avr_irq_t * t = avr_io_getirq(p->avr, AVR_IOCTL_USB_GETIRQ(), USB_IRQ_ATTACH);
	avr_irq_register_notify(t, vhci_usb_attach_hook, p);

	return p;
}

void
usbip_destroy(
		void * p)
{
	free(p);
}
#undef sock_send
