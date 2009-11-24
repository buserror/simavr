#
# This makefile take each "at*" file, extracts it's part name
# And compile it into an ELF binary.
# It also disassemble it for debugging purposes.
# 
# The code is compiled "optimized" to the max.
# 
# The wierd "-Wl,--undefined=_mmcu,--section-start=.mmcu=0x910000"
# is used to tell the linker not to discard the .mmcu section,
# otherwise the --gc-sections will delete it.
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

sources		:= $(wildcard at*.c)

all :  ${sources:.c=.axf} ${sources:.c=.hex}  ${sources:.c=.s}  

%.hex: %.axf
		@avr-objcopy -j .text -j .data -O ihex ${<} ${@}

%.s: %.axf
		@avr-objdump -j .text -j .data -j .bss -d  ${<} > ${@}

%.axf: %.c 
		@echo CC ${<}
		@part=${<} ; part=$${part/_*}; \
		avr-gcc -Wall -g -Os -std=gnu99 \
				-mmcu=$$part \
				-DF_CPU=8000000 \
				-mcall-prologues -fno-inline-small-functions \
				-ffunction-sections -fdata-sections \
				-Wl,--relax,--gc-sections \
				-Wl,--undefined=_mmcu,--section-start=.mmcu=0x910000 \
				-I../include \
				${<} -o ${@}
		@avr-size ${@}|sed '1d'

clean:
	rm -f *.hex *.o *.axf *.s
