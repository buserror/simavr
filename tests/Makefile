#
# This makefile takes each "at*" file, extracts it's part name
# And compiles it into an ELF binary.
# It also disassembles it for debugging purposes.
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


SHELL	 = /bin/bash

sources		:= $(wildcard at*.c)
test_sources    := $(wildcard test_*.c)
simavr = ..

IPATH += ${simavr}/include
IPATH += ${simavr}/simavr/sim
CFLAGS += -Wall
TEST_SRC=${wildcard test_*.c}
TESTS=${TEST_SRC:.c=}

all: obj ${sources:.c=.axf} ${TESTS}
# if you want hex files
#all : ${sources:.c=.hex}
# if you need assembler output
#all :  ${sources:.c=.s}  

include ../Makefile.common

# do not delete intermediate .o files after running `make run_tests'
.SECONDARY:

test_%: ${OBJ}/test_%.o ${OBJ}/tests.o ${simavr}/simavr/${OBJ}/libsimavr.so
	gcc ${LFLAGS} -o $@ $^ ${LDFLAGS}

run_tests: obj ${sources:.c=.axf} ${TESTS}
	./run_tests

clean: clean-${OBJ}
	rm -f ${TESTS} *.axf
