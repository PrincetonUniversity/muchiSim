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
  assert(remaining_sram_words > (min_dcache_sram_words_required + min_pcache_sram_words_required) );

  // Setting up Proxy Cache Size
  if ((remaining_sram_words - min_dcache_sram_words_required) > proxys_words_per_tile){
    // If the minimal DCACHE fits even when the Proxy is entirely stored, the have no Proxy cache (consider it as scratchpad), no tags
    pcache_size = proxys_words_per_tile;
    proxys_cached = false;
    remaining_sram_words -= pcache_size;
  } else {
    // Proxy region does not fit in full, use the remaining space, respecting the minimal DCACHE size
    proxys_cached = true;
    remaining_sram_words -= assign_cache_size(remaining_sram_words - min_dcache_sram_words_required, pcache_size, 0, proxys_words_per_tile);
    assert(remaining_sram_words >= min_dcache_sram_words_required);
  }

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

  // Storage of the app that uses the most storage (SPMV)
  u_int64_t max_dataset_words_per_core = max_footprint(nodePerTile, edgePerTile);

  // Pad to the next multiple of the cache line size
  while (dataset_words_per_tile%dcache_words_in_line!=0) dataset_words_per_tile++;

  // Total footprint of the dataset (without counting the queues)
  data_footprint_in_words = GRID_SIZE * dataset_words_per_tile; //global variable

  u_int64_t max_storage_words = GRID_SIZE * max_dataset_words_per_core;
  cout << "Dataset Footprint: "<<(data_footprint_in_words>>18)<<" MiB\n";  // 1 MiB with 256 * 1024 words
  double dataset_in_kb = (data_footprint_in_words << 5) / 1000;
  double dataset_per_board_mb = (dataset_in_kb / 1000) / BOARDS;
  cout << "Dataset Footprint per Board: "<<dataset_per_board_mb<<" Mb\n";

  u_int32_t io_links_per_tile = 4 * 2; // 2 NoC bidirectional (*2) links assuming Torus (*2)
  u_int32_t io_die_gbits = io_links_per_tile * DIE_W * noc_width;
  u_int32_t io_dies = DIES_BOARD_FACTOR * 4;
  u_int32_t total_io_gbits = io_die_gbits * io_dies;
  area["IO_DIES"] = io_dies;
  area["IO_GBITS"] = total_io_gbits;
  area["DIE_IO_GBITS"] = io_die_gbits;

  // gbit/s is the same as mbit/ms
  cout << "Dataset injection time (ms): "<<(dataset_per_board_mb)/total_io_gbits<<"\n";
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
  cout <<"Ideal SRAM per tile: " << dataset_kib_per_tile + proxys_kib_per_tile + reserved_sram_kib << " KiB" << endl;

  #if SRAM_SIZE > 1
    // SRAM_SIZE is in words
    u_int32_t max_sram_size = SRAM_SIZE;
    assert(max_sram_size >= reserved_sram_words);
    assert(PCACHE || max_sram_size > (reserved_sram_words + proxys_words_per_tile) );
  #else
    // If set to 0, then it's infinite
    u_int32_t max_sram_size = UINT32_MAX;
  #endif


  // Calculate the cache sizes
  u_int32_t unused_sram_words = setup_cache_sizes(max_sram_size, dataset_words_per_tile, proxys_words_per_tile, reserved_sram_words, input_queue_sizes);
  u_int32_t used_words = max_sram_size - unused_sram_words;
  assert(used_words <= max_sram_size);

  u_int64_t active_kibibytes_per_tile = used_words>>8; // 1 KiB with 256 words
  area["PER_TILE_ACTIVE_SRAM_KIB"] = active_kibibytes_per_tile;

  #if SRAM_SIZE > 1
    u_int64_t storage_kibibytes_per_tile = SRAM_SIZE >> 8;
  #else
    u_int64_t storage_kibibytes_per_tile = active_kibibytes_per_tile;
  #endif
  area["PER_TILE_SRAM_KIB"] = storage_kibibytes_per_tile;
}



void area_calculation(){
  // ==== Scratchpad storage ==== 
  u_int64_t total_storage_kibibytes = area["PER_TILE_SRAM_KIB"]*GRID_SIZE;
  u_int64_t total_storage_mebibytes = total_storage_kibibytes>>10;

  cout << "Active SRAM Storage per tile: "<<area["PER_TILE_ACTIVE_SRAM_KIB"]<<" KiB\n"; 
  cout << "Total SRAM Storage per tile: "<<area["PER_TILE_SRAM_KIB"]<<" KiB\n"; 
  cout << "Agreggated SRAM Storage: "<< total_storage_mebibytes<<" MiB\n\n";

  #if DCACHE
    uint64_t active_dram_kib = dram_active_words>>8;
    cout << "Active DRAM Storage per tile: "<<(double)active_dram_kib/1024<<" MiB\n"; 
    cout << "Total DRAM Storage per tile: "<<hbm_capacity_per_tile_mib<<" MiB\n"; 
    cout << "Agreggated DRAM Storage: "<< hbm_die_capacity_mib*DIES / 1024 <<" GiB\n\n";
  #endif

  // Make mem area fixed per Tile
  double total_sram_area_mm2 = total_storage_mebibytes*sram_density_mm2_mib; //287kum2 per MiB
  double per_core_sram_area_mm2 = total_sram_area_mm2/GRID_SIZE;

  double per_core_logic_mm2 = 0.043; //40kum2 for the core, 2kum2 for TMU, 1kum2 for cache comparator
  double total_core_area_mm2 = per_core_logic_mm2 * GRID_SIZE;
  area["TOT_CORE_AREA"] = total_core_area_mm2;
  
  uint64_t die_links = 1;
  #if TORUS==0
    #if RUCHE_X == 0
      double per_router_area_mm2 = no_torus_no_ruche_router_area_mm2;
    #elif RUCHE_X == DIE_W_HALF  // No Full Ruche
      double per_router_area_mm2 = (BORDER_TILES_PER_DIE * no_torus_ruche_router_area_mm2 + INNER_TILES_PER_DIE * no_torus_no_ruche_router_area_mm2)/TILES_PER_DIE;
    #else //Ruche2
      die_links = 2; //Twice the links with Full Ruche
      double per_router_area_mm2 = no_torus_ruche_router_area_mm2;
    #endif
  #else // TORUS==1
    // This Ruche area considers that every link has a ruche channel
    #if RUCHE_X == 0
      die_links = 2; //Twice the links with Torus and no Ruche
      double per_router_area_mm2 = torus_no_ruche_router_area_mm2;
    #elif RUCHE_X == DIE_W_HALF  // No Full Ruche
      die_links = 2; //Twice the links with Torus and no Ruche
      double per_router_area_mm2 = (BORDER_TILES_PER_DIE * torus_ruche_router_area_mm2 + INNER_TILES_PER_DIE * torus_no_ruche_router_area_mm2)/TILES_PER_DIE;
    #else //Ruche2
      die_links = 4; //Four times the links with Torus and Ruche
      double per_router_area_mm2 = torus_ruche_router_area_mm2;
    #endif
  #endif
  // #if RUCHE_X>=3
  //   per_router_area_mm2 = per_router_area_mm2*pow(1.19,RUCHE_X-2);
  // #endif

  // Multiply by the number of NoCs (a wide NoC counts as an entra NoC)
  assert(PHY_NOCS==2);
  #if NOC_CONF<2
    u_int16_t nocs_of_32_bits = 1;
  #elif NOC_CONF==2
    u_int16_t nocs_of_32_bits = PHY_NOCS;
  #else
    u_int16_t nocs_of_32_bits = PHY_NOCS + 1;
  #endif
  die_links *= nocs_of_32_bits;
  die_links +=2; //I/O link on the border of the die
  die_links *= BORDER_TILES_PER_DIE; 
  die_links *= 2; //Links are bi-directional

  per_router_area_mm2 *= nocs_of_32_bits;


  double tile_area_mm2 = per_router_area_mm2 + per_core_logic_mm2 + per_core_sram_area_mm2;
  area["PER_TILE_AREA"] = tile_area_mm2;

  double tile_len_mm = sqrt(tile_area_mm2);
  area["PER_TILE_LEN"] = tile_len_mm;

  double die_area_mm2 = tile_area_mm2 * TILES_PER_DIE;

  // Die wires contribute to the area of the PHI.
  // Since our freq is 1GHz, bits/cycle is equal to Gbit/s
  double die_gbits = die_links * noc_width;
  double per_die_phy_area_mm2 = (die_gbits / d2d_substrate_areal_density_gbits_mm2);
  die_area_mm2 += per_die_phy_area_mm2;
  
  double active_mc_area_mm2 = 0;
  #if DCACHE
    active_mc_area_mm2 = hbm_mc_area_mm2;
  #endif
  die_area_mm2 += active_mc_area_mm2;
  

  // Add the area of the processor Dies
  double pkg_processor_dies_area_mm2 = DIES_PER_PACK * die_area_mm2;
  double pkg_hbm_dies_area_mm2 = 0;
  #if DCACHE
    pkg_hbm_dies_area_mm2 += DIES_PER_PACK * hbm_die_area_mm2;
  #endif
  // A IOdie die for each two dies on the border
  double pkg_IOdie_area_mm2 = 2*DIES_PACK_FACTOR*IOdie_area_mm2;

  double pack_area_mm2 = pkg_processor_dies_area_mm2 + pkg_hbm_dies_area_mm2 + pkg_IOdie_area_mm2; 
  double pack_side_mm = sqrt(pack_area_mm2);

  double board_area_mm2 = pack_area_mm2 * PACKAGES_PER_BOARD;
  double board_side_mm = sqrt(board_area_mm2);

  double die_side_mm = sqrt(die_area_mm2);
  double die_border_wire_density = die_gbits / (4*die_side_mm);
  if(die_border_wire_density > d2d_beachfront_density_gbits_mm){
    cout << "ERROR: Die border wire density is lower than the beachfront density\n";
    cout << "Die border wire density: " << die_border_wire_density << "\n";
    cout << "Die Gbits: " << die_gbits << "\n";
    cout << "Die side mm: " << die_side_mm << "\n"<<flush;
    cout << "Beachfront density: " << d2d_beachfront_density_gbits_mm << "\n"<<flush;
    exit(1);
  }

  cout << "Inter-Die #links: "<<die_links<<"\n";
  cout << "Inter-Die (Gbits): "<<(u_int32_t)die_gbits<<"\n";
  cout << "Inter-Die (Gbits/mm): "<<die_border_wire_density<<"\n";
  cout << "Bisection BW of Board (Gbits): " << (die_gbits/4)*DIES_BOARD_FACTOR << "\n";
  cout << "Border IO #Dies: "<<area["IO_DIES"]<<"\n";
  cout << "Border IO Die BW (Gbits): "<<area["DIE_IO_GBITS"]<<"\n";
  cout << "Off-Board BW Total (Gbits): "<<area["IO_GBITS"]<<"\n";
  cout << "Off-Board BW Density (Gbits/mm): "<< (u_int32_t)(area["IO_GBITS"]/(board_side_mm*4))<<"\n";


  double hbm_wire_density = hbm_gbits/die_side_mm;
  assert(hbm_wire_density < d2d_interposer_beachfront_density_gbits_mm);

  std::cout << std::setprecision(2) << std::fixed;
  cout << "Die side: " << die_side_mm << " mm" << endl;
  cout << "Die border wire density: " << die_border_wire_density << " Gbits/mm" << endl;
  cout << "HBM wire density: " << hbm_wire_density << " Gbits/mm\n" << endl;
  area["DIE_SIDE_MM"] = die_side_mm;

  double total_router_area_mm2 = per_router_area_mm2*GRID_SIZE + per_die_phy_area_mm2*DIES + active_mc_area_mm2*DIES + PACKAGES*pkg_IOdie_area_mm2;
  area["TOT_ROUTER_AREA"] = total_router_area_mm2;
  
  cout << "Tiles per Die: " << TILES_PER_DIE << " (" << DIE_W << "x" << DIE_H << ")" << endl;
  cout << "Tile Area: " << tile_area_mm2 << " mm2, side: " << tile_len_mm << " mm" << endl;
  double logic_area = per_core_logic_mm2+per_router_area_mm2;
  cout << "- Core/Rout Area: " << logic_area << " mm2" << endl;
  cout << "- SRAM Area: " << per_core_sram_area_mm2 << " mm2" << endl;
  cout << "- SRAM/Logic Ratio: " << (per_core_sram_area_mm2/logic_area) << "x\n\n";

  cout << "Dies per Package: " << DIES_PER_PACK << " (" << DIES_PACK_FACTOR<<"x" << DIES_PACK_FACTOR << ")" << endl;
  cout << "Die Area: " << die_area_mm2 << " mm2, side: " << die_side_mm << " mm" << endl;
  cout << "- Tiles Area: " << tile_area_mm2 * TILES_PER_DIE << " mm2" << endl;
  cout << "- PHY Area: " << per_die_phy_area_mm2 << " mm2" << endl;
  cout << "- MC Area: " << active_mc_area_mm2 << " mm2\n" << endl;

  cout << "Packages per Board: " << PACKAGES_PER_BOARD << " (" << PACKAGES_BOARD_FACTOR<<"x" << PACKAGES_BOARD_FACTOR << ")" << endl;
  cout << "Package Area: " << pack_area_mm2 << " mm2, side: " << pack_side_mm << " mm" << endl;
  cout << "- IOdie Area: " << pkg_IOdie_area_mm2 << " mm2" << endl;
  cout << "- Dies Area: " << pkg_processor_dies_area_mm2 << " mm2" << endl;
  cout << "- HBM Area: " << pkg_hbm_dies_area_mm2 << " mm2\n" << endl;

  cout << "Boards: " << BOARDS << " ("<<BOARD_FACTOR<<"x"<<BOARD_FACTOR<<")" << endl;
  cout << "Board Area: " << board_area_mm2 << " mm2" << endl;

  double total_area_used_mm2_a = total_core_area_mm2 + total_router_area_mm2 + total_sram_area_mm2 + pkg_hbm_dies_area_mm2*PACKAGES;
  double total_area_used_mm2_b = board_area_mm2 * BOARDS;
  cout << "Total Area: " << total_area_used_mm2_a << " mm2\n" << endl;

  cout << endl << std::flush;
  assert(int(total_area_used_mm2_a)==(int)total_area_used_mm2_b);
  area["TOT_SYSTEM_AREA"] = total_area_used_mm2_a;

  // ==== COST CALCULATIONS ====
  // Assuming size of 13x13mm chiplet die, we get 299 good dies per wafer
  // With the active interposer the chip is 13x18mm, so we get 202 good dies
  double cost_per_7nm_wafer = 6047.0;
  double single_processor_die_cost = (cost_per_7nm_wafer/299.0);
  double processor_die_cost = single_processor_die_cost * DIES_PER_PACK;

  double single_processor_active_interposer_cost = (cost_per_7nm_wafer/202);
  double processor_active_interposer_cost = single_processor_active_interposer_cost * DIES_PER_PACK;

  double single_IOdie_cost = (cost_per_7nm_wafer/1433);
  double IOdie_cost = single_IOdie_cost * 2*DIES_PACK_FACTOR;

  double hbm_cost = hbm_cost_per_die * DIES_PER_PACK;
  double bonding_overhead_2d = 0.05;
  double substrate_overhead = 0.10;
  double passive_interposer_and_hbm_bonding_overhead = 0.20;

  //Bonding first both to the interposer, and then the interposer to the substrate
  double interposer_and_hbm_bonding_cost = processor_die_cost * passive_interposer_and_hbm_bonding_overhead;
  // Now the passive interposer has twice the area, so we need to double the base cost
  double substrate_and_interposer_bonding_cost = (processor_die_cost*2) * (bonding_overhead_2d + substrate_overhead);
  double packaging_cost_25d = interposer_and_hbm_bonding_cost + substrate_and_interposer_bonding_cost;

  // 3D
  double hbm_bonding_cost = processor_die_cost * bonding_overhead_2d;
  double substrate_and_processor_bonding_cost = processor_active_interposer_cost * (bonding_overhead_2d + substrate_overhead);
  double packaging_cost_3d =  hbm_bonding_cost + substrate_and_processor_bonding_cost;
  cout << "--Dies 3D Cost: " << processor_active_interposer_cost << " $" << endl;
  cout << "--Dies 2.5D Cost: " << processor_die_cost << " $" << endl;

  // 2D Bonding directly to the substrate
  double packaging_cost_2d = processor_die_cost * (bonding_overhead_2d + substrate_overhead);
  cout << "--Dies 2D Cost: " << processor_die_cost << " $" << endl;

  double IOdie_packaging_cost = IOdie_cost * (bonding_overhead_2d + substrate_overhead);
  packaging_cost_3d += IOdie_packaging_cost;
  packaging_cost_25d += IOdie_packaging_cost;
  packaging_cost_2d += IOdie_packaging_cost;

  double pkg_cost_3d  = processor_active_interposer_cost + hbm_cost + IOdie_cost + packaging_cost_3d;
  double pkg_cost_25d = processor_die_cost + hbm_cost + IOdie_cost + packaging_cost_25d;
  double pkg_cost_2d  = processor_die_cost + IOdie_cost + packaging_cost_2d;

  cout << "--HBM Cost: " << hbm_cost << " $" << endl;
  cout << "--IOdie Cost: " << IOdie_cost << " $" << endl;
  cout << "--Packaging Cost 3d: " << packaging_cost_3d << " $" << endl;
  cout << "--Packaging Cost 2.5d: " << packaging_cost_25d << " $" << endl;
  cout << "--Packaging Cost 2d: " << packaging_cost_2d << " $" << endl;
  cout << "Package Cost 3d: " << pkg_cost_3d << " $" << endl;
  cout << "Package Cost 2.5d: " << pkg_cost_25d << " $" << endl;
  cout << "Package Cost 2d: " << pkg_cost_2d << " $" << endl;

  double board_cost = board_area_mm2 / 1000;
  cout << "--Board Cost: " << board_cost << " $" << endl;
  cout << "System Cost 3d: " << pkg_cost_3d * PACKAGES + board_cost << " $" << endl;
  cout << "System Cost 2.5d: " << pkg_cost_25d * PACKAGES + board_cost << " $" << endl;
  cout << "System Cost 2d: " << pkg_cost_2d * PACKAGES + board_cost << " $" << endl;
  cout << endl << std::flush;
}
