
boards_base=${wildcard board_*}
# remove vhci example on anything but linux
ifneq (${shell uname}, Linux)
boards=${subst board_usb,,$(boards_base)}
else
boards=$(boards_base)
endif

all:
	for bi in ${boards}; do make -C $$bi; done

clean:
	for bi in ${boards}; do make -C $$bi clean; done

