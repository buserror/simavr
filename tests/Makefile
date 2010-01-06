#
# This makefile take each "at*" file, extracts it's part name
# And compile it into an ELF binary.
# It also disassemble it for debugging purposes.
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

all :  ${sources:.c=.axf}
# if you want hex files
#all : ${sources:.c=.hex}
# if you need assembler output
#all :  ${sources:.c=.s}  

include ../Makefile.common

clean:
	rm -f *.hex *.o *.axf *.s
