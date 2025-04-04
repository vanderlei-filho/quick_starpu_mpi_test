# Combined Makefile for StarPU MPI Example
# Contains both the general StarPU build configuration and project-specific rules

# StarPU version
STARPU_VERSION=1.4

# Compilers
CC = gcc
NVCC = nvcc

# Programs to build
PROGS=starpu_mpi_test

# StarPU flags and libraries
CPPFLAGS += $(shell pkg-config --cflags starpu-$(STARPU_VERSION))
LDLIBS += $(shell pkg-config --libs starpu-$(STARPU_VERSION))

# Add MPI support
LDLIBS += -lstarpu-mpi

# Compiler flags
CFLAGS += -O3 -Wall -Wextra -lm -fopenmp
NVCCFLAGS = $(shell pkg-config --cflags starpu-$(STARPU_VERSION)) -std=c++11

# To avoid having to use LD_LIBRARY_PATH
LDLIBS += -fopenmp -lm -Wl,-rpath -Wl,$(shell pkg-config --variable=libdir starpu-$(STARPU_VERSION))

# Automatically enable CUDA / OpenCL
STARPU_CONFIG=$(shell pkg-config --variable=includedir starpu-$(STARPU_VERSION))/starpu/$(STARPU_VERSION)/starpu_config.h
ifneq ($(shell grep "STARPU_USE_CUDA 1" $(STARPU_CONFIG)),)
USE_CUDA=1
endif

# CUDA-specific settings
ifeq ($(USE_CUDA),1)
LDLIBS += -L$(CUDA_PATH)/lib64 -lcudart -lstdc++
endif

# Rule for compiling CUDA files
%.o: %.cu
	$(NVCC) $(NVCCFLAGS) $< -c -o $@

# Main build target
all: $(PROGS)

# Link rule for our program
starpu_mpi_test: main.o
	$(CC) -o $@ $< $(LDLIBS)

# Clean target
clean:
	rm -f $(PROGS) *.o
	rm -f paje.trace dag.dot *.rec trace.html starpu.log
	rm -f *.gp *.eps *.data

.PHONY: all clean
