# Simple Makefile for StarPU MPI Master-Slave Hello World

# StarPU version
STARPU_VERSION=1.4

# Compilers
CC = gcc
MPICC = mpicc

# Program to build
PROG = hello_starpu_mpi

# StarPU flags and libraries
CFLAGS += $(shell pkg-config --cflags starpu-$(STARPU_VERSION))
LDLIBS += $(shell pkg-config --libs starpu-$(STARPU_VERSION))

# Add -rdynamic to make functions visible (needed for Master-Slave)
LDFLAGS += -rdynamic

# Main build target
all: $(PROG)

# Compile rule - use MPI compiler wrapper
main.o: main.c
	$(MPICC) $(CFLAGS) -c $< -o $@

# Link rule - use MPI compiler wrapper with -rdynamic
$(PROG): main.o
	$(MPICC) $(LDFLAGS) -o $@ $< $(LDLIBS)

# Clean target
clean:
	rm -f $(PROG) *.o

.PHONY: all clean
