#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <climits>
#include <math.h>
#include <unistd.h>
#include <omp.h>
#include <iomanip>
#include <set>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>

// define a union structure with an int and a float
union float_int{
  int int_val;
  float float_val;
};

#define min_macro(a,b) (((a)<(b))?(a):(b))
#define max_macro(a,b) (((a)>(b))?(a):(b))
#define set_conf(a,b) (a = (a == UINT32_MAX) ? b : a)

#define IQ(q) (routers[tX][tY]->output_q[C][q])
#define OQ(q) (routers[tX][tY]->input_q[C][q])
#define load(r) (frame_counters[tX][tY][LOAD]+=r)
#define store(r) (frame_counters[tX][tY][STORE]+=r)
#define msg(c,r) (frame_counters[id_X][id_Y][MSG_1+c]+=r)
#define msgcol(c,r) (frame_counters[id_X][id_Y][MSG_IN_COLLISION+c]+=r)

// APP
#define SSSP 0
#define PAGE 1
#define BFS 2
#define WCC 3
#define SPMV 4
#define HISTO 5
#define HISTO_NODE_REDUCER 8


//MESH PARAMETERS
#ifndef GRID_X_LOG2
  #define GRID_X_LOG2 4
#endif
#define GRID_X (1<<GRID_X_LOG2)
#ifndef GRID_Y
  #define GRID_Y (GRID_X)
#endif
#define GRID_SIZE (GRID_X * GRID_Y)
#define GRID_SIZE_LOG2 (GRID_X_LOG2 << 1)
#define GRID_SIZEm1 (GRID_SIZE - 1)
#define GRID_Xm1 (GRID_X - 1)
#define GRID_Ym1 (GRID_Y - 1)
#define GRID_X_HALF (GRID_X >> 1)
#define GRID_Y_HALF (GRID_Y >> 1)

// BOARD PARAMETERS
// BOARD_W <= GRID_X
#ifndef BOARD_W
   #define BOARD_W (GRID_X)
#endif
#define BOARD_H (BOARD_W)
#define BOARD_W_HALF (BOARD_W / 2)
#define BOARD_H_HALF (BOARD_H / 2)
#define BOARD_FACTOR (GRID_X / BOARD_W)
#define BOARDS (BOARD_FACTOR * BOARD_FACTOR)
#if BOARD_W < GRID_X
  #define MUX_BUS 2 // Multiplex output links to reduce the number of buses going out of the board
#else
  #define MUX_BUS 1 //Fixed to 1
#endif
#define BOARD_BUSES (BOARD_W / MUX_BUS)


#define PACK_W (GRID_X > 64 ? 64 : GRID_X)
#define PACK_H (PACK_W)
#define PACK_W_HALF (PACK_W / 2)
#define PACK_H_HALF (PACK_H / 2)
#define PACK_FACTOR (GRID_X / PACK_W)
#define PACKAGES (PACK_FACTOR * PACK_FACTOR)
#define PACKAGES_BOARD_FACTOR (BOARD_W / PACK_W)
#define PACKAGES_PER_BOARD (PACKAGES_BOARD_FACTOR * PACKAGES_BOARD_FACTOR)

// PROXY_W < BOARD_W
// A PROXY MUST BE SMALLER THAN A BOARD
// PROXY PARAMETERS
#ifndef PROXY_W
  #define PROXY_W (BOARD_W > 32 ? 32 : BOARD_W)
#endif
#define PROXY_H (PROXY_W)
#define PROXY_FACTOR (GRID_X / PROXY_W)
#define PROXYS (PROXY_FACTOR * PROXY_FACTOR)

// DIE_W >= 4
// CHIPLET DIE PARAMETERS
#ifndef DIE_W
   #define DIE_W (BOARD_W)
#endif
#ifndef DIE_H
   #define DIE_H (DIE_W)
#endif
#define DIE_Wm1 (DIE_W - 1)
#define DIE_Hm1 (DIE_H - 1)

#define DIE_W_HALF (DIE_W / 2)
#define DIE_W_HALFm1 (DIE_W_HALF - 1)
#define DIE_H_HALF (DIE_H / 2)
#define DIE_H_HALFm1 (DIE_H_HALF - 1)
#define TILES_PER_DIE (DIE_W * DIE_H)
#define BORDER_TILES_PER_DIE (DIE_W*2 + DIE_H*2)
#define INNER_TILES_PER_DIE (TILES_PER_DIE - BORDER_TILES_PER_DIE)

#define DIE_FACTOR (GRID_X / DIE_W)
#define DIES (DIE_FACTOR * DIE_FACTOR)

#define DIES_PACK_FACTOR (PACK_W / DIE_W)
#define DIES_PER_PACK (DIES_PACK_FACTOR * DIES_PACK_FACTOR)

#define DIES_BOARD_FACTOR (BOARD_W / DIE_W)

// STEAL_W <= DIE_W
// Steal region means that a in-transit task message can be stolen by a core that is in the same region
#ifndef STEAL_W
   #define STEAL_W 1
#endif
#ifndef STEAL_H
   #define STEAL_H (STEAL_W)
#endif

// RUCHE PARAMETERS
#ifndef RUCHE_X
   #define RUCHE_X (DIE_W_HALF)
#endif
#define RUCHE_Y (RUCHE_X)

// NETWORK PARAMETERS
#ifndef TORUS
  #define TORUS 1
#endif
#ifndef QUEUE_PRIO
  #define QUEUE_PRIO 0
#endif
#ifndef GLOBAL_BARRIER
  #define GLOBAL_BARRIER 0
#endif
#ifndef SHUFFLE
  #define SHUFFLE 1
#endif

// Values 2 or 3
#define PHY_NOCS 2
#ifndef NOC_CONF
  #define NOC_CONF 2
#endif

// NOC_CONF:0 means 2 NoCS of 32 bits intra die, which get throttled to a shared 32 bits across dies
// NOC_CONF:1 means 2 NoCS of 32 and 64 bit intra die, which get throttled to a shared 32 bits across dies
// NOC_CONF:2 means 2 NoCS of 32 and 64 bit intra die, which get throttled to 2 NoCs of 32 and 32 bits across dies
// NOC_CONF:3 means 2 NoCS of 32 and 64 bit intra and inter-die
#if NOC_CONF>0
    #define WIDE_NOC 1
#else
  // Set this to a Channel that doesn't exist
  #define WIDE_NOC 9
#endif


#ifndef PCACHE
  #define PCACHE 1
#endif
#ifndef DCACHE
  #define DCACHE 1 //1: cache and DRAM in separete die, 2: cache and DRAM in the same HBM die
#endif

#ifndef LOOP_CHUNK
  #define LOOP_CHUNK 64
#endif
#ifndef TEST_QUEUES
  #define TEST_QUEUES 0
#endif


#define DIRECT_MAPPED 1
#if DIRECT_MAPPED
  #define CACHE_WAY_BITS 0
  #define CACHE_FREQ_BITS 0
#else
  #define CACHE_WAY_BITS 3 // 8-way cache
  #define CACHE_FREQ_BITS 3
#endif

#define CACHE_WAYS (1<<CACHE_WAY_BITS)
#define CACHE_MAX_FREQ (1 << CACHE_FREQ_BITS)


#define set_id_pcache(tag)((tag % pcache_sets))
#define DCACHE_LOW_ORDER 1
#if DCACHE_LOW_ORDER
  #define set_id_dcache(tag)((tag % dcache_sets))
#else
  #define set_id_dcache(tag)((tag / GRID_SIZE / dcache_footprint_ratio / CACHE_WAYS))
#endif

//ROUTER BASED
#define NUM_QUEUES 4
#define NUM_TASKS 5

#ifndef SRAM_SIZE
  #define SRAM_SIZE 1024
#endif
#ifndef MAX_THREADS
  #define MAX_THREADS 2
#endif

// Columns of the grid that a router thread is responsible for
#define COLUMNS_PER_TH (GRID_X / (MAX_THREADS / 2) )
#define COLUMNS (GRID_X / COLUMNS_PER_TH)
#define CORES_PER_TH (GRID_Y * COLUMNS_PER_TH)


#define proxy_id(tX,tY) ((tX / PROXY_W)*PROXY_FACTOR+(tY / PROXY_H))
#define die_id(tX,tY) ((tX / DIE_W)*DIE_FACTOR+(tY / DIE_H))
#define board_id(tX,tY) ((tX / BOARD_W)*BOARD_FACTOR+(tY / BOARD_H))

#if PHY_NOCS == 3
  #define phy_noc(d,qid) (d*PHY_NOCS+(qid-1))
  // TODO change if we use 3 PHY NOCs
  #define wide_noc(qid) (qid==2)
#else
  // #if PROXY_FACTOR==1 // No Proxy
    #define phy_noc(d,qid) (d*PHY_NOCS+(qid/2))
    #define wide_noc(qid) ((qid/2)==WIDE_NOC)
  // #else // Make NOC3 wide
  //   #define phy_noc(d,qid) (d*PHY_NOCS+(qid/3))
  //   #define wide_noc(qid) ((qid/3)==WIDE_NOC)
  // #endif
#endif

#if TORUS==0
  #define global(x, y)( ((y%2)==1)? ( (u_int32_t)y << GRID_X_LOG2) + (u_int32_t) GRID_Xm1-x : ( (u_int32_t)y << GRID_X_LOG2) + (u_int32_t)x)
#else
  #define global(x, y)( (( (u_int32_t)y << GRID_X_LOG2) + (u_int32_t) x) )
#endif

//On-the-way&cascading (11)? or within&no-cascading (00)
#ifndef PROXY_ROUTING
  #define PROXY_ROUTING 1 // 0: Within proxy, 1: On-the-way, 2: Nearest
#endif
#ifndef CASCADE_WRITEBACK
  #define CASCADE_WRITEBACK 1
#endif

#define DRAIN_PROXY 1
// Whethet an update to the proxy data would be sent to the true owner directly
#define WRITE_THROUGH 0

#if WRITE_THROUGH==1
  #define need_flush_pcache(global_cid)(false)
#else
  #define need_flush_pcache(global_cid)((pcache_occupancy[global_cid] > 0))
#endif

const u_int32_t uncontested_noc = 3;
const u_int32_t write_queue_max_occupancy = 4;

#define get_qid() (dest_qid)


// PRINT: verbosity level
#ifndef PRINT
  #define PRINT 0
#endif
#ifndef ASSERT_MODE
  #define ASSERT_MODE 1
#endif


#if GRID_X > 64
  #define PRINT_X 64
  #define PRINT_Y 64
#else
  #define PRINT_X (GRID_X)
  #define PRINT_Y (GRID_Y)
#endif
#define PRINT_SIZE (PRINT_X * PRINT_Y)

//PERFORMANCE REPORTING PARAMETERS
#define LOCAL_COUNTERS 11
#define GLOBAL_COUNTERS 22
  typedef enum {  
   TASK1 = 0,
   TASK2 = 1,
   TASK3 = 2,
   TASK4 = 3,
   T1 = 4,
   T2 = 5,
   T3 = 6,
   T3b = 7,
   T4 = 8,
   IDLE = 9,
   STORE = 10,
   LOAD = 11,
   MSG_1 = 12,
   MSG_2 = 13,
   MSG_3 = 14,
   MSG_IN_COLLISION = 15,
   MSG_OUT_COLLISION = 16,
   MSG_END_COLLISION = 17,
   MSG_1_LAT = 18,
   MSG_2_LAT = 19,
   MSG_3_LAT = 20,
   ROUTER_ACTIVE = 21
   } counters_enum;


#define DIRECTIONS 11
#define DIR_TO_PRINT (DIRECTIONS - 4)
typedef enum
{
   N = 0,
   S = 1,
   W = 2,
   E = 3,
   RN = 4,
   RS = 5,
   RW = 6,
   RE = 7,
   PY = 8,
   PX = 9,
   C = 10,
   DIR_NONE = DIRECTIONS
} dir;

u_int16_t flipD(u_int16_t d){
  if (d==N) return S;
  if (d==S) return N;
  if (d==W) return E;
  if (d==E) return W;
  assert(0);
}

char getD(int d){
   if (d==N) return 'N';
   if (d==S) return 'S';
   if (d==W) return 'W';
   if (d==E) return 'E';
   if (d==RN) return 'N';
   if (d==RS) return 'S';
   if (d==RW) return 'W';
   if (d==RE) return 'E';
   if (d==C) return 'C';
   if (d==PX) return 'X';
   if (d==PY) return 'Y';
   return '*';
}

typedef enum
{
   MONO = 0,
   HEAD = 1,
   MID = 2,
   TAIL = 3,
   
} head;

struct Msg {
  int data;
  head type; //have head two MSBs of X & Y in HW
  u_int64_t time;

  Msg(int data, head type, u_int64_t time) : 
  data(data), type(type),time(time){}

};