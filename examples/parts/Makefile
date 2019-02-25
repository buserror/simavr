#	Copyright 2014,2017 Doug Szumski <d.s.szumski@gmail.com>
#	Copyright 2008-2011 Michel Pollet <buserror@gmail.com>
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

LDFLAGS += -lpthread
simavr = ../../
IPATH += ${simavr}/simavr/sim

target := libsimavrparts

all: obj ${target}

include ${simavr}/Makefile.common
include ../Makefile.opengl

objects := $(patsubst %.c,${OBJ}/%.o, $(wildcard *.c))

# Unless the VHCI_USB env var is set, don't build this because it requires
# a supporting library to be installed. See examples/extra_board_usb/README
# for details on how to build it.
ifndef VHCI_USB
	objects := $(filter-out %vhci_usb.o, $(objects))
endif

#
# Static library
#
${OBJ}/${target}.a: ${objects}
${target}: ${OBJ}/${target}.a
#
# Shared library (Linux only)
#
ifeq (${shell uname}, Linux)
${target}: ${OBJ}/${target}.so
endif

install: obj ${target}
	$(MKDIR) $(DESTDIR)/include/simavr/parts
	$(INSTALL) -m644 *.h $(DESTDIR)/include/simavr/parts/
	$(INSTALL) ${OBJ}/libsimavrparts.a $(DESTDIR)/lib/
	sed -e "s|PREFIX|${PREFIX}|g" -e "s|VERSION|${SIMAVR_VERSION}|g" \
		simavrparts.pc >$(DESTDIR)/lib/pkgconfig/simavrparts.pc
ifeq (${shell uname}, Linux)
	$(INSTALL) ${OBJ}/libsimavrparts.so.1 $(DESTDIR)/lib/
	ln -sf libsimavrparts.so.1 $(DESTDIR)/lib/libsimavrparts.so
endif

clean: clean-${OBJ}
	rm -rf *.hex *.a *.axf *.vcd .*.swo .*.swp .*.swm .*.swn *.so *.o
