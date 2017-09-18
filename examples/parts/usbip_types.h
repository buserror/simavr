/* vim: set sts=4:sw=4:ts=4:noexpandtab
	usbip_types.h

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

*/

//ref https://github.com/torvalds/linux/blob/master/tools/usb/usbip
//ref https://github.com/torvalds/linux/blob/master/drivers/usb/usbip

#define USBIP_SYSFS_PATH_MAX 256
#define USBIP_SYSFS_BUS_ID_SIZE	32

struct usbip_usb_interface {
	uint8_t bInterfaceClass;
	uint8_t bInterfaceSubClass;
	uint8_t bInterfaceProtocol;
	uint8_t padding;	/* alignment */
} __attribute__((packed));

struct usbip_usb_device {
	char path[USBIP_SYSFS_PATH_MAX];
	char busid[USBIP_SYSFS_BUS_ID_SIZE];

	uint32_t busnum;
	uint32_t devnum;
	uint32_t speed;

	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;

	uint8_t bDeviceClass;
	uint8_t bDeviceSubClass;
	uint8_t bDeviceProtocol;
	uint8_t bConfigurationValue;
	uint8_t bNumConfigurations;
	uint8_t bNumInterfaces;
} __attribute__((packed));


#define USBIP_PROTO_VERSION ((1<<8) | 6)
struct usbip_op_common {
	uint16_t version;

#define USBIP_OP_REQUEST (0x80 << 8)
#define USBIP_OP_REPLY (0x00 << 8)
	uint16_t code;

#define USBIP_ST_OK 0x00
#define USBIP_ST_NA 0x01
	uint32_t status;

} __attribute__((packed));

#define USBIP_OP_DEVLIST 0x05

struct usbip_op_devlist_request {
} __attribute__((packed));

struct usbip_op_devlist_reply {
	uint32_t ndev;
	/* followed by reply_extra[] */
} __attribute__((packed));

struct usbip_op_devlist_reply_extra {
	struct usbip_usb_device    udev;
	struct usbip_usb_interface uinf[];
} __attribute__((packed));


#define USBIP_OP_IMPORT 0x03
struct usbip_op_import_request {
    char busid[USBIP_SYSFS_BUS_ID_SIZE];
} __attribute__((packed));

struct usbip_op_import_reply {
    struct usbip_usb_device udev;
//	struct usbip_usb_interface uinf[];
} __attribute__((packed));

struct usbip_common_hdr {
    uint32_t command;
    uint32_t seqnum;
    uint32_t devid; // (busnum << 16) | devnum
    uint32_t direction;
    uint32_t ep;
} __attribute__ ((__packed__));

#define USBIP_CMD_SUBMIT 0x0001
#define USBIP_CMD_UNLINK 0x0002
#define USBIP_RET_SUBMIT 0x0003
#define USBIP_RET_UNLINK 0x0004
#define USBIP_DIR_OUT 0x00
#define USBIP_DIR_IN 0x01


struct usbip_cmd_submit {
    uint32_t transfer_flags;
    int32_t transfer_buffer_length;
    int32_t start_frame;
    int32_t number_of_packets;
    int32_t interval;
    unsigned char setup[8];
} __attribute__ ((__packed__));

/*
+  Allowed transfer_flags  | value      | control | interrupt | bulk     | isochronous
+ -------------------------+------------+---------+-----------+----------+-------------
+  URB_SHORT_NOT_OK        | 0x00000001 | only in | only in   | only in  | no
+  URB_ISO_ASAP            | 0x00000002 | no      | no        | no       | yes
+  URB_NO_TRANSFER_DMA_MAP | 0x00000004 | yes     | yes       | yes      | yes
+  URB_NO_FSBR             | 0x00000020 | yes     | no        | no       | no
+  URB_ZERO_PACKET         | 0x00000040 | no      | no        | only out | no
+  URB_NO_INTERRUPT        | 0x00000080 | yes     | yes       | yes      | yes
+  URB_FREE_BUFFER         | 0x00000100 | yes     | yes       | yes      | yes
+  URB_DIR_MASK            | 0x00000200 | yes     | yes       | yes      | yes
*/

struct usbip_ret_submit {
    int32_t status;
    int32_t actual_length;
    int32_t start_frame;
    int32_t number_of_packets;
    int32_t error_count;
    long long setup;
} __attribute__ ((__packed__));


struct usbip_cmd_unlink {
    int32_t seqnum_urb;
    int32_t pad1;
    int32_t pad2;
    int32_t pad3;
    int32_t pad4;
    long long pad5;
} __attribute__ ((__packed__));


struct usbip_ret_unlink {
    int32_t status;
    int32_t pad1;
    int32_t pad2;
    int32_t pad3;
    int32_t pad4;
    long long pad5;
} __attribute__ ((__packed__));

struct usbip_header {
    struct usbip_common_hdr hdr;
    union {
        struct usbip_cmd_submit submit;
        struct usbip_ret_submit retsubmit;
        struct usbip_cmd_unlink unlink;
        struct usbip_ret_unlink retunlink;
    } u;
} __attribute__ ((__packed__));
