/* StarPU --- Runtime system for heterogeneous multicore architectures.
 *
 * Copyright (C) 2009-2025  University of Bordeaux, CNRS (LaBRI UMR 5800), Inria
 *
 * StarPU is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * StarPU is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU Lesser General Public License in COPYING.LGPL for more details.
 */

/*
 * This examples demonstrates how to use StarPU with MPI to distribute tasks
 * across multiple processes.
 */

#include <mpi.h>
#include <starpu.h>
#include <starpu_mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define FPRINTF(ofile, fmt, ...)                                               \
  do {                                                                         \
    if (!getenv("STARPU_SSILENT")) {                                           \
      fprintf(ofile, fmt, ##__VA_ARGS__);                                      \
    }                                                                          \
  } while (0)
#define NTASKS                                                                 \
  100 /* Total number of tasks to distribute across MPI processes */

/* When the task is done, task->callback_func(task->callback_arg) is called. Any
 * callback function must have the prototype void (*)(void *).
 * NB: Callback are NOT allowed to perform potentially blocking operations */
void callback_func(void *callback_arg) {
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  FPRINTF(stdout, "[Process %d] Callback function got argument %p\n", rank,
          callback_arg);
}

/* Every implementation of a codelet must have this prototype */
struct params {
  int i;
  float f;
  int task_id;
  int mpi_rank;
};

void cpu_func(void *buffers[], void *cl_arg) {
  (void)buffers;
  struct params *params = (struct params *)cl_arg;

  FPRINTF(stdout, "[Process %d] Processing task %d (params = {%i, %f})\n",
          params->mpi_rank, params->task_id, params->i, params->f);

  /* Simulate some work */
  usleep(100000); /* 100ms of work per task */
}

int main(int argc, char **argv) {
  int ret, i;
  int rank, size;
  int ntasks_per_process;
  int first_task_id;

  /* Initialize MPI */
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  /* Calculate task distribution */
  ntasks_per_process = NTASKS / size;
  first_task_id = rank * ntasks_per_process;

  /* Handle remainder tasks */
  int remainder = NTASKS % size;
  if (rank < remainder) {
    ntasks_per_process++;
    first_task_id += rank;
  } else {
    first_task_id += remainder;
  }

  /* Print distribution info */
  FPRINTF(stdout, "[Process %d] Will process %d tasks starting from task %d\n",
          rank, ntasks_per_process, first_task_id);

  /* Initialize StarPU with default configuration */
  ret = starpu_init(NULL);
  if (ret == -ENODEV) {
    MPI_Finalize();
    return 77;
  }
  STARPU_CHECK_RETURN_VALUE(ret, "starpu_init");

  /* Initialize StarPU-MPI */
  ret = starpu_mpi_init(NULL, NULL, 0);
  if (ret == -ENODEV) {
    starpu_shutdown();
    MPI_Finalize();
    return 77;
  }

  /* Create and submit tasks for this process */
  for (i = 0; i < ntasks_per_process; i++) {
    struct starpu_codelet cl;
    struct starpu_task *task;
    struct params *params = new struct params;

    params->i = i;
    params->f = rank + i * 0.1f;
    params->task_id = first_task_id + i;
    params->mpi_rank = rank;

    /* Create a new task */
    task = starpu_task_create();

    starpu_codelet_init(&cl);
    /* This codelet may only be executed on a CPU */
    cl.cpu_funcs[0] = cpu_func;
    cl.cpu_funcs_name[0] = "cpu_func";
    cl.nbuffers = 0;
    cl.name = "distributed_task";

    /* Associate codelet with task */
    task->cl = &cl;

    /* Set task parameters */
    task->cl_arg = params;
    task->cl_arg_size = sizeof(struct params);
    task->cl_arg_free = 1; /* StarPU will free the params after execution */

    /* Set callback */
    task->callback_func = callback_func;
    task->callback_arg = (void *)(uintptr_t)(first_task_id + i);

    /* Non-blocking submission */
    task->synchronous = 0;

    /* Submit the task to StarPU */
    ret = starpu_task_submit(task);
    if (ret == -ENODEV)
      goto enodev;
    STARPU_CHECK_RETURN_VALUE(ret, "starpu_task_submit");
  }

  /* Wait for all local tasks to complete */
  starpu_task_wait_for_all();

  /* Ensure all processes have completed their tasks before proceeding */
  MPI_Barrier(MPI_COMM_WORLD);

  if (rank == 0) {
    FPRINTF(stdout, "All %d tasks have been processed by %d MPI processes\n",
            NTASKS, size);
  }

  /* Cleanup StarPU-MPI */
  starpu_mpi_shutdown();

  /* Cleanup StarPU */
  starpu_shutdown();

  /* Cleanup MPI */
  MPI_Finalize();

  return 0;

enodev:
  starpu_mpi_shutdown();
  starpu_shutdown();
  MPI_Finalize();
  return 77;
}
