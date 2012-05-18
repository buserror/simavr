#
# This makefile attempts to compile the vhci core module and the
# libusb-vhci library by downloading known tarballs, hacking them
# a little and compiling them
# 
# This has been tested on debian, but coild possibly work in other
# distro. the only "real" dependency is to have the linux kernel
# header symlinked at /lib/modules/<your host kernel version/build
#

HCD_V		= 1.14
VHCI_V 		= 0.6
BASE_URL	= http://downloads.sourceforge.net/project/usb-vhci
PWD			= ${shell pwd}

all: library

module: vhci-hcd-$(HCD_V)/usb-vhci-hcd.ko

vhci-hcd-$(HCD_V)/usb-vhci-hcd.ko: vhci-hcd-$(HCD_V)
	$(MAKE) -C vhci-hcd-$(HCD_V) \
		KDIR=/lib/modules/${shell uname -r}/build \
		CORE_INCLUDE_DIR=$(PWD)/vhci-hcd-$(HCD_V)/linux/2.6.32/drivers/usb/core

vhci-hcd-$(HCD_V).tar.bz2: 
	wget $(BASE_URL)/linux%20kernel%20module/$@

vhci-hcd-$(HCD_V): vhci-hcd-$(HCD_V).tar.bz2
	tar jxf $@.tar.bz2 && \
		cd $@ && \
		cat ../patches/hcd-*.patch | patch --merge -p1 
	sed -i -e 's|#define DEBUG|#undef DEBUG|' vhci-hcd-$(HCD_V)/usb-vhci-hcd.c

library: lib/libusb_vhci.a

lib/libusb_vhci.a: module libusb_vhci-$(VHCI_V)
	ln -snf vhci-hcd-$(HCD_V) linux && \
		cd libusb_vhci-$(VHCI_V) && \
		./configure --prefix=$(PWD) CPPFLAGS="-I$(PWD)" --enable-static --disable-shared && \
			$(MAKE) -j && $(MAKE) install

libusb_vhci-$(VHCI_V).tar.bz2:
	wget $(BASE_URL)/native%20libraries/libusb_vhci-$(VHCI_V).tar.bz2

libusb_vhci-$(VHCI_V): libusb_vhci-$(VHCI_V).tar.bz2
	tar jxf $@.tar.bz2 && \
		cd $@ && \
		cat ../patches/vhci-*.patch | patch --merge -p1 

clean:
	rm -rf libusb_vhci-$(VHCI_V) vhci-hcd-$(HCD_V) lib include linux

distclean: clean
	rm -rf *.tar.bz2
