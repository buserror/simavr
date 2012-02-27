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

SIMAVR_VERSION	= 1.0a10
SIMAVR_REVISION	= 1

target	= run_avr

CFLAGS	+= -O3 -Wall -Werror

# tracing id useful especialy if you develop simavr core.
# it otherwise eat quite a bit of few cycles, even disabled
#CFLAGS	+= -DCONFIG_SIMAVR_TRACE=1

all:	obj ${target}

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
ifeq (${shell test -d /opt/local && echo Exists}, Exists)
IPATH	+= /opt/local/include
LFLAGS	= -L/opt/local/lib/
endif

#
# Static library
#
${OBJ}/libsimavr.a	:	${cores_o}
${OBJ}/libsimavr.a	:	${sim_o}
	@echo AR $@
	@$(AR) cru $@ $^ && $(RANLIB) $@

#
# Shared library (Linux)
#
${OBJ}/libsimavr.so.1	:	${cores_o}
${OBJ}/libsimavr.so.1	:	${sim_o}
	@echo SHARED $@
	@$(CC) -shared -Wl,-soname,libsimavr.so.1 -o $@ $^

${OBJ}/libsimavr.so		: ${OBJ}/libsimavr.so.1
	ln -sf libsimavr.so.1 $@

libsimavr	: ${OBJ}/libsimavr.a
# shared library won't work that easily on non-linux
ifeq (${shell uname}, Linux)
libsimavr	:	${OBJ}/libsimavr.so
endif

${OBJ}/${target}.o		: libsimavr 
${OBJ}/${target}.elf	: ${OBJ}/${target}.o

${target}	: ${OBJ}/${target}.elf
	ln -sf $< $@
 
clean: clean-${OBJ}
	rm -rf ${target} *.a *.so

DESTDIR = /usr/local

install : all
	$(MKDIR) $(DESTDIR)/include/simavr/avr
	$(INSTALL) sim/*.h $(DESTDIR)/include/simavr/
	$(INSTALL) ../include/*.h $(DESTDIR)/include/simavr/avr/
	$(MKDIR) $(DESTDIR)/lib
	$(INSTALL) ${OBJ}/libsimavr.a $(DESTDIR)/lib/
	$(MKDIR) $(DESTDIR)/lib/pkgconfig/
	sed -e "s|PREFIX|${DESTDIR}|g" -e "s|VERSION|${SIMAVR_VERSION}|g" \
		simavr.pc >$(DESTDIR)/lib/pkgconfig/simavr.pc
ifeq (${shell uname}, Linux)
	$(INSTALL) ${OBJ}/libsimavr.so.1 $(DESTDIR)/lib/
	ln -sf libsimavr.so.1 $(DESTDIR)/lib/libsimavr.so
endif
	$(MKDIR) $(DESTDIR)/bin
	$(INSTALL) ${OBJ}/${target}.elf $(DESTDIR)/bin/simavr
