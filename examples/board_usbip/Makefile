#
#	USB/IP version of extra_board_usb, see README.
#
#	This file is part of simavr.
#
#	simavr is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	(at your option) any later version.
#
#	simavr is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with simavr.  If not, see <http://www.gnu.org/licenses/>.

target=	simusbip
simavr = ../../

# the default firmware is the CDC loopback of extra_board_usb
firmware_dir = ../extra_board_usb
firmware = ${firmware_dir}/at90usb162_cdc_loopback.hex

IPATH = .
IPATH += ../parts
IPATH += ${simavr}/include
IPATH += ${simavr}/simavr/sim

VPATH = .
VPATH += ../parts

all: obj ${firmware} ${target}

include ${simavr}/Makefile.common

${firmware}: ${firmware_dir}/at90usb162_cdc_loopback.c
	$(MAKE) -C ${firmware_dir} at90usb162_cdc_loopback.hex

board = ${OBJ}/${target}.elf

${board} : ${OBJ}/usbip_usb.o
${board} : ${OBJ}/${target}.o

${target}: ${board}
	@echo $@ done

clean: clean-${OBJ}
	rm -rf *.a *.axf *.hex ${target} *.vcd
