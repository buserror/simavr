
boards_base=${wildcard board_*}
boards=$(boards_base)

all:
	for bi in ${boards}; do $(MAKE) -C $$bi; done

clean:
	for bi in ${boards}; do $(MAKE) -C $$bi clean; done

#
# The USB example is not made by default, as it downloads stuff
# for the vhci library, it fails the debian policy on being able
# to build offline.
#
extra_board_usb:
	make -C extra_board_usb
