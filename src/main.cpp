using namespace std;
#include "common/macros.h"
// System Configuration Parameters
#include "configs/config_system.h"
#include "configs/config_queue.h"
// Global structures and defined parameters
#include "common/global.h"
#include "mem/memory_util.h"
#if APP<6
  #include "dataset_loaders/graph_loader.h"
#else
  #include "dataset_loaders/no_loader.h"
#endif
graph_loader * graph;
#include "network/router.h"
#include "common/util_stats.h"
#include "mem/memory_system.h"
#include "apps/frontier_common.h"
#include "configs/config_app.h"
#include "network/network.h"

int main(int argc, char** argv) {
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
  string dataset_filename;
  if (argc >= 2) dataset_filename = argv[1];
  else dataset_filename = "datasets/Kron16/";
  // ==== CONFIGURATIONS ====
  config_dataset(dataset_filename);
  config_app();
  config_queue();

  // ==== CALCULATE STORAGE AND AREA ====
  calculate_storage_per_tile();
  area_calculation();
  
  print_configuration();
  init_perf_counters(); cout << "Perf counters initialized\n"<<flush;
  connect_mesh(); cout << "Mesh connected\n"<<flush;

  // ==== ALLOCATE Sync, Cache and Dataset Structures =====
  intialize_sync_structures();
  initialize_cache_structures(); cout << "Cache structures initialized\n"<<flush;
  initialize_dataset_structures(); cout << "Dataset structures initialized\n"<<flush;

  // ==== START THE SIMULATION ====
  cout << std::setprecision(2) << std::fixed << "\n\nStarting Simulation\n" << std::flush;
  auto start = chrono::system_clock::now();
  omp_set_num_threads(MAX_THREADS);
  #pragma omp parallel default(shared)
  {
    int tid = omp_get_thread_num();
    if (tid<COLUMNS) router_thread(tid); else tsu_core_thread(tid-COLUMNS);
  }
  auto end = std::chrono::system_clock::now();
  chrono::duration<double> elapsed_seconds = end-start;
  cout << "\nSimulation time: " << elapsed_seconds.count() << "s\n";

  // ==== PRINT THE PERFORMANCE COUNTERS and RESULTS ====
  print_stats_acum(true);
  bool result_correct = (argc >= 3) ? result_correct = compare_out(argv[2]) : true;
  
  // ==== FREE MEMORY ALLOCATED ====
  destroy_cache_structures();
  destroy_dataset_structures();
  destroy_sync_structures();

  return result_correct ? 0 : 1;
}