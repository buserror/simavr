
boards_base=${wildcard board_*}
# Remove simduino until FileMapping is implemented to work around missing mmap in Win32
ifeq (${shell uname -o}, Msys)
boards=${subst board_usb,,${subst board_simduino,,$(boards_base)}}
else
boards=$(boards_base)
endif

all:
	for bi in ${boards}; do $(MAKE) -C $$bi; done

clean:
	for bi in ${boards}; do $(MAKE) -C $$bi clean; done
