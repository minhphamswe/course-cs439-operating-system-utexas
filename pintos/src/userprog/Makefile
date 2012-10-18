include ../Makefile.kernel

PROGS = cat cmp cp echo halt hex-dump ls mcat mcp mkdir pwd rm shell \
	bubsort insult lineup matmult recursor
FS = ./build/filesys.dsk

.PHONY: disk

disk: all
	rm -f $(FS)
	pintos-mkdisk $(FS) --filesys-size=2
	pintos -v -f -q
	for prog in $(PROGS); do \
		pintos -v -p ../examples/$$prog -a $$prog -- -q; \
	done
