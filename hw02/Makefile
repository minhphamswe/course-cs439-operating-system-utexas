CC = gcc
CXX = g++
LD = ld
CFLAGS = -Wall -g
LDFLAGS = -lpthread

.PHONY: c cpp
.PHONY: clean

threading: threading-c threading-cpp

c: threading-c
threading-c: threading.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

cpp: threading-cpp
threading-cpp: threading.c
	$(CXX) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f threading-c threading-cpp *.o *~