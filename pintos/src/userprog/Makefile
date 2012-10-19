include ../Makefile.kernel

PROGDIR = ../examples
PROGS = cat cmp cp echo halt hex-dump ls mcat mcp mkdir pwd rm shell \
	bubsort insult lineup matmult recursor
FS = ./build/filesys.dsk


.PHONY: disk

disk: all
	make -C $(PROGDIR) all
	rm -f $(FS)
	pintos-mkdisk $(FS) --filesys-size=2
	pintos -f -q
	for prog in $(PROGS); do \
		pintos -v -p $(PROGDIR)/$$prog -a $$prog -- -q; \
	done
