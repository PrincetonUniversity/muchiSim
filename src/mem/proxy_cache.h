void initialize_proxys(){
  #if PROXYS>1
    for (int i=0;i<PROXYS;i++){
      int * array_int = (int *)malloc(graph->nodes * sizeof(int));
      for (int j = 0; j < graph->nodes; j++) array_int[j] = proxy_default;
      proxys[i] = array_int;
    }
    cout << "Allocated proxys" << endl << flush;
  #endif
}

void writeback_evicted(int * proxy, int tX, int tY, u_int64_t neighbor_to_evict, u_int64_t timer){
  //Only cascade writeback for Histo and SPMV
  #if CASCADE_WRITEBACK
    int dest = 3;
  #else
    int dest = 2;
  #endif
  OQ(dest).enqueue(Msg(getHeadFlit(dest, neighbor_to_evict), HEAD,timer) );
  OQ(dest).enqueue(Msg(proxy[neighbor_to_evict], TAIL,timer) );
  return;
}

int next_valid_line( u_int32_t tileid,u_int16_t * freq, u_int64_t * tags){
  u_int32_t evict_pcache_idx = UINT32_MAX;
  #if PCACHE
    // Search the element of tags whose index into freq has the lowest value
    u_int32_t last_pcache_idx = pcache_last_evicted[tileid];
    u_int16_t line_freq,min_freq = UINT16_MAX;

    // REPLACEMENT POLICY IN HW
    for (int i=1; i<=pcache_size; i++){
      u_int32_t pcache_idx = (last_pcache_idx+i)%pcache_size;
      u_int64_t tag = tags[pcache_idx];

      if (tag<UINT64_MAX){assert(freq[tag]>0); line_freq = freq[tag];}
      else {line_freq = UINT16_MAX;}

      if (line_freq < min_freq){
        // Best candidate for eviction
        evict_pcache_idx = pcache_idx;
        min_freq = line_freq;
      }
    }
    pcache_last_evicted[tileid] = evict_pcache_idx;
  #endif
  #if ASSERT_MODE
    assert(evict_pcache_idx<pcache_size);
  #endif
  return evict_pcache_idx;
}

int replacement_policy(u_int32_t tileid,u_int16_t * freq, u_int64_t * tags, u_int64_t new_tag, u_int16_t & set_empty_lines){
  // Search the element of tags whose index into freq has the lowest value
  u_int32_t evict_pcache_idx = UINT32_MAX;
  u_int16_t line_freq, min_freq = UINT16_MAX;
  u_int16_t empty_lines = 0;

  u_int32_t set_base = set_id_pcache(new_tag) << CACHE_WAY_BITS;
  u_int32_t set_end = set_base + CACHE_WAYS;
  // REPLACEMENT POLICY IN HW
  for (u_int32_t idx=set_base; idx<set_end; idx++){
    u_int64_t tag = tags[idx];
    if (tag<UINT64_MAX) line_freq = freq[tag];
    else {line_freq = 0; empty_lines++;}

    if (line_freq < min_freq){
      // Best candidate for eviction
      evict_pcache_idx = idx;
      min_freq = line_freq;
    }
  }
  #if ASSERT_MODE
    assert(evict_pcache_idx<UINT32_MAX);
  #endif

  set_empty_lines = empty_lines;
  return evict_pcache_idx;
}

void drain_pcache(int tX,int tY, u_int64_t timer){
  #if PCACHE
    u_int16_t dst_rep_id = proxy_id(tX,tY);
    int * proxy = (int *) proxys[dst_rep_id];
    u_int16_t * freq = pcache_freq[dst_rep_id];
    u_int32_t tileid = global(tX,tY);
    u_int64_t * tags = pcache_tags[tileid];

    #if ASSERT_MODE
      assert(pcache_occupancy[tileid] > 0);
    #endif

    int evict_pcache_idx = next_valid_line(tileid, freq, tags);
    // Mark the previous element as evicted (freq = 0)
    u_int64_t evict_tag = tags[evict_pcache_idx];
    #if ASSERT_MODE
      assert(evict_tag < UINT64_MAX);
    #endif
    freq[evict_tag] = 0;
    tags[evict_pcache_idx] = UINT64_MAX;
    writeback_evicted(proxy, tX, tY, evict_tag, timer);
    pcache_occupancy[tileid]--;
    
    // u_int32_t non_empty_lines = 0;
    // for (int l=0;l<pcache_size;l++){if (tags[l]<UINT64_MAX) non_empty_lines++;}
    // assert(non_empty_lines==pcache_occupancy[tileid]);
  #endif
}

void update_pcache(int tX, int tY, u_int32_t neighbor, u_int32_t update){
  u_int16_t dst_rep_id = proxy_id(tX,tY); //META 
  int * proxy = (int*)proxys[dst_rep_id];
  store(1);
  proxy[neighbor] = update;
}

int check_pcache(int tX, int tY, u_int32_t neighbor, u_int64_t timer){
u_int16_t dst_rep_id = proxy_id(tX,tY); //META 
int * proxy = (int*)proxys[dst_rep_id];
load(1);
int value = proxy[neighbor];
#if PCACHE
  // freq is the number of hits of a per-neighbour element
  u_int16_t * freq = pcache_freq[dst_rep_id];
  u_int32_t tileid = global(tX,tY);
  // Tags that are currently in the per-tile pcache
  u_int64_t * tags = pcache_tags[tileid];

  if (proxys_cached){
    if (freq[neighbor] > 0){
      // CACHE HIT
      pcache_hits++;
    } else{
      pcache_misses++;
      // CACHE MISS
      #if ASSERT_MODE
      assert(freq[neighbor]==0);
      #endif
      value = proxy_default;
      
      // We decide which line to store the new value
      // If cache full, we needed to evict an element
      // If not full, then the replacement policy will always select an empty line
      u_int16_t set_empty_lines = 0;
      int evict_pcache_idx = replacement_policy(tileid, freq, tags, neighbor, set_empty_lines);
      u_int64_t evict_tag = tags[evict_pcache_idx];

      if (set_empty_lines == 0){// IF CACHE SET FULL
        // Evict Line
        pcache_evictions++;
        #if ASSERT_MODE
          assert(evict_tag < UINT64_MAX);
          assert(freq[evict_tag] > 0);
        #endif
        // Mark the previous element as evicted (freq = 0)
        freq[evict_tag] = 0;

        #if WRITE_THROUGH==0
          writeback_evicted(proxy, tX, tY, evict_tag, timer);
        #endif
      } else{ // IF CACHE NOT FULL
        #if ASSERT_MODE
          assert(evict_tag == UINT64_MAX);
        #endif
        // Increase the occupancy count
        pcache_occupancy[tileid]++;
      }

      // Update the pcache tag with the new neighbor
      tags[evict_pcache_idx] = neighbor;
      freq[neighbor]++;

    }
    #if DIRECT_MAPPED==0
      // If the neighbor was not in the pcache, it's 1 now. If it was in the pcache, freq is incremented by 1.
      freq[neighbor]++;
      u_int32_t set = set_id_pcache(neighbor);
      check_freq(freq, tags, set, neighbor);
    #endif

  } else{ // No cached, always hit (unless written back)
    if (freq[neighbor] == 0){
      value = proxy_default;
      int occupancy = pcache_occupancy[tileid]++;
      int idx=0;while (idx<pcache_size){if (tags[idx]==UINT64_MAX) break; idx++;}
      assert(idx<pcache_size);
      tags[idx] = neighbor;
      freq[neighbor] = 1;
    }
  }
#endif
return value;
}