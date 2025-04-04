#include <iostream>
#include <starpu.h>
#include <starpu_mpi.h>
#include <starpu_scheduler.h>
#include <string>
#include <vector>

// Define a simple task struct to hold data
struct hello_task_data {
  int node_id;
  int task_id;
  int result;
};

// CPU implementation of our hello world task
void hello_world_cpu_func(void *buffers[], void *cl_arg) {
  // Get the task data from the codelet argument
  hello_task_data *data = (hello_task_data *)cl_arg;

  // Simulating some computation by incrementing the task ID
  data->result = data->task_id * 10 + data->node_id;

  // Report task execution
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  std::cout << "Hello from Node " << rank << "! Executing task "
            << data->task_id << ", computed result: " << data->result
            << std::endl;

  // Simulate some work
  starpu_usleep(100000); // Sleep for 100ms
}

// Define our StarPU codelet
static struct starpu_codelet hello_world_codelet = {
    .cpu_funcs = {hello_world_cpu_func},
    .cpu_funcs_name = {"hello_world_cpu_func"},
    .nbuffers = 0, // No data buffers, using cl_arg
    .name = "hello_world_codelet"};

int main(int argc, char **argv) {
  // Initialize MPI
  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);

  int rank, world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  // Initialize StarPU with default configuration
  struct starpu_conf conf;
  starpu_conf_init(&conf);

  // Uncomment to set specific number of CPU cores per node (default uses all
  // available) conf.ncpus = 4;  // Set to use 4 cores per node

  int ret = starpu_init(&conf);
  if (ret != 0) {
    std::cerr << "Error initializing StarPU: " << strerror(-ret) << std::endl;
    MPI_Finalize();
    return 1;
  }

  // Initialize StarPU-MPI
  ret = starpu_mpi_init(NULL, NULL, 0);
  if (ret != 0) {
    std::cerr << "Error initializing StarPU-MPI: " << strerror(-ret)
              << std::endl;
    starpu_shutdown();
    MPI_Finalize();
    return 1;
  }

  std::cout << "Node " << rank << "/" << world_size << " initialized with "
            << starpu_worker_get_count() << " StarPU workers" << std::endl;

  // Total number of tasks to execute
  const int TASKS_PER_NODE = 8;
  const int TOTAL_TASKS = TASKS_PER_NODE * world_size;

  // Simple round-robin task distribution
  if (rank == 0) {
    // MASTER NODE: Create and submit all tasks
    std::cout << "Master node distributing " << TOTAL_TASKS << " tasks..."
              << std::endl;

    std::vector<hello_task_data> task_data(TOTAL_TASKS);
    std::vector<starpu_task *> tasks(TOTAL_TASKS);

    for (int i = 0; i < TOTAL_TASKS; i++) {
      // Determine which node should execute this task
      int target_node = i % world_size;

      // Setup task data
      task_data[i].node_id = target_node;
      task_data[i].task_id = i;
      task_data[i].result = 0;

      // Create the task
      tasks[i] = starpu_task_create();
      tasks[i]->cl = &hello_world_codelet;
      tasks[i]->cl_arg = &task_data[i];
      tasks[i]->cl_arg_size = sizeof(hello_task_data);

      // Set the task to execute on a specific MPI node
      tasks[i]->execute_on_a_specific_worker = 0;

      // Submit the task
      if (target_node == 0) {
        // Execute locally
        ret = starpu_task_submit(tasks[i]);
      } else {
        // Send to another node
        ret = starpu_mpi_task_insert(MPI_COMM_WORLD, &hello_world_codelet,
                                     STARPU_EXECUTE_ON_NODE, target_node,
                                     STARPU_VALUE, &task_data[i],
                                     sizeof(hello_task_data), 0);
      }

      if (ret != 0) {
        std::cerr << "Error submitting task " << i << ": " << strerror(-ret)
                  << std::endl;
      }
    }

    // Wait for all tasks to complete
    starpu_task_wait_for_all();

    // Collect and display results
    std::cout << "\nAll tasks completed! Results summary:" << std::endl;
    for (int i = 0; i < TOTAL_TASKS; i++) {
      std::cout << "Task " << i << " on Node " << task_data[i].node_id
                << " computed: " << task_data[i].result << std::endl;
    }
  } else {
    // WORKER NODES: Just execute tasks sent by the master
    // The StarPU-MPI layer handles receiving and executing the tasks
    starpu_mpi_wait_for_all(MPI_COMM_WORLD);
  }

  // Cleanup and shutdown
  starpu_mpi_shutdown();
  starpu_shutdown();
  MPI_Finalize();

  return 0;
}
