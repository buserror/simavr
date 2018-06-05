/*
	usbip.h

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


struct usbip_t;

struct usbip_t *
usbip_create(
		struct avr_t * avr);

void
usbip_destroy(
		struct usbip_t *);

void *
usbip_main(
		void * /* struct usbip_t * */);
