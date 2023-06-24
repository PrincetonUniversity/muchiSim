

void check_freq(u_int16_t * freq, u_int64_t * tags, u_int32_t set, u_int64_t elem_tag){
  if (freq[elem_tag] == CACHE_MAX_FREQ){
    //Halve the frequency of every line in the cache set
    u_int64_t set_base = set << CACHE_WAY_BITS;
    for (u_int32_t i=0; i<CACHE_WAYS; i++){
      u_int64_t tag = tags[set_base+i];
      if (tag<UINT64_MAX && freq[tag]>1) freq[tag] >>= 1;
    }
  }
}

#include "proxy_cache.h"
#include "data_cache.h"

void intialize_sync_structures(){
  pthread_mutex_init(&printf_mutex, NULL);
  core_timer = (u_int64_t *) calloc(GRID_SIZE*smt_per_tile, sizeof(u_int64_t));
  prev_timer = (u_int64_t *) calloc(GRID_SIZE, sizeof(u_int64_t));
  span_timer = (u_int64_t *) calloc(GRID_SIZE, sizeof(u_int64_t));
  router_timer = (u_int64_t *) calloc(COLUMNS, sizeof(u_int64_t));
  numFrontierNodesPushed  = (u_int64_t *) calloc(COLUMNS, sizeof(u_int64_t));
  numEdgesProcessed  = (u_int64_t *) calloc(COLUMNS, sizeof(u_int64_t));

  epoch_has_work = (bool *) calloc(COLUMNS, sizeof(bool));
  isActive = (bool *) calloc(GRID_SIZE, sizeof(bool));
  for (u_int32_t i=0; i<GRID_SIZE; i++){
    isActive[i] = true;
  }
  for (u_int32_t i=0; i<COLUMNS; i++){
    column_active[i] = true;
    epoch_has_work[i] = true;
  }
  waiting_threads = 0, waiting_threads2 = 0; waiting_routers = 0;
  global_router_active = 1;
}

void destroy_sync_structures(){
  free(core_timer); free(prev_timer); free(span_timer); free(numEdgesProcessed); free(numFrontierNodesPushed);
  free (isActive); free(router_timer);
  free(epoch_has_work);
}

void initialize_cache_structures(){
  for (int i=0;i<GRID_SIZE;i++) pcache_occupancy[i] = 0;
  
  #if PCACHE
    // REVISIT: change size if the proxy array contains other array sizes than #nodes
    for (int i=0;i<PROXYS;i++){pcache_freq[i] = (u_int16_t *)calloc(graph->nodes, sizeof(u_int16_t));}

    for (int i=0;i<GRID_SIZE;i++){
      u_int64_t * array_uint = (u_int64_t *)calloc(pcache_size, sizeof(u_int64_t));
      for (int j=0;j<pcache_size;j++){array_uint[j] = UINT64_MAX;}
      pcache_tags[i] = array_uint;
      pcache_last_evicted[i] = 0;
    }
  #endif

  #if DCACHE
    assert(data_footprint_in_words>0);
    u_int64_t total_lines = data_footprint_in_words >> dcache_words_in_line_log2;
    dcache_freq = (u_int16_t *)calloc(total_lines, sizeof(u_int16_t));

    u_int64_t lines_per_tile = dcache_size >> dcache_words_in_line_log2;
    for (int i=0;i<GRID_SIZE;i++){
      u_int64_t * array_uint = (u_int64_t *)calloc(lines_per_tile, sizeof(u_int64_t));
      for (int j=0;j<lines_per_tile;j++){array_uint[j] = UINT64_MAX;}
      
      dcache_tags[i] = array_uint;
      dcache_occupancy[i] = 0;
      dcache_last_evicted[i] = 0;
    }
  #endif
}

void destroy_cache_structures(){
  #if DCACHE
  free(dcache_freq);
  #endif

  #if PCACHE
  for (int i=0;i<PROXYS;i++){free(pcache_freq[i]);}
  
    uint32_t lines_left = 0;
    for (int i=0;i<GRID_SIZE;i++){
      free(pcache_tags[i]);
      lines_left += pcache_occupancy[i];
    }
    #if DRAIN_PROXY && (WRITE_THROUGH==0)
    assert(lines_left==0);
    #endif
  #endif
}