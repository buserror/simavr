# 
# 	Copyright 2012 Torbjorn Tyridal <ttyridal@gmail.com>
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

target=	simusb
firm_src = at90usb162_cdc_loopback.c
firmware = ${firm_src:.c=.hex}
firmware2 = ${firm_src:.c=.axf}
simavr = ../../

IPATH = .
IPATH += ../parts
IPATH += ${simavr}/include
IPATH += ${simavr}/simavr/sim
#IPATH += /usr/local/include

VPATH = .
VPATH += ../parts

LDFLAGS += -lpthread

all: obj ${firmware} ${target} ${firmware2}

include ${simavr}/Makefile.common

board = ${OBJ}/${target}.elf

${board} : ${OBJ}/usbip.o
${board} : ${OBJ}/${target}.o

${target}: ${board}
	@echo $@ done

clean: clean-${OBJ}
	rm -rf *.a *.axf *.hex ${target} *.vcd
