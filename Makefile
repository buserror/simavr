# Makefile

# Normal build will link against the shared library for simavr
# in the current build tree, so you don't have to 'install' to
# run simavr or the examples.
#
# For package building, you will need to pass RELEASE=1 to make
RELEASE	?= 0

DESTDIR = /usr/local
PREFIX = ${DESTDIR}
ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
SIMAVR_VERSION	:= ${shell \
	git describe --abbrev=0 --tags || \
	echo "unknown" }


.PHONY: doc debug

all:	debug build-simavr build-tests build-examples build-parts

debug:
	$(MAKE) -f Makefile.common debug SIMAVR_VERSION=$(SIMAVR_VERSION) ROOT_DIR=$(ROOT_DIR)

build-simavr:
	$(MAKE) -C simavr RELEASE=$(RELEASE) SIMAVR_VERSION=$(SIMAVR_VERSION) ROOT_DIR=$(ROOT_DIR)

build-tests: build-simavr
	$(MAKE) -C tests RELEASE=$(RELEASE) SIMAVR_VERSION=$(SIMAVR_VERSION) ROOT_DIR=$(ROOT_DIR)

build-examples: build-simavr
	$(MAKE) -C examples RELEASE=$(RELEASE) SIMAVR_VERSION=$(SIMAVR_VERSION) ROOT_DIR=$(ROOT_DIR)

build-parts: build-examples
	$(MAKE) -C examples/parts RELEASE=$(RELEASE) SIMAVR_VERSION=$(SIMAVR_VERSION) ROOT_DIR=$(ROOT_DIR)

install: install-simavr install-parts

install-simavr:
	$(MAKE) -C simavr install RELEASE=$(RELEASE) DESTDIR=$(DESTDIR) PREFIX=$(PREFIX) SIMAVR_VERSION=$(SIMAVR_VERSION) ROOT_DIR=$(ROOT_DIR)

install-parts:
	$(MAKE) -C examples/parts install RELEASE=$(RELEASE) DESTDIR=$(DESTDIR) PREFIX=$(PREFIX) SIMAVR_VERSION=$(SIMAVR_VERSION) ROOT_DIR=$(ROOT_DIR)

doc:
	$(MAKE) -C doc RELEASE=$(RELEASE) SIMAVR_VERSION=$(SIMAVR_VERSION) ROOT_DIR=$(ROOT_DIR)

clean:
	$(MAKE) -C simavr clean
	$(MAKE) -C tests clean
	$(MAKE) -C examples clean
	$(MAKE) -C examples/parts clean
	$(MAKE) -C doc clean

