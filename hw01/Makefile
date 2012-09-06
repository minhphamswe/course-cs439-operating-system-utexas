CC = gcc
CXX = g++
CFLAGS = -O0 -Wall

.PHONY: all

all: time_calls_cc time_calls_cpp

time_calls_cc: time_calls.c
	$(CC) $(CFLAGS) -o $@ $<
	./$@

time_calls_cpp: time_calls.c
	$(CXX) $(CFLAGS) -o $@ $<
	./$@