

all:	make-tests
	make -C simavr && make -C examples

make-tests:
	make -C tests

clean:
	make -C simavr clean
	make -C tests clean
	make -C examples clean
