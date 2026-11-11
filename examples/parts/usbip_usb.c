/*
	usbip_usb.c

	USB/IP server for the simavr USB device model, see usbip_usb.h.

	USB/IP carries USB requests over TCP, so that a USB device plugged
	into one machine (the server) is used by the USB stack of another
	(the client) as if it were plugged in there. The protocol is version
	1.1.1 of Documentation/usb/usbip_protocol.rst in the Linux kernel
	tree. All fields are big endian. On Linux the client is the usbip
	tool and the vhci-hcd driver, a virtual host controller whose ports
	the imported devices are plugged into. On Windows it is usbip-win2.

	A session over one TCP connection, default port 3240:

		client                                  server
		OP_REQ_DEVLIST                 ---->
		                               <----    OP_REP_DEVLIST
		(the connection is closed, "usbip list" is done)

		OP_REQ_IMPORT, busid "1-1"     ---->
		                               <----    OP_REP_IMPORT
		(the socket is handed to vhci-hcd, "usbip attach" is done)
		USBIP_CMD_SUBMIT, seqnum 1     ---->
		USBIP_CMD_SUBMIT, seqnum 2     ---->
		                               <----    USBIP_RET_SUBMIT, seqnum 1
		USBIP_CMD_UNLINK of seqnum 2   ---->
		                               <----    USBIP_RET_UNLINK
		...

	- OP_REQ_DEVLIST lists the exported devices: the fields of their
	  device descriptor and the class of each interface.
	- OP_REQ_IMPORT asks for one device by its bus ID. Once it is
	  accepted, the connection carries that device's URBs until either
	  side closes it. The server closing it is how the client learns
	  that the device was unplugged.
	- A URB (USB request block) is one transfer on one endpoint: a
	  control transfer on EP0 with its 8 byte SETUP, or a bulk or
	  interrupt transfer of up to transfer_buffer_length bytes.
	  USBIP_CMD_SUBMIT carries it with the OUT data following the
	  header. USBIP_RET_SUBMIT completes it with the same seqnum, a
	  status (0 or a Linux errno, negated) and the IN data following
	  the header. The client keeps several URBs pending, so replies
	  can come in any order. A bulk IN URB that the device has no data
	  for stays pending until it has some.
	- USBIP_CMD_UNLINK cancels a pending URB, a client does that when
	  it times out or the file using the device is closed. The
	  USBIP_RET_UNLINK is -ECONNRESET if the URB was still pending,
	  which then gets no USBIP_RET_SUBMIT, or 0 if it had completed
	  already.
	- The client's host controller answers SET_ADDRESS itself, it
	  never reaches the server. The server resets and addresses its
	  own device, as the Linux usbip-host driver does with a real one.

	The part is the server, with the simulated device behind it:
	- It runs from a cycle timer every TICK_US of simulated time:
	  accepts connections, reads the sockets, answers requests and
	  moves the queued URBs on.
	- It enumerates the device itself when the firmware attaches,
	  to list it and to learn its endpoint sizes, and resets it when a
	  client imports it.
	- Each URB is a series of AVR_IOCTL_USB_SETUP, _READ and _WRITE
	  calls to avr_usb.c, one per packet. A packet the firmware isn't
	  ready for (NAK) is tried again on the next tick.
	- When the firmware detaches from the bus, it closes the importing
	  connection, and lists the device again once it has re-enumerated.

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
#include "sim_io.h"
#include "sim_time.h"
#include "sim_network.h"
#include "avr_usb.h"
#include "usbip_usb.h"

/*
 * The sockets are non-blocking, and only read or accepted from once
 * select() says they are ready. A connection that went away between the
 * two then can't make accept() or recv() wait, which would stop the
 * simulation. A send() that finds the socket buffer full waits for room
 * in select() instead, so a client that stops reading holds the
 * simulation until it reads again or closes the connection.
 */
#ifdef __MINGW32__
#define sock_close(fd)	closesocket(fd)
#define sock_error()	WSAGetLastError()
#define sock_blocked()	(WSAGetLastError() == WSAEWOULDBLOCK)
#define SOCK_EINTR		WSAEINTR
#else
#include <fcntl.h>
#include <sys/select.h>
#define sock_close(fd)	close(fd)
#define sock_error()	errno
#define sock_blocked()	(errno == EAGAIN || errno == EWOULDBLOCK)
#define SOCK_EINTR		EINTR
#endif

// macOS has no MSG_NOSIGNAL, SO_NOSIGPIPE is set on the socket instead
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL	0
#endif

#define TICK_US			100		// socket and URB polling period
#define RESET_RECOVERY	(10000 / TICK_US)	// 10ms after a bus reset, TRSTRCY
#define MAX_CONNS		4		// clients listing the device, one importing it
#define BULK_PER_TICK	16		// packets on a pipe while the firmware keeps up

#define USBIP_VERSION	0x0111
#define OP_REQ_DEVLIST	0x8005
#define OP_REP_DEVLIST	0x0005
#define OP_REQ_IMPORT	0x8003
#define OP_REP_IMPORT	0x0003
#define ST_OK			0
#define ST_NA			1
#define ST_DEV_BUSY		2

#define USBIP_CMD_SUBMIT	1
#define USBIP_CMD_UNLINK	2
#define USBIP_RET_SUBMIT	3
#define USBIP_RET_UNLINK	4
#define USBIP_DIR_IN		1
#define USBIP_ISO_DESC_SIZE	16
#define URB_ZERO_PACKET		0x0040	// USBIP_URB_ZERO_PACKET in <linux/usbip.h>

// URB status codes are Linux errno values, whatever the host's are
#define USBIP_EINVAL		22
#define USBIP_EPIPE			32
#define USBIP_ECONNRESET	104

#define USB_SPEED_FULL		2
#define BUSNUM				1
#define DEVNUM				1
#define MAX_IFACES			32

/*
 * The messages, as they are on the wire. Every field is naturally
 * aligned, so there is no padding, and each one is converted with
 * htonl()/htons() or ntohl()/ntohs().
 *
 * They are copies of the Linux definitions, under our names, the paths
 * being in the Linux kernel tree:
 * - usbip_op_t is struct op_common, in tools/usb/usbip/src/usbip_network.h
 * - usbip_device_t and usbip_iface_t are struct usbip_usb_device and
 *   struct usbip_usb_interface, in tools/usb/usbip/libsrc/usbip_common.h,
 *   installed as <usbip/usbip_common.h>
 * - usbip_hdr_t is struct usbip_header, in drivers/usb/usbip/usbip_common.h
 * The first and the last are internal to the usbip tools and the kernel,
 * and <usbip/usbip_common.h> needs libudev and the Linux uapi headers.
 * None of them exist on Windows or macOS, so the part has its own.
 */

// starts OP_REQ_* and OP_REP_*, OP_REQ_IMPORT adds a 32 byte busid
typedef struct usbip_op_t {
	uint16_t		version;	// USBIP_VERSION
	uint16_t		code;		// OP_REQ_* or OP_REP_*
	uint32_t		status;		// ST_*, 0 in requests
} usbip_op_t;

// a device in OP_REP_DEVLIST, after the device count, and in OP_REP_IMPORT
typedef struct usbip_device_t {
	char			path[256];
	char			busid[32];
	uint32_t		busnum;
	uint32_t		devnum;
	uint32_t		speed;
	uint16_t		id_vendor;
	uint16_t		id_product;
	uint16_t		bcd_device;
	uint8_t			device_class;
	uint8_t			device_subclass;
	uint8_t			device_protocol;
	uint8_t			configuration_value;
	uint8_t			num_configurations;
	uint8_t			num_interfaces;
} usbip_device_t;

// follows each device in OP_REP_DEVLIST, num_interfaces of them
typedef struct usbip_iface_t {
	uint8_t			iface_class;
	uint8_t			iface_subclass;
	uint8_t			iface_protocol;
	uint8_t			padding;
} usbip_iface_t;

// starts every message once a device is imported
typedef struct usbip_hdr_t {
	uint32_t		command;	// USBIP_CMD_* or USBIP_RET_*
	uint32_t		seqnum;		// the client's, a reply has its request's
	uint32_t		devid;		// busnum << 16 | devnum
	uint32_t		direction;	// 0 OUT, USBIP_DIR_IN
	uint32_t		ep;			// endpoint number, without the direction bit
	union {
		struct {
			uint32_t	transfer_flags;	// Linux URB flags, URB_ZERO_PACKET
			uint32_t	transfer_buffer_length;
			uint32_t	start_frame;
			// isochronous only, 0 or 0xffffffff otherwise
			uint32_t	number_of_packets;
			uint32_t	interval;
			uint8_t		setup[8];	// SETUP packet of a control transfer
		} submit;
		struct {
			uint32_t	status;			// 0 or a negative Linux errno
			uint32_t	actual_length;	// bytes transferred
			uint32_t	start_frame;
			uint32_t	number_of_packets;
			uint32_t	error_count;
			uint8_t		padding[8];
		} ret;
		struct {
			uint32_t	seqnum;		// of the USBIP_CMD_SUBMIT to cancel
			uint8_t		padding[24];
		} unlink;
	};
} usbip_hdr_t;

enum { DEV_DETACHED, DEV_ENUMERATING, DEV_READY };

// stages of a transfer, the status one being a zero length packet
enum { XFER_SETUP, XFER_DATA, XFER_STATUS };
// what urb_step() did
enum { STEP_NAK, STEP_MORE, STEP_DONE };

/*
 * The part's own enumeration, done with URBs: enum_setups[step - 1].
 * ENUM_IMPORT is the SET_ADDRESS after the bus reset done on import.
 */
static const uint8_t enum_setups[][8] = {
	{ 0x00, 5, DEVNUM, 0, 0, 0, 0, 0 },	// SET_ADDRESS
	{ 0x80, 6, 0, 1, 0, 0, 18, 0 },		// GET_DESCRIPTOR(device)
	{ 0x80, 6, 0, 2, 0, 0, 9, 0 },		// GET_DESCRIPTOR(configuration), head
	{ 0x80, 6, 0, 2, 0, 0, 0, 0 },		// and all of it, wTotalLength set
};
#define ENUM_IMPORT		5

typedef struct urb_t {
	struct urb_t *	next;
	uint32_t		seqnum;
	uint8_t			ep, in, stage;
	uint8_t			internal;	// enumeration step, 0 for client URBs
	uint8_t			setup[8];
	uint32_t		flags, len, done;
	int32_t			status;
	uint8_t			buf[];
} urb_t;

typedef struct conn_t {
	int				fd;			// -1 when the slot is free
	uint8_t *		rx;
	size_t			rx_len, rx_size;
} conn_t;

typedef struct usbip_usb_t {
	avr_io_t		io;			// for the reset and dealloc callbacks
	avr_t *			avr;
	int				listen_fd;
	conn_t			conns[MAX_CONNS];
	conn_t *		imp;		// the connection that imported the device

	int				dev_state;
	int				attached, attach_changed;	// from the attach IRQ
	int				hold_ticks;	// no transfers until 0, after a bus reset

	uint8_t			dev_desc[18];
	uint8_t			n_ifaces;
	usbip_iface_t	ifaces[MAX_IFACES];
	uint16_t		ep_maxp[16][2];	// [ep][in], 0 until the device gives it
	uint8_t			cfg_value;

	urb_t *			urbs;
} usbip_usb_t;

// USB descriptors are little endian
static uint16_t
le16(const uint8_t * bytes)
{
	return bytes[0] | bytes[1] << 8;
}

static urb_t *
urb_new(uint32_t len)
{
	urb_t * urb = calloc(1, sizeof(*urb) + len);

	urb->len = len;
	return urb;
}

static void
urb_queue(usbip_usb_t * usbip, urb_t * urb)
{
	urb_t ** link = &usbip->urbs;

	while (*link)
		link = &(*link)->next;
	urb->next = NULL;
	*link = urb;
}

// all URBs, or only the client's
static void
urbs_flush(usbip_usb_t * usbip, int client_only)
{
	for (urb_t ** link = &usbip->urbs; *link; ) {
		urb_t * urb = *link;
		if (client_only && urb->internal) {
			link = &urb->next;
			continue;
		}
		*link = urb->next;
		free(urb);
	}
}

/*
 * Connections
 */

static void
conn_close(usbip_usb_t * usbip, conn_t * conn, const char * why)
{
	if (!conn || conn->fd < 0)
		return;
	AVR_LOG(usbip->avr, LOG_TRACE, "USBIP: closing connection: %s\n", why);
	sock_close(conn->fd);
	conn->fd = -1;
	conn->rx_len = 0;
	if (conn == usbip->imp) {
		usbip->imp = NULL;
		urbs_flush(usbip, 1);	// the enumeration keeps going
	}
}

static int
sock_nonblock(int fd)
{
#ifdef __MINGW32__
	u_long on = 1;

	return ioctlsocket(fd, FIONBIO, &on);
#else
	int flags = fcntl(fd, F_GETFL);

	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

// until there is room in the socket buffer, or the client has gone
static void
sock_wait_writable(int fd)
{
	fd_set set;

	FD_ZERO(&set);
	FD_SET(fd, &set);
	select(fd + 1, NULL, &set, NULL, NULL);
}

static int
conn_send(usbip_usb_t * usbip, conn_t * conn, const void * buf, size_t len)
{
	const uint8_t * bytes = buf;

	while (len && conn->fd >= 0) {
		ssize_t sent = send(conn->fd, bytes, len, MSG_NOSIGNAL);
		if (sent < 0 && sock_error() == SOCK_EINTR)
			continue;
		if (sent < 0 && sock_blocked()) {
			sock_wait_writable(conn->fd);
			continue;
		}
		if (sent <= 0) {
			conn_close(usbip, conn, "send failed");
			return -1;
		}
		bytes += sent;
		len -= sent;
	}
	if (conn->fd < 0)
		return -1;
	return 0;
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
conn_accept(usbip_usb_t * usbip)
{
	int fd = accept(usbip->listen_fd, NULL, NULL);
	int one = 1;

	if (fd < 0)
		return;	// gone already, or nothing to accept
	for (int i = 0; i < MAX_CONNS; i++) {
		conn_t * conn = &usbip->conns[i];
		if (conn->fd >= 0)
			continue;
		// Linux doesn't pass O_NONBLOCK on from the listening socket
		if (sock_nonblock(fd)) {
			AVR_LOG(usbip->avr, LOG_WARNING,
					"USBIP: can't make the connection non-blocking\n");
			sock_close(fd);
			return;
		}
		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#ifdef SO_NOSIGPIPE
		setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
		conn->fd = fd;
		conn->rx_len = 0;
		AVR_LOG(usbip->avr, LOG_TRACE, "USBIP: client connected\n");
		return;
	}
	AVR_LOG(usbip->avr, LOG_WARNING, "USBIP: too many clients\n");
	sock_close(fd);
}

// one recv() per tick, once select() says there is something to read
static void
conn_read(usbip_usb_t * usbip, conn_t * conn)
{
	ssize_t got;

	if (conn->rx_size - conn->rx_len < 4096) {
		if (conn->rx_size)
			conn->rx_size *= 2;
		else
			conn->rx_size = 16384;
		conn->rx = realloc(conn->rx, conn->rx_size);
	}
	got = recv(conn->fd, conn->rx + conn->rx_len, conn->rx_size - conn->rx_len, 0);
	if (got > 0)
		conn->rx_len += got;
	else if (got == 0)
		conn_close(usbip, conn, "client closed");
	else if (sock_error() != SOCK_EINTR && !sock_blocked())
		conn_close(usbip, conn, "recv failed");
}

static void
rx_consume(conn_t * conn, size_t len)
{
	memmove(conn->rx, conn->rx + len, conn->rx_len - len);
	conn->rx_len -= len;
}

/*
 * Replies. OP_REP_DEVLIST is the header, the number of devices, then
 * the device and its interfaces. OP_REP_IMPORT is the header and, when
 * the import is accepted, the device without its interfaces.
 */

static uint8_t *
append(uint8_t * end, const void * data, size_t len)
{
	memcpy(end, data, len);
	return end + len;
}

static void
op_reply(usbip_usb_t * usbip, conn_t * conn, uint16_t code, uint32_t status,
		int devices)
{
	uint8_t msg[sizeof(usbip_op_t) + sizeof(uint32_t) + sizeof(usbip_device_t) +
			sizeof(usbip->ifaces)];
	usbip_op_t op = {
		.version = htons(USBIP_VERSION),
		.code = htons(code),
		.status = htonl(status),
	};
	uint8_t * end = append(msg, &op, sizeof(op));

	if (code == OP_REP_DEVLIST) {
		uint32_t count = htonl(devices);
		end = append(end, &count, sizeof(count));
	}
	if (devices) {
		const uint8_t * desc = usbip->dev_desc;
		usbip_device_t dev = {
			.busnum = htonl(BUSNUM),
			.devnum = htonl(DEVNUM),
			.speed = htonl(USB_SPEED_FULL),
			.id_vendor = htons(le16(desc + 8)),
			.id_product = htons(le16(desc + 10)),
			.bcd_device = htons(le16(desc + 12)),
			.device_class = desc[4],
			.device_subclass = desc[5],
			.device_protocol = desc[6],
			.configuration_value = usbip->cfg_value,
			.num_configurations = desc[17],
			.num_interfaces = usbip->n_ifaces,
		};
		snprintf(dev.path, sizeof(dev.path), "/sys/devices/simavr/usb%d/%s",
				BUSNUM, USBIP_USB_BUSID);
		snprintf(dev.busid, sizeof(dev.busid), "%s", USBIP_USB_BUSID);
		end = append(end, &dev, sizeof(dev));
		if (code == OP_REP_DEVLIST)
			end = append(end, usbip->ifaces,
					usbip->n_ifaces * sizeof(usbip->ifaces[0]));
	}
	conn_send(usbip, conn, msg, end - msg);
}

// USBIP_RET_SUBMIT or USBIP_RET_UNLINK to the importing client, data
// following the header for an IN transfer
static void
ret_send(usbip_usb_t * usbip, uint32_t command, uint32_t seqnum,
		int32_t status, const uint8_t * data, uint32_t actual)
{
	usbip_hdr_t hdr = {
		.command = htonl(command),
		.seqnum = htonl(seqnum),
		.ret = {
			.status = htonl(status),
			.actual_length = htonl(actual),
		},
	};

	if (usbip->imp && conn_send(usbip, usbip->imp, &hdr, sizeof(hdr)) == 0 &&
			data && actual)
		conn_send(usbip, usbip->imp, data, actual);
}

/*
 * Enumeration
 */

static void
enum_queue(usbip_usb_t * usbip, int step, uint16_t len)
{
	int entry = step - 1;
	const uint8_t * setup;
	urb_t * urb;

	if (step == ENUM_IMPORT)
		entry = 0;	// the SET_ADDRESS again
	setup = enum_setups[entry];
	if (!len)
		len = setup[6];
	urb = urb_new(len);
	memcpy(urb->setup, setup, 8);
	urb->setup[6] = len;
	urb->setup[7] = len >> 8;
	urb->in = setup[0] >> 7;
	urb->internal = step;
	urb_queue(usbip, urb);
}

/*
 * Bus reset. As a real host does, the device is given the reset recovery
 * time before the next transfer: the firmware handles the end of reset
 * in an interrupt and sets EP0 up again, which would lose a SETUP that
 * came first.
 */
static int
bus_reset(usbip_usb_t * usbip)
{
	if (avr_ioctl(usbip->avr, AVR_IOCTL_USB_RESET, NULL))
		return -1;
	usbip->hold_ticks = RESET_RECOVERY;
	return 0;
}

static void
enum_start(usbip_usb_t * usbip)
{
	usbip->dev_state = DEV_ENUMERATING;
	usbip->cfg_value = 0;
	memset(usbip->ep_maxp, 0, sizeof(usbip->ep_maxp));
	if (bus_reset(usbip))
		usbip->dev_state = DEV_DETACHED;
	else
		enum_queue(usbip, 1, 0);
}

// interfaces (their first alternate setting) and endpoint sizes
static void
parse_config(usbip_usb_t * usbip, const uint8_t * cfg, uint32_t len)
{
	memset(usbip->ifaces, 0, sizeof(usbip->ifaces));
	usbip->n_ifaces = 0;
	for (uint32_t i = 0; i + 2 <= len && cfg[i] >= 2; i += cfg[i]) {
		const uint8_t * desc = cfg + i;
		if (i + desc[0] > len)
			break;
		if (desc[1] == 4 && desc[0] >= 9 && desc[3] == 0 &&
				usbip->n_ifaces < MAX_IFACES) {
			usbip_iface_t * iface = &usbip->ifaces[usbip->n_ifaces++];
			iface->iface_class = desc[5];
			iface->iface_subclass = desc[6];
			iface->iface_protocol = desc[7];
		} else if (desc[1] == 5 && desc[0] >= 7)
			usbip->ep_maxp[desc[2] & 0xf][desc[2] >> 7] = le16(desc + 4) & 0x7ff;
	}
}

static void
enum_done(usbip_usb_t * usbip, urb_t * urb)
{
	if (urb->status || urb->done < urb->len) {
		AVR_LOG(usbip->avr, LOG_WARNING,
				"USBIP: enumeration step %d failed (%d)\n",
				urb->internal, urb->status);
		if (urb->internal != ENUM_IMPORT)
			usbip->dev_state = DEV_DETACHED;
		return;
	}
	switch (urb->internal) {
		case 1:
			enum_queue(usbip, 2, 0);
			break;
		case 2:
			memcpy(usbip->dev_desc, urb->buf, 18);
			usbip->ep_maxp[0][0] = usbip->ep_maxp[0][1] = urb->buf[7];
			enum_queue(usbip, 3, 0);
			break;
		case 3:
			enum_queue(usbip, 4, le16(urb->buf + 2));
			break;
		case 4:
			parse_config(usbip, urb->buf, urb->done);
			usbip->dev_state = DEV_READY;
			AVR_LOG(usbip->avr, LOG_TRACE,
					"USBIP: device %04x:%04x ready, %d interface(s)\n",
					le16(usbip->dev_desc + 8), le16(usbip->dev_desc + 10),
					usbip->n_ifaces);
			break;
	}
}

/*
 * Transfers. One IOCTL per call: STEP_NAK, STEP_MORE, or STEP_DONE with
 * the status in urb->status. Until the device gives an endpoint's size
 * (ep_maxp 0, EP0 during the enumeration) a data stage reads whatever
 * the firmware sends, until it has the length asked for.
 */
static int
urb_step(usbip_usb_t * usbip, urb_t * urb)
{
	uint16_t maxp = usbip->ep_maxp[urb->ep][urb->in];
	struct avr_io_usb pkt = { urb->ep, 0, NULL };
	uint32_t ctl;
	int ret;

	if (!maxp && urb->ep)
		maxp = 64;
	switch (urb->stage) {
		case XFER_SETUP:
			pkt.sz = 8;
			pkt.buf = urb->setup;
			ctl = AVR_IOCTL_USB_SETUP;
			break;
		case XFER_DATA:
			pkt.sz = urb->len - urb->done;
			if (maxp && pkt.sz > maxp)
				pkt.sz = maxp;
			pkt.buf = urb->buf + urb->done;
			ctl = urb->in ? AVR_IOCTL_USB_READ : AVR_IOCTL_USB_WRITE;
			break;
		default:	// EP0 status, opposite to the data, or a bulk OUT ZLP
			if (urb->in || urb->ep)
				ctl = AVR_IOCTL_USB_WRITE;
			else
				ctl = AVR_IOCTL_USB_READ;
			break;
	}
	ret = avr_ioctl(usbip->avr, ctl, &pkt);
	if (ret == AVR_IOCTL_USB_NAK)
		return STEP_NAK;
	if (ret) {
		urb->status = -USBIP_EPIPE;	// STALL, or an endpoint the part doesn't have
		return STEP_DONE;
	}
	switch (urb->stage) {
		case XFER_SETUP:
			urb->stage = urb->len ? XFER_DATA : XFER_STATUS;
			return STEP_MORE;
		case XFER_DATA:
			urb->done += pkt.sz;
			if (urb->done < urb->len && !(urb->in && maxp && pkt.sz < maxp))
				return STEP_MORE;	// more packets
			if (urb->ep == 0 || (!urb->in && urb->len &&
					(urb->flags & URB_ZERO_PACKET) && !(urb->len % maxp))) {
				urb->stage = XFER_STATUS;
				return STEP_MORE;
			}
			return STEP_DONE;
		default:
			return STEP_DONE;
	}
}

static void
urb_complete(usbip_usb_t * usbip, urb_t * urb)
{
	AVR_LOG(usbip->avr, LOG_TRACE, "USBIP: #%u EP%d%s %02x %02x %u/%u: %d\n",
			urb->seqnum, urb->ep, urb->in ? " IN" : "", urb->setup[0],
			urb->setup[1], urb->done, urb->len, urb->status);
	if (urb->internal)
		enum_done(usbip, urb);
	else {
		const uint8_t * data = NULL;
		uint32_t actual = 0;

		// SET_CONFIGURATION, to report it in later device listings
		if (!urb->status && urb->ep == 0 && urb->setup[0] == 0x00 &&
				urb->setup[1] == 9)
			usbip->cfg_value = urb->setup[2];
		if (!urb->status)
			actual = urb->done;
		if (urb->in)
			data = urb->buf;	// follows the reply
		ret_send(usbip, USBIP_RET_SUBMIT, urb->seqnum, urb->status, data,
				actual);
	}
	free(urb);
}

/*
 * Each pipe's oldest URB runs once per tick, so that transfers on a pipe
 * complete in order and endpoints interleave. EP0 gets one transaction:
 * the firmware has to run between the stages of a control transfer, as
 * a real host lets it, LUFA stalls a SETUP that comes straight after a
 * status stage or answers a status stage with data left from the
 * previous one. A bulk pipe goes on while the firmware keeps up, up to
 * BULK_PER_TICK packets, more than full speed allows in 100us. URBs are
 * completed after the pass, as that can close the connection.
 */
static void
urbs_run(usbip_usb_t * usbip)
{
	uint32_t busy = 0;
	urb_t * done = NULL, ** done_tail = &done;

	if (usbip->hold_ticks) {
		usbip->hold_ticks--;
		return;
	}
	for (urb_t ** link = &usbip->urbs; *link; ) {
		urb_t * urb = *link;
		uint32_t pipe = 1;	// EP0, both directions
		int packets = 1;
		int step = STEP_NAK;

		if (urb->ep) {
			pipe = 1u << urb->ep;
			if (urb->in)
				pipe <<= 16;
			packets = BULK_PER_TICK;
		}
		if (!(busy & pipe)) {
			busy |= pipe;
			for (int n = 0; n < packets; n++)
				if ((step = urb_step(usbip, urb)) != STEP_MORE)
					break;
		}
		if (step == STEP_DONE) {
			*link = urb->next;
			urb->next = NULL;
			*done_tail = urb;
			done_tail = &urb->next;
		} else
			link = &urb->next;
	}
	while (done) {
		urb_t * urb = done;
		done = urb->next;
		urb_complete(usbip, urb);
	}
}

/*
 * Requests from the clients
 */

static void
handle_op(usbip_usb_t * usbip, conn_t * conn)
{
	while (conn->fd >= 0 && conn->rx_len >= sizeof(usbip_op_t)) {
		usbip_op_t op;
		char busid[33] = { 0 };

		memcpy(&op, conn->rx, sizeof(op));
		if (ntohs(op.code) == OP_REQ_DEVLIST) {
			rx_consume(conn, sizeof(op));
			op_reply(usbip, conn, OP_REP_DEVLIST, ST_OK,
					usbip->dev_state == DEV_READY);
			continue;
		}
		if (ntohs(op.code) != OP_REQ_IMPORT) {
			conn_close(usbip, conn, "unknown request");
			return;
		}
		if (conn->rx_len < sizeof(op) + 32)
			return;
		memcpy(busid, conn->rx + sizeof(op), 32);
		rx_consume(conn, sizeof(op) + 32);
		if (usbip->imp || usbip->dev_state != DEV_READY ||
				strcmp(busid, USBIP_USB_BUSID)) {
			op_reply(usbip, conn, OP_REP_IMPORT,
					usbip->imp ? ST_DEV_BUSY : ST_NA, 0);
			conn_close(usbip, conn, "import refused");
			return;
		}
		op_reply(usbip, conn, OP_REP_IMPORT, ST_OK, 1);
		usbip->imp = conn;
		// a fresh device for the client, as after plugging it in
		usbip->cfg_value = 0;
		if (bus_reset(usbip) == 0)
			enum_queue(usbip, ENUM_IMPORT, 0);
		AVR_LOG(usbip->avr, LOG_TRACE, "USBIP: device imported\n");
		return;
	}
}

static void
handle_cmd(usbip_usb_t * usbip)
{
	conn_t * conn = usbip->imp;

	while (usbip->imp == conn && conn->fd >= 0 &&
			conn->rx_len >= sizeof(usbip_hdr_t)) {
		usbip_hdr_t hdr;

		memcpy(&hdr, conn->rx, sizeof(hdr));
		uint32_t command = ntohl(hdr.command), seqnum = ntohl(hdr.seqnum);
		if (command == USBIP_CMD_UNLINK) {
			uint32_t victim = ntohl(hdr.unlink.seqnum);
			urb_t ** link = &usbip->urbs;

			rx_consume(conn, sizeof(hdr));
			while (*link && ((*link)->internal || (*link)->seqnum != victim))
				link = &(*link)->next;
			// -ECONNRESET: unlinked before completion, 0: already completed
			ret_send(usbip, USBIP_RET_UNLINK, seqnum,
					*link ? -USBIP_ECONNRESET : 0, NULL, 0);
			if (*link) {
				urb_t * urb = *link;
				*link = urb->next;
				free(urb);
			}
			continue;
		}
		if (command != USBIP_CMD_SUBMIT) {
			conn_close(usbip, conn, "unknown command");
			return;
		}
		uint32_t in = ntohl(hdr.direction) == USBIP_DIR_IN;
		uint32_t len = ntohl(hdr.submit.transfer_buffer_length);
		uint32_t npackets = ntohl(hdr.submit.number_of_packets);
		int iso = npackets != 0 && npackets != 0xffffffff;
		size_t need = sizeof(hdr);

		if (!in)
			need += len;	// the OUT data follows the header
		if (iso)
			need += USBIP_ISO_DESC_SIZE * (size_t)npackets;

		if (len > 1 << 20) {
			conn_close(usbip, conn, "transfer too large");
			return;
		}
		if (conn->rx_len < need)
			return;
		urb_t * urb = urb_new(len);
		urb->seqnum = seqnum;
		urb->ep = ntohl(hdr.ep) & 0xf;
		urb->in = in;
		urb->flags = ntohl(hdr.submit.transfer_flags);
		memcpy(urb->setup, hdr.submit.setup, 8);
		if (!in)
			memcpy(urb->buf, conn->rx + sizeof(hdr), len);
		rx_consume(conn, need);
		if (iso) {
			AVR_LOG(usbip->avr, LOG_WARNING,
					"USBIP: isochronous transfers are not supported\n");
			ret_send(usbip, USBIP_RET_SUBMIT, seqnum, -USBIP_EINVAL, NULL, 0);
			free(urb);
			continue;
		}
		if (urb->ep == 0) {
			uint16_t wlength = le16(urb->setup + 6);
			urb->in = urb->setup[0] >> 7;
			if (wlength < urb->len)
				urb->len = wlength;
		} else
			urb->stage = XFER_DATA;
		urb_queue(usbip, urb);
	}
}

static avr_cycle_count_t
usbip_tick(
		struct avr_t * avr,
		avr_cycle_count_t when,
		void * param)
{
	usbip_usb_t * usbip = param;

	if (usbip->attach_changed) {	// a detach, an attach, or both
		usbip->attach_changed = 0;
		urbs_flush(usbip, 0);
		conn_close(usbip, usbip->imp, "device detached");
		usbip->dev_state = DEV_DETACHED;
		if (usbip->attached)
			enum_start(usbip);
	}
	if (sock_readable(usbip->listen_fd))
		conn_accept(usbip);
	for (int i = 0; i < MAX_CONNS; i++) {
		conn_t * conn = &usbip->conns[i];

		if (conn->fd >= 0 && sock_readable(conn->fd))
			conn_read(usbip, conn);
		// requests wait while the device is being (re)enumerated
		if (conn->fd < 0 || usbip->dev_state == DEV_ENUMERATING)
			continue;
		if (conn == usbip->imp)
			handle_cmd(usbip);
		else
			handle_op(usbip, conn);
	}
	urbs_run(usbip);
	return when + avr_usec_to_cycles(avr, TICK_US);
}

// IOCTLs can't be made from here, in the middle of a register write
static void
usbip_attach_hook(
		struct avr_irq_t * irq,
		uint32_t value,
		void * param)
{
	usbip_usb_t * usbip = param;

	usbip->attached = value;
	usbip->attach_changed = 1;
}

/*
 * An AVR reset (watchdog, reset pin) cancels all cycle timers. The
 * device detaches on the way, through the attach IRQ, so the tick only
 * has to be started again.
 */
static void
usbip_reset(
		avr_io_t * io)
{
	usbip_usb_t * usbip = (usbip_usb_t *)io;

	avr_cycle_timer_register_usec(usbip->avr, TICK_US, usbip_tick, usbip);
}

static void
usbip_dealloc(
		avr_io_t * io)
{
	usbip_usb_t * usbip = (usbip_usb_t *)io;

	avr_cycle_timer_cancel(usbip->avr, usbip_tick, usbip);
	urbs_flush(usbip, 0);
	for (int i = 0; i < MAX_CONNS; i++) {
		conn_close(usbip, &usbip->conns[i], "shutdown");
		free(usbip->conns[i].rx);
	}
	sock_close(usbip->listen_fd);
	network_release();
	free(usbip);
}

struct usbip_usb_t *
usbip_usb_init(
		struct avr_t * avr,
		const char * addr,
		uint16_t port)
{
	usbip_usb_t * usbip = calloc(1, sizeof(*usbip));
	struct sockaddr_in sa = { 0 };
	avr_irq_t * irq = avr_io_getirq(avr, AVR_IOCTL_USB_GETIRQ(), USB_IRQ_ATTACH);

	if (!irq) {
		fprintf(stderr, "usbip: %s has no USB controller\n", avr->mmcu);
		free(usbip);
		return NULL;
	}
	usbip->avr = avr;
	for (int i = 0; i < MAX_CONNS; i++)
		usbip->conns[i].fd = -1;
	if (network_init()) {
		fprintf(stderr, "usbip: can't initialise the network\n");
		free(usbip);
		return NULL;
	}
	usbip->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (usbip->listen_fd < 0 || sock_nonblock(usbip->listen_fd))
		goto fail;
	if (!port)
		port = USBIP_USB_DEFAULT_PORT;
#ifndef __MINGW32__
	{	// restart without waiting for TIME_WAIT. On Windows this would let
		// another program bind the same port, and isn't needed there.
		int one = 1;
		setsockopt(usbip->listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	}
#endif
	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (addr && inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
		fprintf(stderr, "usbip: bad address %s\n", addr);
		goto fail;
	}
	if (bind(usbip->listen_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0 ||
			listen(usbip->listen_fd, MAX_CONNS) < 0) {
		fprintf(stderr, "usbip: can't listen on port %u, error %d\n",
				ntohs(sa.sin_port), sock_error());
		goto fail;
	}
	avr_irq_register_notify(irq, usbip_attach_hook, usbip);
	usbip->io.kind = "usbip";
	usbip->io.reset = usbip_reset;
	usbip->io.dealloc = usbip_dealloc;
	avr_register_io(avr, &usbip->io);
	avr_ioctl(avr, AVR_IOCTL_USB_VBUS, (void *)1);
	avr_cycle_timer_register_usec(avr, TICK_US, usbip_tick, usbip);
	return usbip;
fail:
	if (usbip->listen_fd >= 0)
		sock_close(usbip->listen_fd);
	network_release();
	free(usbip);
	return NULL;
}
