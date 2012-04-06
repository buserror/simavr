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

target=	i2ctest
firm_src = ${wildcard at*${board}.c}
firmware = ${firm_src:.c=.axf}
simavr = ../..
#simavr = /usr/local/lib/simavr

SHELL	 = /bin/bash

IPATH = .
#IPATH += ../parts
IPATH += ${simavr}/examples/parts
IPATH += ${simavr}/include
IPATH += ${simavr}/simavr/sim

VPATH = .
VPATH += ../parts
VPATH += ../shared


all: obj atmega1280_${target}.axf ${target} 

include ${simavr}/Makefile.common

atmega1280_${target}.axf: atmega1280_${target}.c
atmega1280_${target}.axf: avr_twi_master.c avr_twi_master.h

board = ${OBJ}/${target}.elf

${board} : ${OBJ}/${target}.o
${board} : ${OBJ}/i2c_eeprom.o
${board} : ${simavr}/simavr/${OBJ}/libsimavr.a

${target}: ${board}
	@echo $@ done

clean: clean-${OBJ}
	rm -rf *.hex *.a *.axf ${target} *.vcd .*.swo .*.swp .*.swm .*.swn
