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

target	= run_avr

CFLAGS	+= -O3 -Wall -Werror

# tracing id useful especialy if you develop simavr core.
# it otherwise eat quite a bit of few cycles, even disabled
#CFLAGS	+= -DCONFIG_SIMAVR_TRACE=1

all:	obj libsimavr ${target}

include ../Makefile.common

cores	= ${wildcard cores/*.c}
cores_o = ${patsubst cores/%.c, ${OBJ}/%.o, ${cores}}
sim		= ${wildcard sim/sim_*.c} ${wildcard sim/avr_*.c}
sim_o 	= ${patsubst sim/%.c, ${OBJ}/%.o, ${sim}}

VPATH	= cores
VPATH	+= sim

IPATH	= sim
IPATH	+= ../../shared
IPATH	+= ../include

# Thats for MacPorts libelf
IPATH	+= /opt/local/include
LFLAGS	= -L/opt/local/lib/

# shared library won't work that easily on non-linux
ifeq (${shell uname}, Linux)
all:	${OBJ}/libsimavr.so
endif


${OBJ}/libsimavr.a	:	${cores_o}
${OBJ}/libsimavr.a	:	${sim_o}
	@echo AR $@
	@$(AR) cru $@ $^ && $(RANLIB) $@

libsimavr: ${OBJ}/libsimavr.a
 
${OBJ}/libsimavr.so	:	${cores_o}
${OBJ}/libsimavr.so	:	${sim_o}
	@echo LD $@
	@$(CC) -shared -fPIC -o $@ $(LFLAGS) $^ $(LDFLAGS)

${OBJ}/${target}.elf	:	${OBJ}/${target}.o
${OBJ}/${target}.elf	:	${OBJ}/libsimavr.a

${target}	: ${OBJ}/${target}.elf
	ln -sf $< $@
 
clean: clean-${OBJ}
	rm -rf ${target} *.a *.so
