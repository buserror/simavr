/*
	usbip_usb.c

	USB/IP server for the simavr USB device model, see usbip_usb.h.

	Protocol: Documentation/usb/usbip_protocol.rst in the Linux kernel
	tree, version 1.1.1. All fields are big endian.

	The part plays the server side: it answers OP_REQ_DEVLIST and
	OP_REQ_IMPORT, then turns USBIP_CMD_SUBMIT requests into the
	AVR_IOCTL_USB_* calls of avr_usb.c. Requests the firmware NAKs stay
	queued and are retried on the next tick. Like the usbip-host driver
	of a real server, the part enumerates the device itself when it
	attaches (to report it in OP_REP_DEVLIST and to learn its endpoint
	sizes), and the client's SET_ADDRESS never reaches the device.

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "sim_avr.h"
#include "sim_time.h"
#include "sim_network.h"
#include "avr_usb.h"
#include "usbip_usb.h"

/*
 * Sockets are blocking, and only read or accepted from once select()
 * says they are ready, which works the same with POSIX and Winsock.
 */
#ifdef __MINGW32__
#define sock_close(s)	closesocket(s)
#define sock_error()	WSAGetLastError()
#define SOCK_EINTR		WSAEINTR
#else
#include <sys/select.h>
#define sock_close(s)	close(s)
#define sock_error()	errno
#define SOCK_EINTR		EINTR
#endif

// macOS has no MSG_NOSIGNAL, SO_NOSIGPIPE is set on the socket instead
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL	0
#endif

#define TICK_US			100		// socket and URB polling period
#define STEPS_PER_TICK	64		// IOCTLs per tick, at most

#define USBIP_VERSION	0x0111
#define OP_REQ_DEVLIST	0x8005
#define OP_REP_DEVLIST	0x0005
#define OP_REQ_IMPORT	0x8003
#define OP_REP_IMPORT	0x0003
#define ST_OK			0
#define ST_NA			1

#define USBIP_CMD_SUBMIT	1
#define USBIP_CMD_UNLINK	2
#define USBIP_RET_SUBMIT	3
#define USBIP_RET_UNLINK	4
#define USBIP_HDR_SIZE		48
#define USBIP_DIR_IN		1

#define URB_ZERO_PACKET		0x0040

// URB status codes are Linux errno values, whatever the host's are
#define USBIP_EINVAL		22
#define USBIP_EPIPE			32
#define USBIP_ECONNRESET	104

#define USB_SPEED_FULL		2
#define BUSNUM				1
#define DEVNUM				1

enum {
	DEV_DETACHED = 0,
	DEV_ENUMERATING,
	DEV_READY,
};

enum {
	XFER_CTL_SETUP = 0,
	XFER_CTL_DATA,
	XFER_CTL_STATUS,
	XFER_DATA,
	XFER_ZLP,
};

// enumeration steps, done with internal URBs
enum {
	ENUM_NONE = 0,
	ENUM_DEV_HEAD,		// GET_DESCRIPTOR(device), 8 bytes, gives bMaxPacketSize0
	ENUM_SET_ADDRESS,
	ENUM_DEV,			// GET_DESCRIPTOR(device)
	ENUM_CFG_HEAD,		// GET_DESCRIPTOR(configuration), 9 bytes, gives wTotalLength
	ENUM_CFG,
	ENUM_IMPORT_ADDRESS,// SET_ADDRESS after the bus reset done on import
};

typedef struct urb_t {
	struct urb_t *	next;
	uint32_t		seqnum;
	uint8_t			ep;
	uint8_t			in;
	uint8_t			stage;
	uint8_t			internal;	// ENUM_* step, 0 for client URBs
	uint8_t			setup[8];
	uint32_t		flags;
	uint32_t		len;
	uint32_t		done;
	uint8_t *		buf;
} urb_t;

typedef struct usbip_usb_t {
	avr_t *			avr;
	int				listen_fd;
	int				conn_fd;
	int				imported;

	int				dev_state;
	int				attach_pending;	// set by the attach IRQ, handled in the tick
	int				detach_pending;

	uint8_t			dev_desc[18];
	uint8_t *		cfg;
	uint16_t		cfg_len;
	uint8_t			n_ifaces;
	uint8_t			ifaces[32][3];		// class, subclass, protocol
	uint16_t		ep_maxp[16][2];		// [ep][in]
	uint8_t			cfg_value;

	uint8_t *		rx;
	size_t			rx_len, rx_size;

	urb_t *			urbs;
} usbip_usb_t;

static uint32_t
get32(const uint8_t * b)
{
	return (uint32_t)b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3];
}

static uint8_t *
put32(uint8_t * b, uint32_t v)
{
	b[0] = v >> 24; b[1] = v >> 16; b[2] = v >> 8; b[3] = v;
	return b + 4;
}

static uint8_t *
put16(uint8_t * b, uint16_t v)
{
	b[0] = v >> 8; b[1] = v;
	return b + 2;
}

/*
 * URB queue
 */

static void
urb_free(urb_t * u)
{
	free(u->buf);
	free(u);
}

static void
urbs_flush(usbip_usb_t * p)
{
	while (p->urbs) {
		urb_t * u = p->urbs;
		p->urbs = u->next;
		urb_free(u);
	}
}

static void
urb_queue(usbip_usb_t * p, urb_t * u)
{
	urb_t ** l = &p->urbs;
	while (*l)
		l = &(*l)->next;
	u->next = NULL;
	*l = u;
}

static void
urb_remove(usbip_usb_t * p, urb_t * u)
{
	for (urb_t ** l = &p->urbs; *l; l = &(*l)->next)
		if (*l == u) {
			*l = u->next;
			return;
		}
}

static urb_t *
urb_control(uint8_t internal, uint8_t type, uint8_t req, uint16_t value,
		uint16_t index, uint16_t len)
{
	urb_t * u = calloc(1, sizeof(*u));
	u->internal = internal;
	u->setup[0] = type;
	u->setup[1] = req;
	u->setup[2] = value;
	u->setup[3] = value >> 8;
	u->setup[4] = index;
	u->setup[5] = index >> 8;
	u->setup[6] = len;
	u->setup[7] = len >> 8;
	u->in = (type & 0x80) != 0;
	u->len = len;
	u->buf = len ? calloc(1, len) : NULL;
	u->stage = XFER_CTL_SETUP;
	return u;
}

/*
 * Socket
 */

static void
conn_close(usbip_usb_t * p, const char * why)
{
	if (p->conn_fd < 0)
		return;
	AVR_LOG(p->avr, LOG_TRACE, "USBIP: closing connection: %s\n", why);
	sock_close(p->conn_fd);
	p->conn_fd = -1;
	p->imported = 0;
	p->rx_len = 0;
	// client URBs die with the connection, enumeration keeps going
	for (urb_t ** l = &p->urbs; *l; ) {
		urb_t * u = *l;
		if (u->internal) {
			l = &u->next;
		} else {
			*l = u->next;
			urb_free(u);
		}
	}
}

static int
conn_send(usbip_usb_t * p, const void * buf, size_t len)
{
	const uint8_t * b = buf;

	while (len && p->conn_fd >= 0) {
		ssize_t n = send(p->conn_fd, b, len, MSG_NOSIGNAL);
		if (n < 0 && sock_error() == SOCK_EINTR)
			continue;
		if (n <= 0) {
			conn_close(p, "send failed");
			return -1;
		}
		b += n;
		len -= n;
	}
	return p->conn_fd >= 0 ? 0 : -1;
}

static int
sock_readable(int fd)
{
	fd_set set;
	struct timeval tv = { 0, 0 };

	FD_ZERO(&set);
	FD_SET(fd, &set);
	return select(fd + 1, &set, NULL, NULL, &tv) > 0;
}

static void
conn_accept(usbip_usb_t * p)
{
	int fd = accept(p->listen_fd, NULL, NULL);
	int one = 1;

	if (fd < 0)
		return;
	if (p->conn_fd >= 0) {
		// one client at a time, like a device that can be imported once
		sock_close(fd);
		return;
	}
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#ifdef SO_NOSIGPIPE
	setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
	p->conn_fd = fd;
	p->imported = 0;
	p->rx_len = 0;
	AVR_LOG(p->avr, LOG_TRACE, "USBIP: client connected\n");
}

// one recv() per tick, once select() says there is something to read
static void
conn_read(usbip_usb_t * p)
{
	ssize_t n;

	if (p->rx_size - p->rx_len < 4096) {
		p->rx_size = p->rx_size ? p->rx_size * 2 : 16384;
		p->rx = realloc(p->rx, p->rx_size);
	}
	n = recv(p->conn_fd, p->rx + p->rx_len, p->rx_size - p->rx_len, 0);
	if (n > 0)
		p->rx_len += n;
	else if (n == 0 || sock_error() != SOCK_EINTR)
		conn_close(p, n == 0 ? "client closed" : "recv failed");
}

static void
rx_consume(usbip_usb_t * p, size_t n)
{
	memmove(p->rx, p->rx + n, p->rx_len - n);
	p->rx_len -= n;
}

/*
 * Device information, struct usbip_usb_device (312 bytes) and the
 * interface list that follows it in OP_REP_DEVLIST.
 */
static uint8_t *
put_device(usbip_usb_t * p, uint8_t * b)
{
	memset(b, 0, 256 + 32);
	snprintf((char *)b, 256, "/sys/devices/simavr/usb%d/%s", BUSNUM,
			USBIP_USB_BUSID);
	snprintf((char *)b + 256, 32, "%s", USBIP_USB_BUSID);
	b += 256 + 32;
	b = put32(b, BUSNUM);
	b = put32(b, DEVNUM);
	b = put32(b, USB_SPEED_FULL);
	b = put16(b, p->dev_desc[8] | p->dev_desc[9] << 8);		// idVendor
	b = put16(b, p->dev_desc[10] | p->dev_desc[11] << 8);	// idProduct
	b = put16(b, p->dev_desc[12] | p->dev_desc[13] << 8);	// bcdDevice
	*b++ = p->dev_desc[4];		// bDeviceClass
	*b++ = p->dev_desc[5];
	*b++ = p->dev_desc[6];
	*b++ = p->cfg_value;
	*b++ = p->dev_desc[17];		// bNumConfigurations
	*b++ = p->n_ifaces;
	return b;
}

static void
op_reply(usbip_usb_t * p, uint16_t code, uint32_t status, int with_devices)
{
	uint8_t b[8 + 4 + 312 + 32 * 4], * e;

	e = put16(b, USBIP_VERSION);
	e = put16(e, code);
	e = put32(e, status);
	if (with_devices == 1) {	// OP_REP_DEVLIST
		e = put32(e, p->dev_state == DEV_READY ? 1 : 0);
		if (p->dev_state == DEV_READY) {
			e = put_device(p, e);
			for (int i = 0; i < p->n_ifaces; i++) {
				*e++ = p->ifaces[i][0];
				*e++ = p->ifaces[i][1];
				*e++ = p->ifaces[i][2];
				*e++ = 0;
			}
		}
	} else if (with_devices == 2)	// OP_REP_IMPORT
		e = put_device(p, e);
	conn_send(p, b, e - b);
}

static void
ret_submit(usbip_usb_t * p, urb_t * u, int32_t status)
{
	uint8_t b[USBIP_HDR_SIZE], * e;
	uint32_t actual = status == 0 ? u->done : 0;

	memset(b, 0, sizeof(b));
	e = put32(b, USBIP_RET_SUBMIT);
	e = put32(e, u->seqnum);
	e = put32(e, 0);		// devid, direction, ep: zero in replies
	e = put32(e, 0);
	e = put32(e, 0);
	e = put32(e, status);
	e = put32(e, actual);
	e = put32(e, 0);		// start_frame
	e = put32(e, 0);		// number_of_packets, not isochronous
	e = put32(e, 0);		// error_count
	if (conn_send(p, b, sizeof(b)) == 0 && u->in && actual)
		conn_send(p, u->buf, actual);
}

static void
ret_unlink(usbip_usb_t * p, uint32_t seqnum, int32_t status)
{
	uint8_t b[USBIP_HDR_SIZE], * e;

	memset(b, 0, sizeof(b));
	e = put32(b, USBIP_RET_UNLINK);
	e = put32(e, seqnum);
	e = put32(e, 0);
	e = put32(e, 0);
	e = put32(e, 0);
	put32(e, status);
	conn_send(p, b, sizeof(b));
}

/*
 * Enumeration
 */

static void
parse_config(usbip_usb_t * p)
{
	p->n_ifaces = 0;
	for (int i = 0; i < 16; i++)
		p->ep_maxp[i][0] = p->ep_maxp[i][1] = 0;
	for (int i = 0; i + 2 <= p->cfg_len && p->cfg[i] >= 2; i += p->cfg[i]) {
		uint8_t * d = p->cfg + i;
		if (i + d[0] > p->cfg_len)
			break;
		if (d[1] == 4 && d[0] >= 9 && p->n_ifaces < 32 && d[3] == 0) {
			// first alternate setting of each interface
			p->ifaces[p->n_ifaces][0] = d[5];
			p->ifaces[p->n_ifaces][1] = d[6];
			p->ifaces[p->n_ifaces][2] = d[7];
			p->n_ifaces++;
		} else if (d[1] == 5 && d[0] >= 7)
			p->ep_maxp[d[2] & 0xf][d[2] >> 7] = (d[4] | d[5] << 8) & 0x7ff;
	}
}

static void
enum_start(usbip_usb_t * p)
{
	p->dev_state = DEV_ENUMERATING;
	p->cfg_value = 0;
	p->ep_maxp[0][0] = p->ep_maxp[0][1] = 8;	// until the device says
	if (avr_ioctl(p->avr, AVR_IOCTL_USB_RESET, NULL)) {
		p->dev_state = DEV_DETACHED;
		return;
	}
	urb_queue(p, urb_control(ENUM_DEV_HEAD, 0x80, 6, 0x0100, 0, 8));
}

static void
enum_step_done(usbip_usb_t * p, urb_t * u, int status)
{
	if (status) {
		AVR_LOG(p->avr, LOG_WARNING,
				"USBIP: enumeration step %d failed (%d)\n", u->internal, status);
		if (u->internal != ENUM_IMPORT_ADDRESS)
			p->dev_state = DEV_DETACHED;
		return;
	}
	switch (u->internal) {
		case ENUM_DEV_HEAD:
			if (u->done < 8)
				goto short_desc;
			p->ep_maxp[0][0] = p->ep_maxp[0][1] = u->buf[7];
			urb_queue(p, urb_control(ENUM_SET_ADDRESS, 0x00, 5, DEVNUM, 0, 0));
			break;
		case ENUM_SET_ADDRESS:
			urb_queue(p, urb_control(ENUM_DEV, 0x80, 6, 0x0100, 0, 18));
			break;
		case ENUM_DEV:
			if (u->done < 18)
				goto short_desc;
			memcpy(p->dev_desc, u->buf, 18);
			urb_queue(p, urb_control(ENUM_CFG_HEAD, 0x80, 6, 0x0200, 0, 9));
			break;
		case ENUM_CFG_HEAD:
			if (u->done < 9)
				goto short_desc;
			urb_queue(p, urb_control(ENUM_CFG, 0x80, 6, 0x0200, 0,
					u->buf[2] | u->buf[3] << 8));
			break;
		case ENUM_CFG:
			free(p->cfg);
			p->cfg = malloc(u->done);
			memcpy(p->cfg, u->buf, u->done);
			p->cfg_len = u->done;
			parse_config(p);
			p->ep_maxp[0][0] = p->ep_maxp[0][1] = p->dev_desc[7];
			p->dev_state = DEV_READY;
			AVR_LOG(p->avr, LOG_TRACE,
					"USBIP: device %04x:%04x ready, %d interface(s)\n",
					p->dev_desc[8] | p->dev_desc[9] << 8,
					p->dev_desc[10] | p->dev_desc[11] << 8, p->n_ifaces);
			break;
		case ENUM_IMPORT_ADDRESS:
			break;
	}
	return;
short_desc:
	AVR_LOG(p->avr, LOG_WARNING, "USBIP: short descriptor in step %d\n",
			u->internal);
	p->dev_state = DEV_DETACHED;
}

/*
 * Transfers. Each call does at most one IOCTL; returns 1 when the URB
 * completed (status in *status), 0 when it is still pending.
 */
static int
urb_step(usbip_usb_t * p, urb_t * u, int * status)
{
	struct avr_io_usb pkt;
	uint16_t maxp = p->ep_maxp[u->ep][u->in];
	int ret;

	if (!maxp)
		maxp = 64;
	switch (u->stage) {
		case XFER_CTL_SETUP:
			pkt = (struct avr_io_usb){ u->ep, 8, u->setup };
			ret = avr_ioctl(p->avr, AVR_IOCTL_USB_SETUP, &pkt);
			if (ret == AVR_IOCTL_USB_NAK)
				return 0;
			if (ret)
				goto failed;
			u->stage = u->len ? XFER_CTL_DATA : XFER_CTL_STATUS;
			return 0;
		case XFER_CTL_DATA: {
			uint32_t sz = u->len - u->done;
			if (sz > maxp)
				sz = maxp;
			pkt = (struct avr_io_usb){ u->ep, sz, u->buf + u->done };
			ret = avr_ioctl(p->avr,
					u->in ? AVR_IOCTL_USB_READ : AVR_IOCTL_USB_WRITE, &pkt);
			if (ret == AVR_IOCTL_USB_NAK)
				return 0;
			if (ret)
				goto failed;
			u->done += pkt.sz;
			if (u->done == u->len || (u->in && pkt.sz < maxp))
				u->stage = XFER_CTL_STATUS;
			return 0;
		}
		case XFER_CTL_STATUS:
			// zero length packet in the direction opposite to the data
			pkt = (struct avr_io_usb){ u->ep, 0, NULL };
			ret = avr_ioctl(p->avr,
					u->in ? AVR_IOCTL_USB_WRITE : AVR_IOCTL_USB_READ, &pkt);
			if (ret == AVR_IOCTL_USB_NAK)
				return 0;
			if (ret)
				goto failed;
			*status = 0;
			return 1;
		case XFER_DATA: {
			uint32_t sz = u->len - u->done;
			if (sz > maxp)
				sz = maxp;
			pkt = (struct avr_io_usb){ u->ep | (u->in ? 0x80 : 0), sz,
					u->buf + u->done };
			ret = avr_ioctl(p->avr,
					u->in ? AVR_IOCTL_USB_READ : AVR_IOCTL_USB_WRITE, &pkt);
			if (ret == AVR_IOCTL_USB_NAK)
				return 0;
			if (ret)
				goto failed;
			u->done += pkt.sz;
			if (u->in) {
				if (u->done == u->len || pkt.sz < maxp) {
					*status = 0;
					return 1;
				}
			} else if (u->done == u->len) {
				if (u->len && (u->flags & URB_ZERO_PACKET) && !(u->len % maxp)) {
					u->stage = XFER_ZLP;
					return 0;
				}
				*status = 0;
				return 1;
			}
			return 0;
		}
		case XFER_ZLP:
			pkt = (struct avr_io_usb){ u->ep, 0, NULL };
			ret = avr_ioctl(p->avr, AVR_IOCTL_USB_WRITE, &pkt);
			if (ret == AVR_IOCTL_USB_NAK)
				return 0;
			if (ret)
				goto failed;
			*status = 0;
			return 1;
	}
failed:
	*status = -USBIP_EPIPE;	// STALL, or an endpoint the part doesn't have
	return 1;
}

static void
urb_complete(usbip_usb_t * p, urb_t * u, int status)
{
	if (u->ep == 0)
		AVR_LOG(p->avr, LOG_TRACE, "USBIP: #%u EP0 %02x %02x %02x%02x %u/%u: %d\n",
				u->seqnum, u->setup[0], u->setup[1], u->setup[3], u->setup[2],
				u->done, u->len, status);
	else
		AVR_LOG(p->avr, LOG_TRACE, "USBIP: #%u EP%d %s %u/%u: %d\n",
				u->seqnum, u->ep, u->in ? "IN" : "OUT", u->done, u->len, status);
	urb_remove(p, u);
	if (u->internal)
		enum_step_done(p, u, status);
	else {
		// SET_CONFIGURATION, to report it in later device listings
		if (!status && u->ep == 0 && u->setup[0] == 0x00 && u->setup[1] == 9)
			p->cfg_value = u->setup[2];
		ret_submit(p, u, status);
	}
	urb_free(u);
}

/*
 * Runs the URBs, the oldest one of each pipe only, so that transfers on
 * a pipe complete in order while different endpoints interleave.
 */
static void
urbs_run(usbip_usb_t * p)
{
	int steps = 0;
	int progress = 1;

	while (progress && steps < STEPS_PER_TICK) {
		uint32_t busy = 0;	// pipes that already had their turn
		progress = 0;
		for (urb_t * u = p->urbs, * next; u && steps < STEPS_PER_TICK; u = next) {
			uint32_t pipe = u->ep == 0 ? 1 : 1u << (u->ep + (u->in ? 16 : 0));
			uint32_t done_before = u->done;
			uint8_t stage_before = u->stage;
			int status;

			next = u->next;
			if (busy & pipe)
				continue;
			busy |= pipe;
			steps++;
			if (urb_step(p, u, &status)) {
				urb_complete(p, u, status);
				progress = 1;
			} else if (u->done != done_before || u->stage != stage_before)
				progress = 1;
		}
	}
}

/*
 * Requests from the client
 */

static void
handle_op(usbip_usb_t * p)
{
	while (p->conn_fd >= 0 && p->rx_len >= 8) {
		uint16_t code = p->rx[2] << 8 | p->rx[3];

		if (code == OP_REQ_DEVLIST) {
			rx_consume(p, 8);
			op_reply(p, OP_REP_DEVLIST, ST_OK, 1);
		} else if (code == OP_REQ_IMPORT) {
			char busid[33];
			if (p->rx_len < 8 + 32)
				return;
			memcpy(busid, p->rx + 8, 32);
			busid[32] = 0;
			rx_consume(p, 8 + 32);
			if (p->dev_state != DEV_READY || strcmp(busid, USBIP_USB_BUSID)) {
				op_reply(p, OP_REP_IMPORT, ST_NA, 0);
				conn_close(p, "import refused");
				return;
			}
			op_reply(p, OP_REP_IMPORT, ST_OK, 2);
			p->imported = 1;
			// a fresh device for the client, as after plugging it in
			p->cfg_value = 0;
			if (avr_ioctl(p->avr, AVR_IOCTL_USB_RESET, NULL) == 0)
				urb_queue(p, urb_control(ENUM_IMPORT_ADDRESS, 0x00, 5, DEVNUM, 0, 0));
			AVR_LOG(p->avr, LOG_TRACE, "USBIP: device imported\n");
			return;
		} else {
			conn_close(p, "unknown request");
			return;
		}
	}
}

static void
handle_cmd(usbip_usb_t * p)
{
	while (p->conn_fd >= 0 && p->rx_len >= USBIP_HDR_SIZE) {
		uint8_t * h = p->rx;
		uint32_t cmd = get32(h);
		uint32_t seqnum = get32(h + 4);

		if (cmd == USBIP_CMD_SUBMIT) {
			uint32_t dir = get32(h + 12);
			uint32_t ep = get32(h + 16);
			uint32_t flags = get32(h + 20);
			uint32_t len = get32(h + 24);
			uint32_t npackets = get32(h + 32);
			size_t need = USBIP_HDR_SIZE + (dir == USBIP_DIR_IN ? 0 : len);
			int iso = npackets != 0 && npackets != 0xffffffff;

			if (len > 1 << 20) {
				conn_close(p, "transfer too large");
				return;
			}
			if (iso)
				need += 16 * (size_t)npackets;
			if (p->rx_len < need)
				return;
			urb_t * u = calloc(1, sizeof(*u));
			u->seqnum = seqnum;
			u->ep = ep & 0xf;
			u->in = dir == USBIP_DIR_IN;
			u->flags = flags;
			u->len = len;
			u->buf = len ? malloc(len) : NULL;
			memcpy(u->setup, h + 40, 8);
			if (!u->in && len)
				memcpy(u->buf, h + USBIP_HDR_SIZE, len);
			rx_consume(p, need);
			if (iso) {
				AVR_LOG(p->avr, LOG_WARNING,
						"USBIP: isochronous transfers are not supported\n");
				ret_submit(p, u, -USBIP_EINVAL);
				urb_free(u);
				continue;
			}
			if (u->ep == 0) {
				uint16_t wlength = u->setup[6] | u->setup[7] << 8;
				u->in = (u->setup[0] & 0x80) != 0;
				if (wlength < u->len)
					u->len = wlength;
				u->stage = XFER_CTL_SETUP;
			} else
				u->stage = XFER_DATA;
			urb_queue(p, u);
		} else if (cmd == USBIP_CMD_UNLINK) {
			uint32_t victim = get32(h + 20);
			urb_t * found = NULL;

			rx_consume(p, USBIP_HDR_SIZE);
			for (urb_t * u = p->urbs; u; u = u->next)
				if (!u->internal && u->seqnum == victim)
					found = u;
			if (found) {
				urb_remove(p, found);
				urb_free(found);
			}
			// -ECONNRESET: unlinked before completion, 0: already completed
			ret_unlink(p, seqnum, found ? -USBIP_ECONNRESET : 0);
		} else {
			conn_close(p, "unknown command");
			return;
		}
	}
}

static avr_cycle_count_t
usbip_tick(
		struct avr_t * avr,
		avr_cycle_count_t when,
		void * param)
{
	usbip_usb_t * p = param;

	if (p->detach_pending) {
		p->detach_pending = 0;
		urbs_flush(p);
		conn_close(p, "device detached");
		p->dev_state = DEV_DETACHED;
	}
	if (p->attach_pending) {
		p->attach_pending = 0;
		urbs_flush(p);
		enum_start(p);
	}

	if (p->conn_fd < 0 && sock_readable(p->listen_fd))
		conn_accept(p);
	if (p->conn_fd >= 0 && sock_readable(p->conn_fd))
		conn_read(p);
	// requests wait while the device is being (re)enumerated
	if (p->conn_fd >= 0 && p->dev_state != DEV_ENUMERATING) {
		if (p->imported)
			handle_cmd(p);
		else
			handle_op(p);
	}
	urbs_run(p);

	return when + avr_usec_to_cycles(avr, TICK_US);
}

static void
usbip_attach_hook(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	usbip_usb_t * p = param;

	// IOCTLs can't be made from here, in the middle of a register write
	if (value) {
		p->attach_pending = 1;
		p->detach_pending = 0;
	} else {
		p->detach_pending = 1;
		p->attach_pending = 0;
	}
}

struct usbip_usb_t *
usbip_usb_init(
		struct avr_t * avr,
		const char * addr,
		uint16_t port)
{
	usbip_usb_t * p = calloc(1, sizeof(*p));
	struct sockaddr_in sa = { 0 };
	avr_irq_t * irq;

	p->avr = avr;
	p->conn_fd = -1;
	if (network_init()) {
		fprintf(stderr, "usbip: can't initialise the network\n");
		free(p);
		return NULL;
	}
	p->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (p->listen_fd < 0)
		goto fail;
#ifndef __MINGW32__
	{	// restart without waiting for TIME_WAIT. On Windows this would let
		// another program bind the same port, and isn't needed there.
		int one = 1;
		setsockopt(p->listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	}
#endif
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port ? port : USBIP_USB_DEFAULT_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	if (addr && inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
		fprintf(stderr, "usbip: bad address %s\n", addr);
		goto fail;
	}
	if (bind(p->listen_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0 ||
			listen(p->listen_fd, 1) < 0) {
		fprintf(stderr, "usbip: can't listen on port %u, error %d\n",
				ntohs(sa.sin_port), sock_error());
		goto fail;
	}

	irq = avr_io_getirq(avr, AVR_IOCTL_USB_GETIRQ(), USB_IRQ_ATTACH);
	if (!irq) {
		fprintf(stderr, "usbip: %s has no USB controller\n", avr->mmcu);
		goto fail;
	}
	avr_irq_register_notify(irq, usbip_attach_hook, p);
	avr_ioctl(avr, AVR_IOCTL_USB_VBUS, (void *)1);
	avr_cycle_timer_register_usec(avr, TICK_US, usbip_tick, p);
	return p;
fail:
	if (p->listen_fd >= 0)
		sock_close(p->listen_fd);
	network_release();
	free(p);
	return NULL;
}

void
usbip_usb_free(
		struct usbip_usb_t * p)
{
	if (!p)
		return;
	avr_cycle_timer_cancel(p->avr, usbip_tick, p);
	urbs_flush(p);
	conn_close(p, "shutdown");
	sock_close(p->listen_fd);
	network_release();
	free(p->cfg);
	free(p->rx);
	free(p);
}
