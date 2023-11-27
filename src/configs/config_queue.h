
// Minimum number entries a software queue might have before we consider it's getting empty,
// and the task that feeds that queue should get priority.
// Similarly this is the number of entries left that a queue might have before we consider a task low priority
// In this way, the task priority is obtained dinamically based on the queue occupancy. This could be runtime programmable (config reg)
const u_int32_t unused_buffer = 6;
const u_int32_t router_buffer = 32;

// Size of the hardware queues that connect routers (for each direction and channel)
u_int32_t rq_sizes[NUM_QUEUES] = {unused_buffer, router_buffer, router_buffer, router_buffer};

#if APP==SPMV || APP==HISTO || APP==SPMV_FLEX
  const u_int32_t default_io_ratio = smt_per_tile > 1 ? (32 / smt_per_tile) : 16;
#else
  const u_int32_t default_io_ratio = 8;
#endif

#define Q_SIZE_REF (GRID_X > 64 ? 64 : GRID_X)
#if TEST_QUEUES>0
  const u_int32_t io_factor_t3b = default_io_ratio; //1 << (TEST_QUEUES-1);
  const u_int32_t io_factor_t2  = default_io_ratio; //1 << (TEST_QUEUES-1);
#else
  const u_int32_t io_factor_t3b = default_io_ratio;  //PROXY_W
  const u_int32_t io_factor_t2  = default_io_ratio;  //Q_SIZE_REF/4;
#endif
const u_int32_t io_factor_t3  = PROXY_ROUTING==1 ? 1 : io_factor_t3b;

int nodePerTile;
int edgePerTile;
u_int32_t num_task_params[NUM_QUEUES] = {1,3,2,2};
u_int32_t task_chunk_size[NUM_QUEUES] = {UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};

#if PROXY_FACTOR==1
    int dest_qid = 2;
#else
    int dest_qid = 3;
#endif

// DEFAULT CONFIGURATIONS THAT CAN BE OVERWRITTEN PER APP
// These MUST NOT BE USED WITHIN THIS FILE, but after config() has completed
u_int32_t t2_piped_tasks = smt_per_tile * 12; //16;
#if TEST_QUEUES>0
u_int32_t t2_t3_ratio = smt_per_tile * (1 << (TEST_QUEUES-1));
#else
u_int32_t t2_t3_ratio = smt_per_tile * 16; //20;
#endif


u_int32_t oq_sizes[NUM_QUEUES] = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
u_int32_t iq_sizes[NUM_QUEUES] = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};

u_int32_t task1_dest = 1;
u_int32_t max_task_chunk = UINT32_MAX;
u_int32_t proxy_shrink = 2;
u_int32_t proxy_default = INT32_MAX;


// Config queue executes after config_app() within each app
void config_queue(){
  set_conf(task_chunk_size[0], nodePerTile);
  set_conf(task_chunk_size[1], edgePerTile);
  set_conf(task_chunk_size[2], nodePerTile);
  set_conf(task_chunk_size[3], nodePerTile);

  u_int32_t t3b_piped_tasks = 1;
  u_int32_t t3_piped_tasks  = t2_piped_tasks * t2_t3_ratio;
  #if PROXY_FACTOR>1 //Proxys
      t3b_piped_tasks = t3_piped_tasks;
      t3_piped_tasks  =  (PROXY_ROUTING==1) ? unused_buffer : t3_piped_tasks / proxy_shrink;
  #endif

  // Check configurations that are allowed
  for (int qid=1; qid<NUM_QUEUES; qid++) if (wide_noc(qid)) assert(num_task_params[qid] >= 2);

  #if PROXY_FACTOR > 1
    assert(wide_noc(2)); // Two flits are routed the same cycle
    assert(num_task_params[2] == 2); // Must be 2 flits if we use proxy
    assert(num_task_params[3] == 2);
  #endif
  // ==== Check Configurations Allowed ====
  assert(oq_sizes[dest_qid] >= 16);
  assert(io_factor_t2);
  assert(io_factor_t3);
  assert(io_factor_t3b);

  // Declare number of parameters to be read from each queue
  // OQ feeds into IQ on the same NOC
  set_conf(oq_sizes[0], unused_buffer);
  set_conf(oq_sizes[1], t2_piped_tasks  * num_task_params[1]);
  set_conf(oq_sizes[2], t3_piped_tasks  * num_task_params[2]);
  set_conf(oq_sizes[3], t3b_piped_tasks * num_task_params[3]);
  
  set_conf(iq_sizes[0], 64); // overwritten at memory_until.h if BARRIER=1
  set_conf(iq_sizes[1], oq_sizes[1] * io_factor_t2);
  set_conf(iq_sizes[2], oq_sizes[2] * io_factor_t3);
  set_conf(iq_sizes[3], oq_sizes[3] * ((PROXY_FACTOR==1) ? 1 : io_factor_t3b));

  // Only set if it hasn't been written already
  set_conf(max_task_chunk, min_macro( (oq_sizes[dest_qid]/2)/num_task_params[2], LOOP_CHUNK));
}






