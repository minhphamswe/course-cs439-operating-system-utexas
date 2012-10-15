.PHONY: all

TOOLSDIR=$(shell cd ./tools; pwd)

all:	qemu-1.1.2
	cd qemu-1.1.2 && ./configure '--target-list=i386-softmmu' '--enable-debug' '--enable-kvm' --prefix=$(TOOLSDIR)
	make -C qemu-1.1.2
	make -C qemu-1.1.2 install

qemu-1.1.2: qemu-1.1.2.tar.bz2
	tar -xvjf qemu-1.1.2.tar.bz2

