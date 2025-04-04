# Simple Makefile for StarPU MPI Master-Slave Hello World
# StarPU version
STARPU_VERSION=1.4

# Compilers - use C++ compilers since the source is C++
CXX = g++
MPICXX = mpicxx

# Program to build
PROG = hello_starpu_mpi

# StarPU flags and libraries
CXXFLAGS += $(shell pkg-config --cflags starpu-$(STARPU_VERSION))
LDLIBS += $(shell pkg-config --libs starpu-$(STARPU_VERSION)) -lmpi

# Add -rdynamic to make functions visible (needed for Master-Slave)
LDFLAGS += -rdynamic

# Main build target
all: $(PROG)

# Compile rule - use MPI C++ compiler wrapper
main.o: main.cpp
	$(MPICXX) $(CXXFLAGS) -c $< -o $@

# Link rule - use MPI C++ compiler wrapper with -rdynamic
$(PROG): main.o
	$(MPICXX) $(LDFLAGS) -o $@ $< $(LDLIBS)

# Clean target
clean:
	rm -f $(PROG) *.o

.PHONY: all clean
