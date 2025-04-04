/*
 * Simple StarPU MPI Master-Slave Hello World Example
 * For running on 2 nodes with 4 cores each
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <starpu.h>
#include <mpi.h>

/* Simple structure to pass to tasks */
typedef struct {
    int task_id;
    char message[128];
} hello_data_t;

/* CPU task implementation - must be a globally visible function (not static) */
void hello_world_cpu(void *buffers[], void *cl_arg)
{
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
    printf("Task %d executing on %s (MPI rank %d, worker %d): %s\n",
           task_id, hostname, rank, worker_id, message);
    
    /* Simulate some work */
    usleep(100000 * (task_id % 10 + 1));
}

/* Define StarPU codelet with function name - critical for Master-Slave */
static struct starpu_codelet hello_cl = {
    .cpu_funcs = {hello_world_cpu},
    .cpu_funcs_name = {"hello_world_cpu"}, /* Name required for Master-Slave */
    .nbuffers = 0, /* No data buffers used, just arguments */
    .name = "hello_world"
};

int main(int argc, char **argv)
{
    int ret, i;
    int rank, world_size;
    
    /* Initialize MPI with thread support */
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    
    /* Initialize StarPU */
    struct starpu_conf conf;
    starpu_conf_init(&conf);
    
    ret = starpu_init(&conf);
    if (ret != 0) {
        fprintf(stderr, "Error initializing StarPU: %s\n", strerror(-ret));
        MPI_Finalize();
        return 1;
    }
    
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    
    /* In the Master-Slave model, only the master node (rank 0) submits tasks */
    if (rank == 0) {
        /* MASTER NODE CODE */
        printf("Master node starting on %s with %d workers\n", 
               hostname, starpu_worker_get_count());
        printf("Detected %d nodes in the MPI world\n", world_size);
        
        /* Number of tasks to submit */
        int num_tasks = 10;
        printf("\nSubmitting %d hello world tasks...\n\n", num_tasks);
        
        /* Submit tasks to be distributed across nodes */
        for (i = 0; i < num_tasks; i++) {
            /* Prepare message for this task */
            char message[128];
            sprintf(message, "Hello World from task %d!", i);
            
            /* Choose which node should run this task (round-robin) */
            int target_node = i % world_size;
            
            /* Submit the task */
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
        printf("\nAll tasks completed!\n");
    }
    else {
        /* SLAVE NODE CODE - just report that we're ready */
        printf("Slave node %s (Rank %d) ready with %d StarPU workers\n", 
               hostname, rank, starpu_worker_get_count());
        
        /* The slave nodes automatically process tasks sent by the master */
    }
    
    /* Shutdown */
    starpu_shutdown();
    MPI_Finalize();
    
    return 0;
}
