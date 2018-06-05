/* vim: set sts=4:sw=4:ts=4:noexpandtab
	usb_types.h

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

#include <stdint.h>

#define byte uint8_t
#define word uint16_t

struct usb_setup_pkt {
	byte reqtype;
	byte req;
	word wValue;
	word wIndex;
	word wLength;
} __attribute__((__packed__));

#define USB_REQTYPE_DIR_DEV_TO_HOST 0x80
#define USB_REQTYPE_STD 0
#define USB_REQTYPE_DEVICE 0

#define USB_REQUEST_GET_DESCRIPTOR 0x06


// USB Descriptors
#define USB_DESCRIPTOR_DEVICE           0x01    // Device Descriptor.
#define USB_DESCRIPTOR_CONFIGURATION    0x02    // Configuration Descriptor.
#define USB_DESCRIPTOR_STRING           0x03    // String Descriptor.
#define USB_DESCRIPTOR_INTERFACE        0x04    // Interface Descriptor.
#define USB_DESCRIPTOR_ENDPOINT         0x05    // Endpoint Descriptor.
#define USB_DESCRIPTOR_DEVICE_QUALIFIER 0x06    // Device Qualifier.

struct usb_device_descriptor {
    byte bLength;               // Length of this descriptor.
    byte bDescriptorType;       // DEVICE descriptor type (USB_DESCRIPTOR_DEVICE).
    word bcdUSB;                // USB Spec Release Number (BCD).
    byte bDeviceClass;          // Class code (assigned by the USB-IF). 0xFF-Vendor specific.
    byte bDeviceSubClass;       // Subclass code (assigned by the USB-IF).
    byte bDeviceProtocol;       // Protocol code (assigned by the USB-IF). 0xFF-Vendor specific.
    byte bMaxPacketSize0;       // Maximum packet size for endpoint 0.
    word idVendor;              // Vendor ID (assigned by the USB-IF).
    word idProduct;             // Product ID (assigned by the manufacturer).
    word bcdDevice;             // Device release number (BCD).
    byte iManufacturer;         // Index of String Descriptor describing the manufacturer.
    byte iProduct;              // Index of String Descriptor describing the product.
    byte iSerialNumber;         // Index of String Descriptor with the device's serial number.
    byte bNumConfigurations;    // Number of possible configurations.
} __attribute__ ((__packed__));

struct usb_configuration_descriptor
{
    byte bLength;               // Length of this descriptor.
    byte bDescriptorType;       // CONFIGURATION descriptor type (USB_DESCRIPTOR_CONFIGURATION).
    word wTotalLength;          // Total length of all descriptors for this configuration.
    byte bNumInterfaces;        // Number of interfaces in this configuration.
    byte bConfigurationValue;   // Value of this configuration (1 based).
    byte iConfiguration;        // Index of String Descriptor describing the configuration.
    byte bmAttributes;          // Configuration characteristics.
    byte bMaxPower;             // Maximum power consumed by this configuration.
} __attribute__ ((__packed__));


struct usb_interface_descriptor
{
    byte bLength;               // Length of this descriptor.
    byte bDescriptorType;       // INTERFACE descriptor type (USB_DESCRIPTOR_INTERFACE).
    byte bInterfaceNumber;      // Number of this interface (0 based).
    byte bAlternateSetting;     // Value of this alternate interface setting.
    byte bNumEndpoints;         // Number of endpoints in this interface.
    byte bInterfaceClass;       // Class code (assigned by the USB-IF).  0xFF-Vendor specific.
    byte bInterfaceSubClass;    // Subclass code (assigned by the USB-IF).
    byte bInterfaceProtocol;    // Protocol code (assigned by the USB-IF).  0xFF-Vendor specific.
    byte iInterface;            // Index of String Descriptor describing the interface.
} __attribute__ ((__packed__));
