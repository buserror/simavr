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

CFLAGS	+= -O3  -mfpmath=sse -msse2 -Wall
#CFLAGS	+= -DCONFIG_SIMAVR_TRACE=1

cores	= ${wildcard cores/*.c}
cores_o = ${patsubst cores/%.c, obj/%.o, ${cores}}
sim		= ${wildcard sim/sim_*.c} ${wildcard sim/avr_*.c}
sim_o 	= ${patsubst sim/%.c, obj/%.o, ${sim}}

VPATH	= cores
VPATH	+= sim

IPATH	= sim
IPATH	+= ../../shared
IPATH	+= ../include

# Thats for MacPorts libelf
IPATH	+= /opt/local/include
LFLAGS	= -L/opt/local/lib/

all:	obj libsimavr.a
# shared library won't work that easily on non-linux
ifeq (${shell uname}, Linux)
all:	libsimavr.so
endif
all: 	${target}

obj/sim_%.o : cores/sim_%.c
	@gcc $(CFLAGS) -MD \
		-I${AVR_INC}/include/ \
		$<  -c -o $@
	@echo CORE $<

include ../Makefile.common

libsimavr.a	:	${cores_o}
libsimavr.a	:	${sim_o}
	@echo AR $@
	@ar cru $@ $^
	@ranlib $@

libsimavr.so	:	${cores_o}
libsimavr.so	:	${sim_o}
	@echo LD $@
	@gcc -shared -fPIC -o $@ $(LFLAGS) $^ $(LDFLAGS)

${target}	:	libsimavr.a
${target}	:	obj/${target}.o
	@gcc $(CFLAGS) $(LFLAGS) \
		${^} -o $@ \
		 $(LDFLAGS)
	@echo LD $@

clean:
	rm -rf ${target} obj *.a *.so
