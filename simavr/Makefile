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

SHELL 			:= ${shell which bash}
SIMAVR_VERSION	:= ${shell \
	{ git log -1 --tags --simplify-by-decoration --pretty="format:%d"|\
		sed -e 's/[\(\) ]//g'; } || \
	echo "unknown" }
SIMAVR_REVISION	= 2

target	= run_avr

CFLAGS	+= -Werror
# tracing is useful especialy if you develop simavr core.
# it otherwise eat quite a bit of few cycles, even disabled
#CFLAGS	+= -DCONFIG_SIMAVR_TRACE=1

all:
	$(MAKE) obj config
	$(MAKE) libsimavr ${target}

include ../Makefile.common

cores	:= ${wildcard cores/*.c}
sim		:= ${wildcard sim/sim_*.c} ${wildcard sim/avr_*.c}
sim_o 	:= ${patsubst sim/%.c, ${OBJ}/%.o, ${sim}}

VPATH	= cores
VPATH	+= sim

IPATH	+= sim
IPATH	+= .
IPATH	+= ../../shared
    
#
# Static library
#
${OBJ}/libsimavr.a	:	${sim_o}
ifeq ($(V),1)
	$(AR) cru $@ $^ && $(RANLIB) $@
else
	@echo AR $@
	@$(AR) cru $@ $^ && $(RANLIB) $@
endif

#
# Shared library (Linux)
#
${OBJ}/libsimavr.so.1	:	${sim_o}
ifeq ($(V),1)
	$(CC) -shared -Wl,-soname,libsimavr.so.1 -o $@ $^ ${filter-out -lsimavr, $(LDFLAGS)}
else
	@echo SHARED $@
	@$(CC) -shared -Wl,-soname,libsimavr.so.1 -o $@ $^ ${filter-out -lsimavr, $(LDFLAGS)}
endif

${OBJ}/libsimavr.so		: ${OBJ}/libsimavr.so.1
	ln -sf libsimavr.so.1 $@

libsimavr	: config ${OBJ}/libsimavr.a
# shared library won't work that easily on non-linux
ifeq (${shell uname}, Linux)
libsimavr	:	${OBJ}/libsimavr.so
endif

${OBJ}/${target}.elf	: ${OBJ}/${target}.o

${target}	: ${OBJ}/${target}.elf

# FIXME uname -o doesn't work on BSD
#ifeq (${shell uname -o}, Msys)
#	ln -sf $< $@.exe
#else
	ln -sf $< $@
#endif
 
clean: clean-${OBJ}
	rm -rf ${target} *.a *.so *.exe
	rm -f sim_core_*.h

DESTDIR = /usr/local
PREFIX = ${DESTDIR}

install : all
	$(MKDIR) $(DESTDIR)/include/simavr/avr
	$(INSTALL) -m644 sim/*.h $(DESTDIR)/include/simavr/
	$(INSTALL) -m644 sim_core_*.h $(DESTDIR)/include/simavr/
	$(INSTALL) -m644 sim/avr/*.h $(DESTDIR)/include/simavr/avr/
	$(MKDIR) $(DESTDIR)/lib
	$(INSTALL) ${OBJ}/libsimavr.a $(DESTDIR)/lib/
	$(MKDIR) $(DESTDIR)/lib/pkgconfig/
	sed -e "s|PREFIX|${PREFIX}|g" -e "s|VERSION|${SIMAVR_VERSION}|g" \
		simavr-avr.pc >$(DESTDIR)/lib/pkgconfig/simavr-avr.pc
	sed -e "s|PREFIX|${PREFIX}|g" -e "s|VERSION|${SIMAVR_VERSION}|g" \
		simavr.pc >$(DESTDIR)/lib/pkgconfig/simavr.pc
ifeq (${shell uname}, Linux)
	$(INSTALL) ${OBJ}/libsimavr.so.1 $(DESTDIR)/lib/
	ln -sf libsimavr.so.1 $(DESTDIR)/lib/libsimavr.so
endif
	$(MKDIR) $(DESTDIR)/bin
	$(INSTALL) ${OBJ}/${target}.elf $(DESTDIR)/bin/simavr

# Needs 'fpm', oneline package manager. Install with 'gem install fpm'
# This generates 'mock' debian files, without all the policy, scripts
# and all the things normal debian requires. If debian decides to roll
# in simavr officialy, this section shall diseapear
debian: 
	rm -rf /tmp/simavr-tmp && mkdir -p /tmp/simavr-tmp/usr && \
	make cleam; make install DESTDIR=/tmp/simavr-tmp/usr RELEASE=1 && \
	(cd /tmp/simavr-tmp && \
	fpm -s dir -t deb -C /tmp/simavr-tmp -n libsimavr -v $(SIMAVR_VERSION) \
		--iteration $(SIMAVR_REVISION) \
		-d 'libelf' \
        --description "lean and mean Atmel AVR simulator: Runtime library" \
        usr/lib/lib*.so* && \
	fpm -s dir -t deb -C /tmp/simavr-tmp -n libsimavr-dev -v $(SIMAVR_VERSION) \
		--iteration $(SIMAVR_REVISION) \
		-d 'libsimavr >= $(SIMAVR_VERSION)' \
        --description "lean and mean Atmel AVR simulator: Development files" \
        usr/include usr/lib/lib*.a usr/lib/pkgconfig && \
	fpm -s dir -t deb -C /tmp/simavr-tmp -n simavr -v $(SIMAVR_VERSION) \
		--iteration $(SIMAVR_REVISION) \
		-d 'libsimavr >= $(SIMAVR_VERSION)' \
        --description "lean and mean Atmel AVR simulator: Executable" \
        usr/bin ) && \
	echo Done

config: ${OBJ}/cores.deps sim_core_config.h sim_core_decl.h

#
# this tries to preprocess all the cores and decide
# which ones have a chance of compiling at all. Some platform
# ship with old version of avr-gcc and associated and needs
# to be verified
#
ifeq ($(V), 1)
DEBUGLOG=/tmp/simavr_conf.log
else
DEBUGLOG=/dev/null
endif
sim_core_config.h ${OBJ}/cores.deps: $(cores) Makefile
	@echo CONF $@
	@conf=""; decl=""; array=""; \
	mkdir -p ${OBJ} ; echo >${OBJ}/cores.deps ; echo >$(DEBUGLOG) ;\
	for core in cores/*.c ; do \
		file=$$core; global=$${core/cores\/sim_}; global=$${global/.c}; \
		upper=$$(echo $$global|tr '[a-z]' '[A-Z]'); \
		if $(CC) -E $(CFLAGS) ${AVR_CPPFLAGS} $$file \
			>>$(DEBUGLOG) 2>&1 ; then \
			conf+="#define CONFIG_$$upper 1\n"; \
			obj=$${file/.c/.o} ; obj=$${obj/cores\/}; \
			printf "\$${OBJ}/libsimavr.a: \$${OBJ}/$$obj\n">>${OBJ}/cores.deps ; \
			printf "\$${OBJ}/libsimavr.so.1: \$${OBJ}/$$obj\n">>${OBJ}/cores.deps ; \
		else \
			echo WARNING $$file did not compile, check your avr-gcc toolchain; \
		fi \
	done ; \
	( printf "// Autogenerated do not edit\n"; \
	  printf "#ifndef __SIM_CORE_CONFIG_H__\n#define __SIM_CORE_CONFIG_H__\n\n"; \
	  printf "#define CONFIG_SIMAVR_VERSION \"${SIMAVR_VERSION}\"\n"; \
	  printf "$$conf\n"; \
	  printf "#endif\n"; \
	) >sim_core_config.h

#
# This take the config file that was generated, and create the static
# table of all available cores for name lookups, as well as a C
# config file
#  
sim_core_decl.h: sim_core_config.h $(cores) Makefile
	@echo CONF $@
	@decl=""; array=""; \
	for core in $$(grep -r avr_kind_t cores/|awk -F '[ :]' '{print $$1 "=" $$3;}') ; do \
		file=$${core/=*}; global=$${core/*=}; \
		upper=$$global; upper=$${upper/.c}; \
		upper=$$(echo $$upper|tr '[a-z]' '[A-Z]'); \
		decl+="#if CONFIG_$$upper\nextern avr_kind_t $$global;\n#endif\n"; \
		array+="#if CONFIG_$$upper\n\t&$$global,\n#endif\n"; \
	done ; \
	( printf "// Autogenerated do not edit\n"; \
	  printf "#ifndef __SIM_CORE_DECL_H__\n#define __SIM_CORE_DECL_H__\n\n"; \
	  printf "#include \"sim_core_config.h\"\n";\
	  printf "$$decl\n" ;  \
	  printf "extern avr_kind_t * avr_kind[];\n"; \
	  printf "#ifdef AVR_KIND_DECL\navr_kind_t * avr_kind[] = {\n$$array\tNULL\n};\n#endif\n"; \
	  printf "#endif\n"; \
	) >sim_core_decl.h

-include ${OBJ}/cores.deps
