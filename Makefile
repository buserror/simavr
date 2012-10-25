.PHONY: doc

all:	build-simavr build-tests build-examples

build-simavr:
	$(MAKE) -C simavr

build-tests: build-simavr
	$(MAKE) -C tests

build-examples: build-simavr
	$(MAKE) -C examples

install:
	$(MAKE) -C simavr install

doc:
	$(MAKE) -C doc

clean:
	$(MAKE) -C simavr clean
	$(MAKE) -C tests clean
	$(MAKE) -C examples clean
	$(MAKE) -C doc clean

