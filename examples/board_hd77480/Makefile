# 
# 	Copyright 2008-2011 Michel Pollet <buserror@gmail.com>
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

board=	charlcd
firm_src = ${wildcard at*${board}.c}
firmware = ${firm_src:.c=.axf}
simavr = ../../

SHELL	 = /bin/bash

IPATH = .
IPATH += ../parts
IPATH += ${simavr}/include
IPATH += ${simavr}/simavr/sim

VPATH = .
VPATH += ../parts

LDFLAGS += -lpthread
ifneq (${shell uname}, Darwin)
LDFLAGS += -lGL -lglut
else
LDFLAGS += -framework GLUT -framework OpenGL 
endif

all: obj atmega48_charlcd.axf ${board} 

atmega48_charlcd.axf: atmega48_charlcd.c

include ${simavr}/Makefile.common

${board} : ${simavr}/simavr/libsimavr.a
${board} : ${OBJ}/ac_input.o
${board} : ${OBJ}/hd44780.o
${board} : ${OBJ}/hd44780_glut.o
${board} : ${OBJ}/${board}.o
	@echo LD $@
	@gcc -MD ${CFLAGS}  ${LFLAGS} -o $@ $^ $(LDFLAGS)

clean:
	rm -rf obj *.hex *.a *.axf ${board} *.vcd .*.swo .*.swp .*.swm .*.swn
