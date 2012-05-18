
boards_base=${wildcard board_*}
# remove vhci example on anything but linux
ifneq (${shell uname}, Linux)
boards=${subst board_usb,,$(boards_base)}
else
boards=$(boards_base)
endif

all:
	for bi in ${boards}; do $(MAKE) -C $$bi; done

clean:
	for bi in ${boards}; do $(MAKE) -C $$bi clean; done

