#
#	The Arduino Uno R3's ATmega16U2 USB to serial adapter over USB/IP,
#	see README.
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

target=	usbserial
simavr = ../../

IPATH = .
IPATH += ../parts
IPATH += ${simavr}/include
IPATH += ${simavr}/simavr/sim

VPATH = .
VPATH += ../parts

all: obj ${target}

include ${simavr}/Makefile.common

board = ${OBJ}/${target}.elf

${board} : ${OBJ}/usbip_usb.o
${board} : ${OBJ}/${target}.o
# the pty for USART1, POSIX only
ifneq (${shell uname -o}, Msys)
LDFLAGS += -lpthread -lutil
${board} : ${OBJ}/uart_pty.o
endif

${target}: ${board}
	@echo $@ done

clean: clean-${OBJ}
	rm -rf *.a *.axf ${target} *.vcd
