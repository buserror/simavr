# 
# 	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>
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

target=	ayab
simavr = ../..

IPATH = .
IPATH += ${simavr}/examples/parts
IPATH += ${simavr}/include
IPATH += ${simavr}/simavr/sim

VPATH = .
VPATH += ${simavr}/examples/parts
#CFLAGS += -DAVR_STACK_WATCH=1 -DCONFIG_SIMAVR_TRACE=1
LDFLAGS += -lpthread

all: obj ${target} 

include ../Makefile.opengl
include ${simavr}/Makefile.common

board = ${OBJ}/${target}.elf

${board} : ${OBJ}/${target}.o ${OBJ}/${target}_display.o ${OBJ}/queue.o
${board} : ${OBJ}/button.o
${board} : ${OBJ}/uart_pty.o
${board} : ${OBJ}/i2c_mcp23008_virt.o

${target}: ${board}
	@echo $@ done

clean: clean-${OBJ}
	rm -rf *.hex *.a *.axf ${target} *.vcd .*.swo .*.swp .*.swm .*.swn
