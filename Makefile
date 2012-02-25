all:	make-simavr make-tests make-examples

make-simavr:
	make -C simavr

make-tests: make-simavr
	make -C tests

make-examples: make-simavr
	make -C examples

clean:
	make -C simavr clean
	make -C tests clean
	make -C examples clean
