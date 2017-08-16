# Makefile

# Normal build will link against the shared library for simavr
# in the current build tree, so you don't have to 'install' to
# run simavr or the examples.
#
# For package building, you will need to pass RELEASE=1 to make
RELEASE	?= 0

.PHONY: doc

all:	build-simavr build-tests build-examples build-parts

build-simavr:
	$(MAKE) -C simavr RELEASE=$(RELEASE)

build-tests: build-simavr
	$(MAKE) -C tests RELEASE=$(RELEASE)

build-examples: build-simavr
	$(MAKE) -C examples RELEASE=$(RELEASE)

build-parts: build-examples
	$(MAKE) -C examples/parts RELEASE=$(RELEASE)

install:
	$(MAKE) -C simavr install RELEASE=$(RELEASE)

doc:
	$(MAKE) -C doc RELEASE=$(RELEASE)

clean:
	$(MAKE) -C simavr clean
	$(MAKE) -C tests clean
	$(MAKE) -C examples clean
	$(MAKE) -C examples/parts clean
	$(MAKE) -C doc clean

