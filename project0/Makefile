# Makefile for the CS:APP Shell Lab

TEAM = NOBODY
VERSION = 1
DRIVER = ./sdriver.pl
MSH = ./msh
MSHREF = ./mshref
MSHARGS = "-p"
CC = gcc
CFLAGS = -Wall -O2 -g -pg
FILES = $(MSH) ./myspin ./mysplit ./mystop ./myint ./fib ./handle ./mykill \
        ./psh

all: $(FILES)

msh: msh.o util.o jobs.o
	$(CC) $(CFLAGS) msh.o util.o jobs.o -o msh

psh: psh.o util.o 
	$(CC) $(CFLAGS) psh.o util.o -o psh

handle: handle.o util.o
	$(CC) $(CFLAGS) handle.o util.o -o handle


##################
# Handin your work
##################
TAID = "yjkwon"

turnin.tar: clean
	tar cf turnin.tar `find . -type f | grep -v '^\.*$$' | grep -v '/CVS/' | grep -v '/\.svn/' | grep -v '/\.git/' | grep -v 'lab[0-9].*\.tar\.gz' | grep -v '/\~/' | grep -v 'mshref' | grep -v '/\.txt' | grep -v '/\.pl' |grep -v '/\.tar/'` 

TURNIN := /lusr/bin/turnin
GRADER := yjkwon
LAB1_NAME := proj0_norman

turnin: turnin.tar
	echo "Turning in turnin.tar containing the following files:"
	tar tf turnin.tar
	$(TURNIN) --submit $(GRADER) $(LAB1_NAME) turnin.tar


##################
# Regression tests
##################

# Run tests using the student's shell program
test01:
	$(DRIVER) -t trace01.txt -s $(MSH) -a $(MSHARGS)
test02:
	$(DRIVER) -t trace02.txt -s $(MSH) -a $(MSHARGS)
test03:
	$(DRIVER) -t trace03.txt -s $(MSH) -a $(MSHARGS)
test04:
	$(DRIVER) -t trace04.txt -s $(MSH) -a $(MSHARGS)
test05:
	$(DRIVER) -t trace05.txt -s $(MSH) -a $(MSHARGS)
test06:
	$(DRIVER) -t trace06.txt -s $(MSH) -a $(MSHARGS)
test07:
	$(DRIVER) -t trace07.txt -s $(MSH) -a $(MSHARGS)
test08:
	$(DRIVER) -t trace08.txt -s $(MSH) -a $(MSHARGS)
test09:
	$(DRIVER) -t trace09.txt -s $(MSH) -a $(MSHARGS)
test10:
	$(DRIVER) -t trace10.txt -s $(MSH) -a $(MSHARGS)
test11:
	$(DRIVER) -t trace11.txt -s $(MSH) -a $(MSHARGS)
test12:
	$(DRIVER) -t trace12.txt -s $(MSH) -a $(MSHARGS)
test13:
	$(DRIVER) -t trace13.txt -s $(MSH) -a $(MSHARGS)
test14:
	$(DRIVER) -t trace14.txt -s $(MSH) -a $(MSHARGS)
test15:
	$(DRIVER) -t trace15.txt -s $(MSH) -a $(MSHARGS)
test16:
	$(DRIVER) -t trace16.txt -s $(MSH) -a $(MSHARGS)

# Run the tests using the reference shell program
rtest01:
	$(DRIVER) -t trace01.txt -s $(MSHREF) -a $(MSHARGS)
rtest02:
	$(DRIVER) -t trace02.txt -s $(MSHREF) -a $(MSHARGS)
rtest03:
	$(DRIVER) -t trace03.txt -s $(MSHREF) -a $(MSHARGS)
rtest04:
	$(DRIVER) -t trace04.txt -s $(MSHREF) -a $(MSHARGS)
rtest05:
	$(DRIVER) -t trace05.txt -s $(MSHREF) -a $(MSHARGS)
rtest06:
	$(DRIVER) -t trace06.txt -s $(MSHREF) -a $(MSHARGS)
rtest07:
	$(DRIVER) -t trace07.txt -s $(MSHREF) -a $(MSHARGS)
rtest08:
	$(DRIVER) -t trace08.txt -s $(MSHREF) -a $(MSHARGS)
rtest09:
	$(DRIVER) -t trace09.txt -s $(MSHREF) -a $(MSHARGS)
rtest10:
	$(DRIVER) -t trace10.txt -s $(MSHREF) -a $(MSHARGS)
rtest11:
	$(DRIVER) -t trace11.txt -s $(MSHREF) -a $(MSHARGS)
rtest12:
	$(DRIVER) -t trace12.txt -s $(MSHREF) -a $(MSHARGS)
rtest13:
	$(DRIVER) -t trace13.txt -s $(MSHREF) -a $(MSHARGS)
rtest14:
	$(DRIVER) -t trace14.txt -s $(MSHREF) -a $(MSHARGS)
rtest15:
	$(DRIVER) -t trace15.txt -s $(MSHREF) -a $(MSHARGS)
rtest16:
	$(DRIVER) -t trace16.txt -s $(MSHREF) -a $(MSHARGS)


# clean up
clean:
	rm -f $(FILES) *.o *~ *.bak *.BAK



