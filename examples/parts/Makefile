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
static_lib = $(target).a

${OBJ}/${static_lib} : ${objects}
${OBJ}/${static_lib} :
	$(AR) cru $@ $^
${target}: ${OBJ}/${static_lib}

#
# Shared library (Linux only)
#
shared_lib = ${target}.so.1

${OBJ}/${shared_lib} : ${objects}
${OBJ}/${shared_lib} :
	${CC} -shared -Wl,-soname,${shared_lib} $^ -o $@
ifeq (${shell uname}, Linux)
${target}: ${OBJ}/${shared_lib}
endif
	@echo $@ done

clean: clean-${OBJ}
	rm -rf *.hex *.a *.axf *.vcd .*.swo .*.swp .*.swm .*.swn *.so *.o
