#CIS3090 Assignment 2: Conway's Game of Life MPI
#Author: Kyle Lukaszek
#ID: 1113798
#Due: October 20th, 2023

CC=mpicc
CFLAGS=-g -Wall

# If "make DEBUG=1", then compile with debug flags
ifndef DEBUG
else
CFLAGS += -DDEBUG
endif

all: a2

a2: a2.c
	$(CC) $(CFLAGS) -o a2 a2.c

clean:
	rm -f a2