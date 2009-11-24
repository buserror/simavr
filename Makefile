

all:	make-tests
	make -C simavr

make-tests:
	make -C tests

clean:
	make -C simavr clean
	make -C tests clean
	