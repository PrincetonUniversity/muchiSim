
#if DCACHE
  int dcache_replacement_policy(u_int32_t tileid, u_int64_t * tags, u_int64_t new_tag, u_int16_t & set_empty_lines){
    // Search the element of tags whose index into freq has the lowest value
    int evict_dcache_idx,dcache_idx;
    u_int16_t line_freq,min_freq = UINT16_MAX;

    // Set base is the index of the first cache line in a set
    // Set_id hashes the cache line tag to a set
    u_int32_t set = set_id_dcache(new_tag);
    u_int64_t set_base = set << CACHE_WAY_BITS;

    // REPLACEMENT POLICY IN HW
    for (u_int32_t i=0; i<CACHE_WAYS; i++){
      dcache_idx = i+set_base;
      u_int64_t tag = tags[dcache_idx];
      if (tag<UINT64_MAX) line_freq = dcache_freq[tag];
      else {line_freq = 0; set_empty_lines++;}

      if (line_freq < min_freq){
        // Best candidate for eviction
        evict_dcache_idx = dcache_idx;
        min_freq = line_freq;
      }
    }
    assert(evict_dcache_idx==set);
    return evict_dcache_idx;
  }
#endif

u_int64_t frontier_array_base(){
  if (app_has_frontier){
    #if APP==WCC || APP==BFS
      return (u_int64_t) 2*graph->nodes + graph->edges;
    #elif APP==PAGE
      return (u_int64_t) 3*graph->nodes + graph->edges;
    #elif APP==SSSP
      return (u_int64_t) 2*graph->nodes + 2*graph->edges;
    #endif
  }
  return UINT64_MAX;
}

bool is_dirty(u_int64_t tag){
  u_int64_t tag_base = tag << dcache_words_in_line_log2;
  if (tag_base < graph->nodes) return true;
  u_int64_t base_frontier = frontier_array_base();
  return (tag_base >= base_frontier);
}

u_int64_t cache_tag(void * addr) {
  u_int64_t word_index = (u_int64_t) addr >> 2; //4bytes in a word
  word_index = word_index % data_footprint_in_words;
  return word_index >> dcache_words_in_line_log2;
}

#if APP<=5
  u_int64_t cache_tag(void * addr, u_int64_t index) {
    // ret is [0, nodes] for all
    // edge array is [nodes, nodes+edges] (histo is out already)
    if (addr==graph->edge_array){index += graph->nodes;}
    // node array is [N+E, 2N+E] (wcc and bfs are out already)
    else if (addr==graph->node_array){index += graph->nodes + graph->edges;}
    // in pagerank, in_r is [2N+E,]
    else if (addr==in_r){index += 2*graph->nodes + graph->edges;}
    // in SPMV/SSSP, edge_values is [2N+E,]
    else if (addr==graph->edge_values){index += 2*graph->nodes + graph->edges;}
    // SPMV includes a dense vector of size N
    else if (addr==graph->dense_vector){index += 2*graph->nodes + 2*graph->edges;}
    // Frontier
    else if (addr!=ret){
      bool is_list = (addr==frontier_list);
      assert(is_list || addr==bitmap_frontier);
      if (is_list) index += (bitmap_len*GRID_SIZE);
      index += frontier_array_base();
    }
    #if ASSERT_MODE
      assert(index < data_footprint_in_words);
    #endif
    return index >> dcache_words_in_line_log2;
  }
#else
  u_int64_t cache_tag(void * addr, u_int64_t index){
    u_int32_t * array = (u_int32_t *) addr;
    return cache_tag(&array[index]);
  }
#endif

template<typename T>
void write_dcache(int tX,int tY, T * array, T elem){
  // Write data
  array[0] = elem;
  #if DCACHE>0
    // Check tag
    u_int64_t elem_tag = cache_tag(array);
    if (dcache_freq[elem_tag] > 0) dcache_freq[elem_tag]=2;
    else {
      u_int16_t mc_queue_id = die_id(tX,tY)*hbm_channels + (tY*DIE_W+tX)%hbm_channels;
      mc_writebacks[mc_queue_id]++;
      mc_transactions[mc_queue_id]++;
    }
  #endif
}

// NEW VERSION WHERE THE FUNCTION RETURNS THE ACTUAL ARRAY ELEMENT
// Prefetching mode (DCACHE <=4) assumes that the prefetch was done enough in advance to mitigate latency
// Contention is calculated as the memory channel performing one requests per cycle
// A new requests will get contention if the cycle # that is issues is > #transactions
template<typename T>
T get_dcache(int tX,int tY, T * array, u_int64_t & penalty, u_int64_t timer){
  int64_t time = timer + penalty;
  int64_t time_fetched = time;
  #if DCACHE<=4
    time_fetched -= hbm_read_latency;
  #endif
  load(1);

#if DCACHE>=1
  #if DCACHE==1 // Enable dache if data doesn't fit in scratchpad
  if (dataset_cached){
  #elif DCACHE>=2 // Always force cache to be enabled
  {
  #endif
      // freq is the number of hits of a per-neighbour element
      u_int32_t tileid = global(tX,tY);
      // Tags that are currently in the per-tile dcache
      u_int64_t * tags = dcache_tags[tileid];

      // the tag is a global tag (not within the dcache)
      u_int64_t elem_tag = cache_tag(array);
      
      #if DCACHE<=5 // If we allow some type of data hits
      // dcache_freq is a global array (not per tile)
      if (dcache_freq[elem_tag] > 0){
        // ==== CACHE HIT ====
        dcache_hits++;
        penalty += sram_read_latency;
      } else{
      #else
      {
      #endif
        // ==== CACHE MISS ====
        dcache_misses++;
        u_int16_t mc_queue_id = die_id(tX,tY)*hbm_channels + (tY*DIE_W+tX)%hbm_channels;
        int64_t trans_count = (int64_t)mc_transactions[mc_queue_id];
        // Fetched when the last request has satisfied
        if (trans_count > time_fetched) time_fetched = trans_count;

        // May be a negative number!
        int64_t fetch_cycles_ahead = (time - time_fetched);

        int64_t read_latency = 0;
        if (fetch_cycles_ahead < hbm_read_latency) read_latency = (hbm_read_latency-fetch_cycles_ahead);
        //  if (fetch_cycles_ahead < 0) cout << "MC:"<<mc_queue_id<<" latency: " << read_latency << endl;
        mc_transactions[mc_queue_id] = trans_count + 1;
        mc_latency[mc_queue_id] += read_latency;
        penalty += read_latency;
        #if DCACHE==3 // On prefetching modes we add the latency of accessing the specific level
          penalty += sram_read_latency;
        #elif DCACHE==4
          penalty += l2_read_latency;
        #endif

        u_int16_t set_empty_lines = 0;
        int evict_dcache_idx = dcache_replacement_policy(tileid, tags, elem_tag, set_empty_lines);
        #if ASSERT_MODE && DCACHE<=5 // If we allow some type of data hits
          assert(dcache_freq[elem_tag]==0);
          assert(evict_dcache_idx < dcache_size);
        #endif

        u_int64_t evict_tag = tags[evict_dcache_idx];
        
        if (set_empty_lines == 0){// IF CACHE SET FULL
          // Evict Line
          dcache_evictions++;
          #if ASSERT_MODE
            assert(evict_tag < UINT64_MAX);
          #endif
          // Mark the previous element as evicted (dcache_freq = 0)
          bool is_dirty = dcache_freq[evict_tag] > 1;
          dcache_freq[evict_tag] = 0;
          //Writeback only if dirty!
          if (is_dirty){
            mc_writebacks[mc_queue_id]++;
            mc_transactions[mc_queue_id]++;
          }

        } else{ // IF CACHE NOT FULL
          // Increase the occupancy count
          #if ASSERT_MODE
            assert(evict_tag == UINT64_MAX);
          #endif
          dcache_occupancy[tileid]++;
        }

        // Update the dcache tag with the new elem
        tags[evict_dcache_idx] = elem_tag;
        dcache_freq[elem_tag]++;
      }

      #if DIRECT_MAPPED==0
        // If the elem was not in the dcache, it's 1 now. If it was in the dcache, freq is incremented by 1.
        dcache_freq[elem_tag]++;
        u_int32_t set = set_id_dcache(elem_tag);
        check_freq(dcache_freq, tags, set, elem_tag);
      #endif
  }
#endif

  return array[0];
}


// LEGACY VERSION WHERE THE FUNCTION THAT ONLY RETURN THE PENALTY
// Next-line prefetching only
int check_dcache(int tX,int tY, void * array, u_int64_t timer, u_int64_t & time_fetched, u_int64_t & time_prefetched, u_int64_t & prefetch_tag){
  load(1);
  int penalty = sram_read_latency;
#if DCACHE==1
  #if DCACHE==1
  if (dataset_cached){
  #elif DCACHE>=2
  {
  #endif
      // freq is the number of hits of a per-neighbour element
      u_int32_t tileid = global(tX,tY);
      // Tags that are currently in the per-tile dcache
      u_int64_t * tags = dcache_tags[tileid];

      // the tag is a global tag (not within the dcache)
      u_int64_t elem_tag = cache_tag(array);
      
      #if DCACHE<=4
        // If the data I'm fetching was prefetched, then update the time_fetched, and we issue next prefetch now
        if (elem_tag == prefetch_tag){
          time_fetched = time_prefetched;
          time_prefetched = timer;
          prefetch_tag++;
        }
      #else
        time_fetched = timer;
      #endif
      
      #if DCACHE<=5 // If we allow some type of data hits
      // dcache_freq is a global array (not per tile)
      if (dcache_freq[elem_tag] > 0){
        // ==== CACHE HIT ====
        dcache_hits++;
      } else{
      #else
      {
      #endif
        // ==== CACHE MISS ====
        dcache_misses++;
        u_int16_t mc_queue_id = die_id(tX,tY)*hbm_channels + (tY*DIE_W+tX)%hbm_channels;
        u_int64_t trans_count = mc_transactions[mc_queue_id];
        // Fetched when the last request has satisfied
        if (trans_count > time_fetched) time_fetched = trans_count;

        // May be a negative number!
        int fetch_cycles_ahead = (int64_t)timer - (int64_t)time_fetched;
        // if (fetch_cycles_ahead < 0) cout << "fetch_cycles_ahead: " << fetch_cycles_ahead << endl;
        // If cycles ahead is smaller than HBM latency, then we calculate the real latency
        u_int64_t read_latency = 0;
        if (fetch_cycles_ahead < hbm_read_latency) read_latency = (hbm_read_latency-fetch_cycles_ahead);
        //  if (fetch_cycles_ahead < 0) cout << "MC:"<<mc_queue_id<<" latency: " << read_latency << endl;
        mc_transactions[mc_queue_id] = trans_count + 1;
        mc_latency[mc_queue_id] += read_latency;
        penalty += read_latency;

        u_int16_t set_empty_lines = 0;
        int evict_dcache_idx = dcache_replacement_policy(tileid, tags, elem_tag, set_empty_lines);
        #if ASSERT_MODE && DCACHE<=5
          assert(dcache_freq[elem_tag]==0);
          assert(evict_dcache_idx < dcache_size);
        #endif
        u_int64_t evict_tag = tags[evict_dcache_idx];
        
        // An eviction occurs when dcache_freq (valid bit) is 0, but a tag is already in the dcache
        // FIXME: A local Dcache can have evictions by collision of tags since the address space is not aligned locally (e.g. a dcache with 100 lines may get evictions even if its footprint is only 10 vertices and 40 edges, since we don't consider the local address space)
        
        if (set_empty_lines == 0){// IF CACHE SET FULL
          // Evict Line
          dcache_evictions++;
          #if ASSERT_MODE
            assert(evict_tag < UINT64_MAX);
          #endif
          // Mark the previous element as evicted (dcache_freq = 0)
          dcache_freq[evict_tag] = 0;
          //Writeback only if dirty!
          if (is_dirty(evict_tag)){
            //cout << "Writeback: " << evict_tag << endl;
            mc_writebacks[mc_queue_id]++;
            mc_transactions[mc_queue_id]++;
          }

        } else{ // IF CACHE NOT FULL
          // Increase the occupancy count
          #if ASSERT_MODE
            assert(evict_tag == UINT64_MAX);
          #endif
          dcache_occupancy[tileid]++;
        }

        // Update the dcache tag with the new elem
        tags[evict_dcache_idx] = elem_tag;
        dcache_freq[elem_tag]++;
      }

      #if DIRECT_MAPPED==0
        // If the elem was not in the dcache, it's 1 now. If it was in the dcache, freq is incremented by 1.
        dcache_freq[elem_tag]++;
        u_int32_t set = set_id_dcache(elem_tag);
        check_freq(dcache_freq, tags, set, elem_tag);
      #endif
  }
#endif

  return penalty;
}

// LEGACY CODE FOR DALOREX APPS
int check_dcache(int tX,int tY, void * array, u_int64_t elem, u_int64_t timer, u_int64_t & time_fetched, u_int64_t & time_prefetched, u_int64_t & prefetch_tag){
  u_int64_t elem_tag = cache_tag(array,elem) << (dcache_words_in_line_log2 + 2);
  return check_dcache(tX, tY, (void*)elem_tag, timer, time_fetched, time_prefetched, prefetch_tag);
}

// LEGACY CODE FOR DALOREX APPS, No prefetch VERSION
int check_dcache(int tX,int tY, void * array, u_int64_t elem, u_int64_t timer, u_int64_t time_fetched){
  u_int64_t tag = UINT64_MAX;
  return check_dcache(tX, tY, array, elem, timer, time_fetched, time_fetched, tag);
}
// LEGACY CODE FOR DALOREX APPS, No prefetch VERSION
int check_dcache(int tX,int tY, void * array, u_int64_t elem, u_int64_t timer){
  u_int64_t tag = UINT64_MAX;
  return check_dcache(tX,tY,array,elem,timer, timer, timer,tag);
}