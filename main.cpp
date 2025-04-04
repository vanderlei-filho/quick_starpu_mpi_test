/*
 * Simple StarPU MPI Master-Slave Hello World Example
 */
#include <mpi.h>
#include <starpu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* CPU task implementation - using extern "C" to prevent name mangling */
extern "C" __attribute__((visibility("default"))) 
void hello_world_cpu(void *buffers[], void *cl_arg) {
  /* Unpack the task arguments */
  int task_id;
  char message[128];
  starpu_codelet_unpack_args(cl_arg, &task_id, message);
  
  /* Get information about where we're running */
  char hostname[256];
  gethostname(hostname, sizeof(hostname));
  int worker_id = starpu_worker_get_id();
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  
  /* Print hello message */
  printf("Task %d executing on %s (MPI rank %d, worker %d): %s\n", task_id,
         hostname, rank, worker_id, message);
         
  /* Simulate some work */
  usleep(50000);
}

/* Define StarPU codelet with function name - critical for Master-Slave */
static struct starpu_codelet hello_cl = {
    .cpu_funcs = {hello_world_cpu},
    .cpu_funcs_name = {"hello_world_cpu"}, /* Name required for Master-Slave */
    .nbuffers = 0,
    .name = "hello_world"
};

int main(int argc, char **argv) {
  int ret, i;
  int rank, world_size;
  
  /* Initialize MPI with thread support */
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  
  /* Initialize StarPU with minimal config */
  struct starpu_conf conf;
  starpu_conf_init(&conf);
  conf.nmpi_ms = 1; /* Use only 1 thread per slave node */
  
  ret = starpu_init(&conf);
  if (ret != 0) {
    fprintf(stderr, "Error initializing StarPU: %s\n", strerror(-ret));
    MPI_Finalize();
    return 1;
  }
  
  char hostname[256];
  gethostname(hostname, sizeof(hostname));
  
  /* For master/slave coordination */
  int signal_exit = 0;
  
  /* In the Master-Slave model, only the master node (rank 0) submits tasks */
  if (rank == 0) {
    printf("Master node %s with %d workers\n", hostname, starpu_worker_get_count());
    printf("Using %d MPI nodes\n", world_size);
    
    /* Submit only a few tasks */
    int num_tasks = world_size * 2; /* 2 tasks per node */
    
    /* Submit tasks to be distributed across nodes */
    for (i = 0; i < num_tasks; i++) {
      char message[128];
      sprintf(message, "Hello from task %d", i);
      
      /* Round-robin distribution */
      int target_node = i % world_size;
      
      ret = starpu_task_insert(&hello_cl, 
                               STARPU_VALUE, &i, sizeof(int),
                               STARPU_VALUE, message, strlen(message) + 1,
                               STARPU_EXECUTE_ON_NODE, target_node, 
                               0);
      
      if (ret != 0) {
        fprintf(stderr, "Error submitting task %d: %s\n", i, strerror(-ret));
      }
    }
    
    /* Wait for all tasks to complete */
    starpu_task_wait_for_all();
    printf("All tasks completed!\n");
    
    /* Signal slave nodes to exit */
    signal_exit = 1;
    for (i = 1; i < world_size; i++) {
      MPI_Send(&signal_exit, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    }
  } else {
    printf("Slave node %s (Rank %d) ready with %d workers\n", 
           hostname, rank, starpu_worker_get_count());
    
    /* Wait for exit signal from master */
    MPI_Recv(&signal_exit, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    printf("Slave node %d shutting down\n", rank);
  }
  
  /* Clean shutdown without barriers */
  starpu_shutdown();
  MPI_Finalize();
  
  return 0;
}
