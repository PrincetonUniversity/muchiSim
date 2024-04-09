// === GLOBAL VARIABLES ===
u_int64_t *** total_counters;
u_int32_t *** frame_counters;
u_int64_t *** print_counters;
u_int64_t * core_timer;
u_int64_t * prev_timer;
u_int64_t * span_timer;
bool * isActive;
u_int64_t * router_timer;
string dataset_filename, binary_filename;

// SYNCHRONIZATION VARIABLES
std::atomic<int> global_router_active;
std::mutex router_mutex, router_mutex2, all_threads_mutex2, all_threads_mutex;
std::condition_variable router_cv, router_cv2, all_threads_cv2, all_threads_cv;
std::atomic<int> waiting_routers, waiting_routers2, waiting_threads2, waiting_threads;
std::atomic<bool> column_active[COLUMNS];

#include "print_lock.h"
print_lock print_lock;
pthread_mutex_t printf_mutex;
#define LK pthread_mutex_lock(&printf_mutex)
#define ULK pthread_mutex_unlock(&printf_mutex)


#if APP==PAGE
  float * proxys[PROXY_FACTOR*PROXY_FACTOR];
  float * ret;
#else
  int * proxys[PROXY_FACTOR*PROXY_FACTOR];
  int * ret;
#endif

int dataset_has_edge_val = 0;
int app_has_frontier = 0;
u_int64_t dataset_words_per_tile;

int epoch_counter = 1;
int num_kernels = 1;
int64_t * task_global_params;

u_int64_t * numFrontierNodesPushed;
u_int64_t * numEdgesProcessed; 
u_int64_t cool_down_time = 0;

// ==== FRONTIER VARIABLES ====
//Frontier is a bitmap, where each 4byte word contains the bit of 32 nodes
u_int32_t bitmap_len;
u_int32_t * bitmap_frontier;
u_int32_t frontier_list_len;
u_int32_t fr_head_offset, fr_tail_offset;
int * frontier_list;
bool * epoch_has_work;
bool * epoch_pcache_busy;

// Dataset storage per core
u_int64_t data_footprint_in_words=0;
u_int64_t dram_active_words=0;

// ==== WORKLOAD RESULTS ====
float * in_r = NULL;

u_int64_t * mc_transactions;
u_int64_t * mc_writebacks;
u_int64_t * mc_latency;

u_int64_t inter_board_traffic[BOARDS];
u_int64_t inter_pack_traffic[PACKAGES];
u_int64_t inter_die_traffic[DIES];
u_int64_t ruche_traffic[DIES];

u_int32_t pcache_size, pcache_sets;
u_int32_t dcache_size, dcache_sets;
u_int32_t dcache_footprint_ratio;
bool proxys_cached=false, dataset_cached=false;

u_int32_t pcache_occupancy[GRID_SIZE];
#if PCACHE
u_int16_t * pcache_freq[PROXY_FACTOR*PROXY_FACTOR];
u_int64_t * pcache_tags[GRID_SIZE];
u_int32_t pcache_last_evicted[GRID_SIZE];
#endif
u_int64_t pcache_misses = 0,pcache_hits = 0,pcache_evictions = 0;

#if DCACHE
u_int16_t * dcache_freq;
u_int64_t * dcache_tags[GRID_SIZE];
u_int32_t dcache_occupancy[GRID_SIZE];
u_int32_t dcache_last_evicted[GRID_SIZE];
#endif
u_int64_t dcache_hits=0, dcache_misses=0, dcache_evictions=0;

u_int32_t board_arbitration_N[BOARDS*BOARD_BUSES];
u_int32_t board_arbitration_S[BOARDS*BOARD_BUSES];
u_int32_t board_arbitration_E[BOARDS*BOARD_BUSES];
u_int32_t board_arbitration_W[BOARDS*BOARD_BUSES];

#include <map>
std::map<std::string, double> vars;