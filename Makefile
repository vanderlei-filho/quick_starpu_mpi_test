# StarPU version
STARPU_VERSION=1.4

# Compilers
CXX = g++
MPICXX = mpicxx

# Programs to build
PROG1 = hello_starpu
PROG2 = hello_starpu_mpi

# StarPU flags and libraries
CXXFLAGS += $(shell pkg-config --cflags starpu-$(STARPU_VERSION)) -fPIC -rdynamic
LDLIBS += $(shell pkg-config --libs starpu-$(STARPU_VERSION)) -ldl
MPI_LDLIBS = $(LDLIBS) -lmpi

# Flags to make functions visible (needed for MPI Master-Slave)
LDFLAGS += -rdynamic -Wl,--export-dynamic -Wl,-E

# Main build target - build both programs
all: $(PROG1) $(PROG2)

# Compile rules
hello_starpu.o: hello_starpu.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

mpi_hello_starpu.o: mpi_hello_starpu.cpp
	$(MPICXX) $(CXXFLAGS) -c $< -o $@

# Link rules
$(PROG1): hello_starpu.o
	$(CXX) $(LDFLAGS) -o $@ $< $(LDLIBS)

$(PROG2): mpi_hello_starpu.o
	$(MPICXX) $(LDFLAGS) -o $@ $< $(MPI_LDLIBS)

# Clean target
clean:
	rm -f $(PROG1) $(PROG2) *.o

.PHONY: all clean
