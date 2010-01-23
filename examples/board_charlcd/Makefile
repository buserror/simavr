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
LDFLAGS += -lGLU
LDFLAGS += -lGL
LDFLAGS += `sdl-config --libs`
LDFLAGS += -lSDL_image

CFLAGS = `sdl-config --cflags`
#ifneq (${shell uname}, Darwin)
#else
#LDFLAGS += -framework GLUT -framework OpenGL 
#endif
all: obj atmega48_charlcd.axf ${board} 

atmega48_charlcd.axf: atmega48_charlcd.c atmega48_lcd.c atmega48_lcd.h
	@echo AVR-CC ${<}
	${AVR}gcc -Wall -gdwarf-2 -Os -std=gnu99 \
			-mmcu=atmega48 \
			-DF_CPU=8000000 \
			-fno-inline-small-functions \
			-ffunction-sections -fdata-sections \
			-Wl,--relax,--gc-sections \
			-Wl,--undefined=_mmcu,--section-start=.mmcu=0x910000 \
			-I../include -I../../include \
			atmega48_charlcd.c atmega48_lcd.c -o atmega48_charlcd.axf
	@${AVR}size ${@}|sed '1d'

include ${simavr}/Makefile.common

${board} : ${OBJ}/ac_input.o
${board} : ${OBJ}/hd44780.o
${board} : ${OBJ}/hd44780_glut.o
${board} : ${OBJ}/${board}.o
	@echo LD $@
	@gcc -MD ${CFLAGS}  ${LFLAGS} -o $@ $^ $(LDFLAGS) ${simavr}/simavr/libsimavr.a

clean:
	rm -rf obj *.hex *.a *.axf ${board} *.vcd .*.swo .*.swp .*.swm .*.swn
