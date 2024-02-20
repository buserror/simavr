#
# This makefile takes each "at*" file, extracts its part name
# and compiles it into an ELF binary.
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

sources		:= $(wildcard at*.c)
simavr 		= ..

IPATH 		+= ${simavr}/include
IPATH 		+= ${simavr}/simavr/sim

tests_src	:= ${wildcard test_*.c}

all: obj axf hex tst

include ../Makefile.common

tst: ${patsubst %.c, ${OBJ}/%.tst, ${tests_src}}

axf: ${sources:.c=.axf}

hex: atmega88_example.hex

${OBJ}/%.tst: tests.c %.c
ifneq ($(E),)
	@echo TST $@
endif
	${E}$(CC) -MMD ${CPPFLAGS} ${CFLAGS} ${LFLAGS} -o $@ ${patsubst %.h,, ${^}} $(LDFLAGS)

run_tests: all
	@export LD_LIBRARY_PATH=${simavr}/simavr/${OBJ} ;\
	num_failed=0 ;\
	num_run=0 ;\
	for test in ${OBJ}/test_*.tst; do \
	    num_run=$$(($$num_run+1)) ;\
	    if ! $$test; then \
			echo "$$test returned with exit value $$?." ;\
			num_failed=$$(($$num_failed+1)) ;\
	    fi ;\
	done ;\
	echo "Tests run: $$num_run  Successes: $$(($$num_run-$$num_failed))  Failures: $$num_failed" ;\
	exit $$num_failed

clean: clean-${OBJ}
	rm -f *.axf *.hex *.vcd
