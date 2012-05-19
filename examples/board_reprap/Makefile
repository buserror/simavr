# 
# 	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>
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

target=	reprap
firm_src = ${wildcard atmega*.c}
firmware = ${firm_src:.c=.hex}
simavr = ../../

IPATH = .
IPATH += src
IPATH += ../parts
IPATH += ../shared
IPATH += ${simavr}/include
IPATH += ${simavr}/simavr/sim

VPATH = src
VPATH += src/c3
VPATH += ../parts
VPATH += ../shared

# for the Open Motion Controller board
CFLAGS += -DMOTHERBOARD=91
CFLAGS += ${shell pkg-config --cflags pangocairo}

include ../Makefile.opengl

LDFLAGS += ${shell pkg-config --libs pangocairo}
LDFLAGS += -lpthread -lutil -ldl
LDFLAGS += -lm

C3SRC	= ${wildcard src/c3/*.c}
C3OBJ 	= ${patsubst src/c3%,${OBJ}%,${C3SRC:.c=.o}}

CFLAGS	+= ${patsubst %,-I%,${subst :, ,${IPATH}}}


all: obj ${firmware} ${target}

include ${simavr}/Makefile.common

board = ${OBJ}/${target}.elf

${board} : ${C3OBJ}
${board} : ${OBJ}/mongoose.o
${board} : ${OBJ}/button.o
${board} : ${OBJ}/uart_pty.o
${board} : ${OBJ}/thermistor.o
${board} : ${OBJ}/heatpot.o
${board} : ${OBJ}/stepper.o
${board} : ${OBJ}/c_utils.o
${board} : ${OBJ}/${target}.o
${board} : ${OBJ}/${target}_gl.o

${target}: ${board}
	@echo $@ done

clean: clean-${OBJ}
	rm -rf *.a *.axf ${target} *.vcd

