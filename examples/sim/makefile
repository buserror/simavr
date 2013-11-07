# 
# 	Copyright 2008-2012 Michel Pollet <buserror@gmail.com>
# 	modified by Yann GOUY <yann_gouy@yahoo.fr>
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

targets = mpu6050.so eeprom_i2c.so
simavr = ../..

IPATH = .
IPATH += ${simavr}/include
IPATH += ${simavr}/simavr/sim
IPATH += ${simavr}/examples/parts

VPATH = .
VPATH += ${simavr}/examples/shared
VPATH += ${simavr}/examples/parts

CFLAGS += -shared


all: ${targets} 

include ${simavr}/Makefile.common

%.so: %.c
	$(CC) -MMD ${CPPFLAGS} ${CFLAGS}  ${LFLAGS} -o $@ $^ $(LDFLAGS)


clean:
	rm -rf *.so .*.swo .*.swp .*.swm .*.swn


