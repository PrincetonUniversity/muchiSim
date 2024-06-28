#include "common/macros.h"
#if USE_OMP
  #include <omp.h>
#else
  #include <pthread.h>
#endif
// System Configuration Parameters
#include "configs/config_system.h"
#include "configs/config_queue.h"
// Global structures and defined parameters
#include "common/global.h"
#include "mem/memory_util.h"
#if APP<ALTERNATIVE
  #include "dataset_loaders/graph_loader.h"
#else
  #include "dataset_loaders/no_loader.h"
#endif
graph_loader * graph = NULL;
#include "network/router.h"
#include "common/calc_area.h"
#include "common/calc_cost.h"
#include "common/util_stats.h"
#include "common/calc_stats.h"
#include "mem/memory_system.h"
#include "apps/frontier_common.h"
#include "configs/config_app.h"
#include "network/network.h"


void *thread_function(void *arg) {
    long tid = (long)arg;
    // cout << tid << flush;
    if (tid < COLUMNS) {
        router_thread(tid);
    } else {
        tsu_core_thread(tid - COLUMNS);
    }
    return NULL;
}

int main(int argc, char** argv) {
  //print_processor_info();
  calculate_derived_param();
  // ==== Check Configurations Allowed ====
  assert(pow(2,log2(GRID_X))==GRID_X );
  assert(GRID_X>=BOARD_W);
  assert(BOARD_W>=DIE_W);
  assert(MUX_BUS<=BOARD_W);
  #if BOARD_W < GRID_X
  assert(COLUMNS_PER_TH>=MUX_BUS);
  #endif

  // ==== ALLOCATE Memory for the DATASET =====
  dataset_filename = "datasets/Kron16/";
  binary_filename = "0";
  bool dry_run = false;
  if (argc >= 2) dataset_filename = argv[1];
  if (argc >= 3) binary_filename = argv[2];
  if (argc >= 4) dry_run = (bool) atoi(argv[3]);
  cout << "Dataset: " << dataset_filename << endl;
  cout << "Dry run: " << dry_run << endl;
  // ==== CONFIGURATIONS ====
  config_dataset(dataset_filename);
  config_app();
  config_queue();

  // ==== CALCULATE STORAGE, AREA and COST ====
  // NOTE: Don't change the order of these functions as there are values that are used in the next ones
  calculate_storage_per_tile();
  print_configuration(cout);
  area_calculation();
  cost_calculation();
  if (dry_run) return 0;

  init_perf_counters(); cout << "Perf counters initialized\n"<<flush;
  connect_mesh(); cout << "Mesh connected\n"<<flush;

  // ==== ALLOCATE Sync, Cache and Dataset Structures =====
  intialize_sync_structures();
  initialize_cache_structures(); cout << "Cache structures initialized\n"<<flush;
  initialize_dataset_structures(); cout << "Dataset structures initialized\n"<<flush;

  // ==== START THE SIMULATION ====
  cout << std::setprecision(2) << std::fixed << "\n\nStarting Simulation\n" << std::flush;
  auto start = chrono::system_clock::now();
  #if USE_OMP
    // int max_threads = omp_get_max_threads(); cout << "Max Available threads: " << max_threads << "\n"; assert(MAX_THREADS <= max_threads);
    omp_set_num_threads(MAX_THREADS);
    #pragma omp parallel default(shared)
    {
      long tid = omp_get_thread_num();
      thread_function((void*)tid);
    }
  #else
    pthread_t threads[MAX_THREADS]; int rc; long t;
    for(t = 0; t < MAX_THREADS; t++) {
        rc = pthread_create(&threads[t], NULL, thread_function, (void *)t);
        if (rc) {std::cout << "Error: unable to create thread," << rc << "\n"; exit(-1);}
    }
    for(t = 0; t < MAX_THREADS; t++) pthread_join(threads[t], NULL);
  #endif

  auto end = std::chrono::system_clock::now();
  chrono::duration<double> elapsed_seconds = end-start;
  double sim_time = elapsed_seconds.count();

  // ==== PRINT THE PERFORMANCE COUNTERS and RESULTS ====
  print_stats_acum(true, sim_time);
  bool result_correct = (argc >= 5) ? result_correct = compare_out(argv[4]) : true;
  cout << "\nVersion 32\n\n";

  // ==== FREE MEMORY ALLOCATED ====
  destroy_cache_structures();
  destroy_dataset_structures();
  destroy_sync_structures();

  return result_correct ? 0 : 1;
}