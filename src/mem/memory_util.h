

void get_frontier_size(uint32_t node_count){
  // Since EXPRESS_FRONTIER only holds block_ids, the max size should be node_count/32
  // That would guarantee no overlflow to the bitmap frontier, we can have less than that to reduce footprint
  u_int32_t express_list_len = max_macro((node_count/32), 32) + 3;

  #if (GLOBAL_BARRIER>=1 && GLOBAL_BARRIER<=2)
    // Set as big as express list because we emulate barrier by running task4 one time per epoch! So everything needs to fit
    iq_sizes[0] = node_count;
    
  #elif (GLOBAL_BARRIER==3) // Frontier no coalescing
    // If there isn't coalescing of nodes, the list should be of node_count*extra
    express_list_len = node_count * 512;
    iq_sizes[0] = express_list_len;
  #endif
  fr_head_offset = express_list_len;
  fr_tail_offset = fr_head_offset + 1;
  frontier_list_len = (fr_tail_offset+2);
  bitmap_len = (node_count-1)/32 + 1;
}

u_int64_t max_footprint(uint32_t node_count, uint32_t edge_count){
  u_int64_t words =  node_count*3; // ret,node_idx, dense_vector
  words += edge_count*2; //edge_idx, edge_values
  //We don't add the frontier since SPMV is the one using most memory
  return words;
}


u_int32_t calculate_words_for_tags(u_int32_t cache_sets, u_int32_t cache_lines,int words_in_line_log2, u_int32_t local_address_space_log2){
  // Calculate bits that we save on the tag based on the set index
  u_int32_t cache_sets_log2 = 0; u_int32_t temp = cache_sets; while(temp >>= 1) cache_sets_log2++;
  if ((1<<cache_sets_log2) < cache_sets) cache_sets_log2++;
  // cout << "cache_sets_log2:" <<cache_sets_log2<< "\n";
  // cout << "local_address_space_log2:" <<local_address_space_log2<< "\n" << flush;
  
  assert(1<<cache_sets_log2 >= cache_sets);
  if (cache_sets_log2 ==local_address_space_log2) local_address_space_log2++;

  u_int32_t tag_bits = local_address_space_log2 - cache_sets_log2;
  u_int32_t per_line_bits = tag_bits + CACHE_FREQ_BITS + 1; //valid bit. (dirty bit is not needed as we write-back based on address range)
  return (cache_lines * per_line_bits) / 32; // 32 bits per word
}

u_int32_t calculate_cache_space(u_int32_t & min_data_words, int words_in_line_log2, u_int64_t words_per_tile){
  // Calculate sets that is multiple of #ways, below the max space
  u_int32_t min_cache_sets = ((min_data_words >> words_in_line_log2) / CACHE_WAYS);
  u_int32_t min_cache_lines = min_cache_sets * CACHE_WAYS;
  min_data_words = (min_cache_lines << words_in_line_log2);

  // Calculate space dedicated to tags. Consider the total footprint to be cached by this tile, not the entire address space!!
  // Basically the TAG has to be big enough to hold which of the possible addresses that can be cached is actually cached
  // TAG size is pow2 of the ratio between address range and cache size.
  u_int32_t local_address_space_log2 = 0;
  u_int32_t temp = words_per_tile; while(temp >>= 1) local_address_space_log2++;
  
  u_int32_t words_in_tags = calculate_words_for_tags(min_cache_sets, min_cache_lines, words_in_line_log2, local_address_space_log2);
  // Total space in words
  u_int32_t min_cache_words = min_data_words + words_in_tags;
  return min_cache_words;
}


u_int32_t assign_cache_size(u_int32_t remaining_sram_words, u_int32_t & cache_size, int words_in_line_log2, u_int64_t words_per_tile){
  cache_size = remaining_sram_words;
  // Data arrays don't fit in full, use the remaining space
  // Cache size gets modified by the function
  u_int32_t cache_sram_words_required = calculate_cache_space(cache_size, words_in_line_log2, words_per_tile);
  while (cache_sram_words_required > remaining_sram_words){
    cache_size -= (CACHE_WAYS << words_in_line_log2); // Remove one set
    cache_sram_words_required = calculate_cache_space(cache_size, words_in_line_log2, words_per_tile);
  }
  return cache_sram_words_required;
}


u_int32_t setup_cache_sizes(u_int32_t max_capacity, u_int64_t dataset_words_per_tile, u_int32_t proxys_words_per_tile, u_int32_t reserved_sram_words, u_int32_t input_queue_sizes){
  u_int32_t remaining_sram_words = max_capacity - reserved_sram_words;
  #if DCACHE>=5 // Only use a buffer instead of a cache!
    u_int32_t reserved_words_for_dataset = dcache_words_in_line*2; // 2 lines in the L0 to allow spatial locality
  #elif DCACHE==4 // Assume we get spatial locality from the fact that we read from the L2 scratchpad (big)
    u_int32_t reserved_words_for_dataset = min(dcache_words_in_line*512,remaining_sram_words);
  #elif DCACHE==3 // Assume we get spatial locality from the fact that we read from the L1 scrachpad (small)
    u_int32_t reserved_words_for_dataset = min(dcache_words_in_line*512,remaining_sram_words); // 32 cache lines as the L1 scratchpad
  #else
    u_int32_t reserved_words_for_dataset = remaining_sram_words / 5;
    // Calculate minimum cache size
    if (input_queue_sizes > reserved_words_for_dataset) reserved_words_for_dataset = input_queue_sizes;
  #endif

  cout << "reserved_words_for_dataset:" << reserved_words_for_dataset << "\n"; 
  cout << "dataset_words_per_tile:" << dataset_words_per_tile << "\n" << flush;

  #if DCACHE
    u_int32_t min_dcache_data_words = reserved_words_for_dataset;
    // If the footprint is smaller than the region reserved, then the entire footprint is cached
    u_int32_t min_dcache_sram_words_required;
    // If the reserved words is more than half of the dataset, then the entire dataset is cached
    if (reserved_words_for_dataset > (dataset_words_per_tile/2)) min_dcache_sram_words_required = dataset_words_per_tile;
    else
      min_dcache_sram_words_required = calculate_cache_space(min_dcache_data_words, dcache_words_in_line_log2, dataset_words_per_tile);
  #else
    // If there is no DCACHE, then everything is required to be on the scratchpad
    u_int32_t min_dcache_sram_words_required = dataset_words_per_tile;
  #endif

  #if PCACHE
    u_int32_t min_pcache_sram_words_required = min((u_int32_t)8*CACHE_WAYS, proxys_words_per_tile);
  #else
    // If there is no PCACHE, then the entire proxy array is required to be on the scratchpad
    u_int32_t min_pcache_sram_words_required = proxys_words_per_tile;
  #endif

  cout << "remaining_sram_words:" <<remaining_sram_words << "\n" ;
  cout << "min_dcache_sram_words_required:" << min_dcache_sram_words_required << "\n";
  cout << "dataset_words_per_tile:" << dataset_words_per_tile << "\n";
  cout << "min_pcache_sram_words_required:" << min_pcache_sram_words_required << "\n";
  cout << "proxys_words_per_tile:" << proxys_words_per_tile << "\n"<< flush;
  // Make sure that the remaining space is enough for the minimum required cache sizes
  assert(remaining_sram_words >= (min_dcache_sram_words_required + min_pcache_sram_words_required) );

  // Setting up Proxy Cache Size
  u_int32_t budget_to_allocate_pcache = remaining_sram_words - min_dcache_sram_words_required;
  #if FORCE_PROXY_RATIO<=1
    if (budget_to_allocate_pcache > proxys_words_per_tile){
      // If the minimal DCACHE fits even when the Proxy is entirely stored, the have no Proxy cache (consider it as scratchpad), no tags
      pcache_size = proxys_words_per_tile;
      proxys_cached = false;
      remaining_sram_words -= pcache_size;
    } else {
      // Proxy region does not fit in full, use the remaining space, respecting the minimal DCACHE size
      proxys_cached = true;
      u_int32_t words_on_pcache = assign_cache_size(budget_to_allocate_pcache, pcache_size, 0, proxys_words_per_tile);
      remaining_sram_words -= words_on_pcache;
      assert(remaining_sram_words >= min_dcache_sram_words_required);
    }
  #else // Ratio >=2 (the space of the proxy cache will be at most half, it can be less if no space available)
    proxys_cached = true;
    u_int32_t target_footprint = proxys_words_per_tile / FORCE_PROXY_RATIO;
    // If the target is less than the potential allocatable space, we update the buget to the target, otherwise we use the allocatable space
    if (target_footprint < budget_to_allocate_pcache) budget_to_allocate_pcache = target_footprint;
    u_int32_t words_on_pcache = assign_cache_size(budget_to_allocate_pcache, pcache_size, 0, proxys_words_per_tile);
    remaining_sram_words -= words_on_pcache;
    assert(remaining_sram_words >= min_dcache_sram_words_required);
  #endif

  // Setting up Dataset Cache Size
  #if DCACHE>=3
    dcache_size = reserved_words_for_dataset;
    dataset_cached = true;
  #else
    if (remaining_sram_words >= dataset_words_per_tile){ // Check if the entire dataset fits
      dcache_size = dataset_words_per_tile;
      dataset_cached = false;
      remaining_sram_words -= dcache_size;
      #if DCACHE==2
        dataset_cached = true;
      #endif
      
    } else { // DCACHE doesn't fit in full, use the remaining space
      remaining_sram_words -= assign_cache_size(remaining_sram_words, dcache_size, dcache_words_in_line_log2, dataset_words_per_tile);
      dataset_cached = true;
    }
  #endif

  if (dataset_cached) dram_active_words = dataset_words_per_tile;

  // Dcache size is in words
  dcache_footprint_ratio = ((dataset_words_per_tile-1) / dcache_size)+1;
  cout << "Dcache footprint ratio: " << dcache_footprint_ratio << endl << flush;
  
  if (pcache_size){
    u_int32_t pcache_footprint_ratio = ((proxys_words_per_tile-1) / pcache_size)+1;
    cout << "Pcache footprint ratio: " << pcache_footprint_ratio << endl << flush;
  }

  u_int32_t dcache_lines = dcache_size >> dcache_words_in_line_log2;
  dcache_sets = (dcache_lines >> CACHE_WAY_BITS);
  // Pcache lines and words are the same thing. Each line is a single word
  pcache_sets = pcache_size >> CACHE_WAY_BITS;

  #if DCACHE==1
    if (dataset_cached){
      assert((dcache_sets<<CACHE_WAY_BITS) == dcache_lines);
      assert(dcache_lines<<dcache_words_in_line_log2 == dcache_size);
    }
  #endif

  cout << "Dataset Cached: " << dataset_cached << endl;
  cout << "DCache: " << dcache_size << " words, lines: "<<dcache_lines<< ", sets: " << dcache_sets << endl;
  cout << "Proxys Cached: " << proxys_cached << endl;
  cout << "Pcache: " << pcache_size << " words, sets: " << pcache_sets << endl;

  assert(remaining_sram_words>=0);
  return remaining_sram_words;
}



void calculate_storage_per_tile(){

  #if PROXYS > 1
    u_int32_t active_queues = NUM_QUEUES;
    // Since with X Proxys we have X copies of the dataset, the proxy footprint per tile is X times bigger
    u_int32_t proxys_words_per_tile = nodePerTile * PROXYS;
  #else
    u_int32_t active_queues = NUM_QUEUES-1;
    u_int32_t proxys_words_per_tile = 0;
  #endif

  u_int32_t input_queue_sizes = 0;
  u_int32_t total_queue_sizes = 0;
  
  for (int q = 1; q < active_queues; q++){
      input_queue_sizes+=iq_sizes[q];
      total_queue_sizes+=oq_sizes[q];
  }
  total_queue_sizes+=input_queue_sizes;

  // Pad to the next multiple of the cache line size
  while (dataset_words_per_tile%dcache_words_in_line!=0) dataset_words_per_tile++;

  // Total footprint of the dataset (without counting the queues)
  data_footprint_in_words = GRID_SIZE * dataset_words_per_tile; //global variable

  
  cout << "Dataset Footprint: "<<(data_footprint_in_words>>18)<<" MiB\n";  // 1 MiB with 256 * 1024 words
  double dataset_in_kb = (data_footprint_in_words << 5) / 1000;
  double dataset_per_board_mb = (dataset_in_kb / 1000) / BOARDS;
  cout << "Dataset Footprint per Board: "<<dataset_per_board_mb<<" Mb\n";

  u_int32_t io_links_per_tile = PHY_NOCS * 2; // 2 NoC bidirectional
  #if TORUS==1
    io_links_per_tile *= 2; // With Torus 2x more links
  #endif
  u_int32_t io_die_gbits = io_links_per_tile * DIE_W * noc_width * noc_freq;
  u_int32_t io_dies = DIES_BOARD_FACTOR * 4;
  u_int32_t total_io_gbits = io_die_gbits * io_dies;
  vars["IO_DIES"] = io_dies;
  vars["IO_GBITS"] = total_io_gbits;
  vars["DIE_IO_GBITS"] = io_die_gbits;

  // gbit/s is the same as mbit/ms
  cout << "Dataset injection time (ms): "<<(dataset_per_board_mb)/total_io_gbits<<"\n";

  // Storage of the app that uses the most storage (SPMV)
  u_int64_t max_dataset_words_per_core = max_footprint(nodePerTile, edgePerTile);
  u_int64_t max_storage_words = GRID_SIZE * max_dataset_words_per_core;
  cout << "Max Dataset Footprint: "<<(max_storage_words>>18)<<" MiB\n\n";

  // Reserved for Queues and instructions
  u_int32_t reserved_sram_words = total_queue_sizes + code_size_in_words;
  u_int32_t reserved_sram_kib = (reserved_sram_words>>8);
  // Footprint of the proxy (not necesarily fiting in SRAM)
  u_int32_t proxys_kib_per_tile = (proxys_words_per_tile>>8);
  u_int32_t dataset_kib_per_tile = (dataset_words_per_tile>>8);
  cout << "Dataset footprint per tile: " <<dataset_kib_per_tile << " KiB" << endl;
  cout << "Proxys footprint per tile: " << proxys_kib_per_tile << " KiB" << endl;
  cout << "Reserved SRAM per tile: " << reserved_sram_kib << " KiB" << endl << endl;
  cout <<"Ideal SRAM per tile: " << dataset_kib_per_tile + proxys_kib_per_tile + reserved_sram_kib << " KiB" << endl << flush;

  // SRAM_SIZE is in words
  u_int32_t max_sram_size = SRAM_SIZE;
  #if SRAM_SIZE > 1
    u_int32_t min_data_capacity = reserved_sram_words;
    if (min_data_capacity > max_sram_size){
      cout << "WARNING: Not enough SRAM to fit reserved words\n";
      max_sram_size = min_data_capacity;
    }
    #if PCACHE==1
      min_data_capacity += 256*16; // 16 Kib
    #endif
    #if DCACHE==0
      min_data_capacity += dataset_words_per_tile;
      if (min_data_capacity > max_sram_size){
        cout << "WARNING: Not enough SRAM to fit the dataset and reserved space, new size: " << (min_data_capacity>>8) << " KiB\n";
        max_sram_size = min_data_capacity;
      }
    #endif
  #else
    // If set to 0, then it's infinite
    max_sram_size = UINT32_MAX;
  #endif

  // Calculate the cache sizes
  u_int32_t unused_sram_words = setup_cache_sizes(max_sram_size, dataset_words_per_tile, proxys_words_per_tile, reserved_sram_words, input_queue_sizes);
  u_int32_t used_words = max_sram_size - unused_sram_words;
  assert(used_words <= max_sram_size);

  u_int64_t active_kibibytes_per_tile = used_words>>8; // 1 KiB with 256 words
  vars["PER_TILE_ACTIVE_SRAM_KIB"] = active_kibibytes_per_tile;

  #if SRAM_SIZE > 1
    u_int64_t storage_kibibytes_per_tile = max_sram_size >> 8;
  #else
    u_int64_t storage_kibibytes_per_tile = active_kibibytes_per_tile;
  #endif
  vars["PER_TILE_SRAM_KIB"] = storage_kibibytes_per_tile;
  
}
