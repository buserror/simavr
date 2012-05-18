all:	$(MAKE)-simavr $(MAKE)-tests $(MAKE)-examples

$(MAKE)-simavr:
	$(MAKE) -C simavr

$(MAKE)-tests: $(MAKE)-simavr
	$(MAKE) -C tests

$(MAKE)-examples: $(MAKE)-simavr
	$(MAKE) -C examples

clean:
	$(MAKE) -C simavr clean
	$(MAKE) -C tests clean
	$(MAKE) -C examples clean
