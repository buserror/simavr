/*
	vhci_usb.h

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

#include <stdbool.h>

struct avr_t;
typedef struct vhci_usb_t {
    struct avr_t * avr;

    bool attached;
    int fd;
} vhci_usb_t;


void vhci_usb_connect(struct vhci_usb_t * p, char uart);
void vhci_usb_init(struct avr_t * avr, struct vhci_usb_t * p);
