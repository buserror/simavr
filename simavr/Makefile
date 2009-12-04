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

ifeq (${shell uname}, Darwin)
AVR_ROOT := "/Applications/Arduino.app/Contents/Resources/Java/hardware/tools/avr/avr-4/"
else
AVR_ROOT := /usr/lib/avr
endif

CFLAGS	=  -g -std=gnu99 -Wall
CFLAGS	+= -O3  -mfpmath=sse -msse2

cores	= ${wildcard cores/*.c}
cores_o = ${patsubst cores/%.c, obj/%.o, ${cores}}
sim		= ${wildcard sim/sim_*.c} ${wildcard sim/avr_*.c}
sim_o 	= ${patsubst sim/%.c, obj/%.o, ${sim}}

VPATH	= .
VPATH	+= cores
VPATH	+= sim

IPATH	= .
IPATH	+= sim
IPATH	+= ../../shared
IPATH	+= ../include
IPATH	+= /opt/local/include

CFLAGS	+= ${patsubst %,-I%,${subst :, ,${IPATH}}}
LFLAGS	= -L/opt/local/lib/
LDFLAGS	+= -lelf 

all:	obj ${target} libsimavr.a

obj:
	@mkdir -p obj

obj/sim_%.o : cores/sim_%.h ${wildcard cores/*.h} ${wildcard sim/*.h}
obj/sim_%.o : cores/sim_%.c
	@gcc $(CFLAGS) \
		-I${AVR_ROOT}/include/ \
		$<  -c -o $@
	@echo CORE $<

obj/%.o: %.h sim/*.h
obj/%.o: %.c
	@gcc $(CFLAGS) \
		$<  -c -o $@
	@echo CC $<

libsimavr.a	:	${cores_o}
libsimavr.a	:	${sim_o}
	@echo AR $@
	@ar cru $@ $^
	@ranlib $@

${target}	:	${cores_o}
${target}	:	${sim_o}
${target}	:	obj/${target}.o
	@gcc $(CFLAGS) $(LFLAGS) \
		${^} -o $@ \
		 $(LDFLAGS)
	@echo LD $@

clean:
	rm -rf ${target} obj *.a

