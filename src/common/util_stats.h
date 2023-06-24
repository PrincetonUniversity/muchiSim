
void init_perf_counters() {
  mc_transactions = (u_int64_t *) calloc(total_hbm_channels, sizeof(u_int64_t));
  mc_writebacks = (u_int64_t *) calloc(total_hbm_channels, sizeof(u_int64_t));
  mc_latency = (u_int64_t *) calloc(total_hbm_channels, sizeof(u_int64_t));


  for (int i=0;i<BOARDS;i++){
    inter_board_traffic[i] = 0;
  }
  for (int i=0;i<DIES;i++){
    inter_die_traffic[i] = 0;
  }
  for (int i=0;i<PACKAGES;i++){
    inter_pack_traffic[i] = 0;
  }

  total_counters = new u_int64_t**[GRID_X];
  frame_counters = new u_int32_t**[GRID_X];
  for (int i=0; i < GRID_X; i++){
    total_counters[i] = new u_int64_t*[GRID_Y];
    frame_counters[i] = new u_int32_t*[GRID_Y];
    for (int j=0; j < GRID_Y; j++){
      total_counters[i][j] = new u_int64_t[GLOBAL_COUNTERS];
      frame_counters[i][j] = new u_int32_t[GLOBAL_COUNTERS];
      for (int c = 0; c < GLOBAL_COUNTERS; c++){
        total_counters[i][j][c] = 0;
        frame_counters[i][j][c] = 0;
      }
    }
  }
  print_counters = new u_int64_t**[PRINT_X];
  for (u_int32_t i=0; i<PRINT_X;i++){
    print_counters[i] = new u_int64_t*[PRINT_Y];
    for (u_int32_t j=0; j<PRINT_Y;j++){
      print_counters[i][j] = new u_int64_t[GLOBAL_COUNTERS];
    }
  }
  
}

void print_configuration(){
  // Print with format thousands separator
  std::cout.imbue(std::locale(""));

  cout << "Node per tile: "<<nodePerTile<<"\n";
  cout << "Edge per tile: "<<edgePerTile<<"\n";
  // Print Configuration Run
  cout << "Rectangle is X: "<<GRID_X<<", Y:"<<GRID_Y<<"\n";
  cout << "Total of "<<GRID_X*GRID_Y<<" tiles\n\n";
  cout << "Ruche is X: "<<RUCHE_X<<", Y:"<<RUCHE_Y<<"\n";
  cout << "PROXY_W: "<<PROXY_W<<"\n";
  cout << "DIE_W: "<<DIE_W<<"\n";
  cout << "PACK_W: "<<PACK_W<<"\n";
  cout << "BOARD_W: "<<BOARD_W<<"\n";
  cout << "Network Torus: "<< TORUS<<"\n";
  cout << "Physical NoC: "<< PHY_NOCS<<"\n";
  cout << "Wide NoC: "<< WIDE_NOC<<"\n";
  cout << "Routing Proxy: "<< PROXY_ROUTING<<"\n";
  // cout << "STEAL_W: "<< STEAL_W<<"\n";
  cout << "Cascade Writeback: "<<CASCADE_WRITEBACK<<"\n";
  cout << "Queue Mode: "<< QUEUE_PRIO<<"\n";
  cout << "Shuffle Dataset: "<<SHUFFLE<<"\n";
  cout << "ASSERT_MODE: "<<ASSERT_MODE<<"\n";
  cout << "Barrier: "<<GLOBAL_BARRIER<<"\n";
  cout << "OQ flits: "<<oq_sizes[1]<<" ; "<<oq_sizes[2]<<" ; "<<oq_sizes[3]<<"\n";
  cout << "IQ flits: "<<iq_sizes[1]<<" ; "<<iq_sizes[2]<<" ; "<<iq_sizes[3]<<"\n";
  cout << "OQ messages: "<<oq_sizes[1]/num_task_params[1]<<" ; "<<oq_sizes[2]/num_task_params[2]<<" ; "<<oq_sizes[3]/num_task_params[3]<<"\n";
  cout << "IQ messages: "<<iq_sizes[1]/num_task_params[1]<<" ; "<<iq_sizes[2]/num_task_params[2]<<" ; "<<iq_sizes[3]/num_task_params[3]<<"\n";
  cout << "max_task2 size: "<<max_task_chunk<<"\n";

  cout << "io_factor_t2: "<<io_factor_t2<<"\n";
  cout << "io_factor_t3: "<<io_factor_t3<<"\n";
  cout << "io_factor_t3b: "<<io_factor_t3b<<"\n";
  cout << std::flush;

  // ==== Check Configurations Allowed ====
  #if APP!=HISTO
    assert(oq_sizes[dest_qid] >= 64);
  #endif
  assert(io_factor_t2);
  assert(io_factor_t3);
  assert(io_factor_t3b);
}

#define STATS_COUNTERS 8

const string stats_names[STATS_COUNTERS] = {"Collision In", "Collision Out", "Collision End","Task1","Task2","Task3", "Core Active", "Router Active"};
const u_int64_t stats_id[STATS_COUNTERS] = {MSG_IN_COLLISION, MSG_OUT_COLLISION, MSG_END_COLLISION, TASK1, TASK2, TASK3, IDLE, ROUTER_ACTIVE};

double calculateSD(int total, double mean, double * data) {
  double sd = 0.0;
  int i;
  for (i = 0; i < total; ++i) {
      sd += pow(data[i] - mean, 2);
  }
  return sqrt(sd / total);
}
void printMatrixf(double * data){
  for (uint32_t j=0; j<PRINT_Y; j++){
    for (uint32_t i=0; i<PRINT_X; i++){
      uint32_t idx = j*PRINT_X + i;
      cout << ((uint32_t) data[idx]) << "\t";
    }
   cout << "\n" << std::flush;
  }
}

u_int64_t get_final_time(){
  //Get max value from core_timer array
  u_int64_t max = 0;
  for (int i=0; i<GRID_SIZE*smt_per_tile; i++){
    if (core_timer[i] > max) max = core_timer[i];
  }
  return max;
}

void print_counter_stats(bool last_print, u_int64_t * cumm_counters, u_int64_t *** counters, int len_x, int len_y){
  bool frame = (counters == print_counters);
  string pre;
  if (frame) pre = "F";
  else pre = "A";

  // Arrays to store the stats
  double stats_count[STATS_COUNTERS*PRINT_SIZE];
  double stats_max[STATS_COUNTERS];
  double stats_min[STATS_COUNTERS];
  double stats_avg[STATS_COUNTERS];
  for (int i=0;i<STATS_COUNTERS;i++){
    stats_max[i] = 0;
    stats_min[i] = std::numeric_limits<double>::max();
    stats_avg[i] = 0;
  }

  double counters_f[GLOBAL_COUNTERS];
  for (int c = 0; c <GLOBAL_COUNTERS; c++) cumm_counters[c] = 0;

  for (u_int32_t i=0; i<len_x; i++){
    for (u_int32_t j=0; j<len_y; j++){

      u_int64_t time;
      // To distinguish between frame and accum counting
      if (frame) time = span_timer[global(i,j)];
      else if (last_print){
        time = get_final_time();
        //for (int c = 0; c < GLOBAL_COUNTERS; c++) counters[i][j][c] += frame_counters[i][j][c];
      }
      else time = prev_timer[global(i,j)];
      

      double time_div100 = time/100.0;
      for (int c = 0; c <GLOBAL_COUNTERS; c++){
        u_int64_t counter;
        // Calculate active core cycles as total cycles minus idle cycles
        if (c==IDLE) counter = time - (counters[i][j][c] / smt_per_tile);
        else counter = counters[i][j][c];
        
        counters_f[c] = (double)counter/time_div100;
        cumm_counters[c] += counter;
      }
      
      if ((i < PRINT_X) && (j < PRINT_Y)){
        u_int32_t local_idx = j*PRINT_X + i;
        assert(local_idx < PRINT_SIZE);
        
        for (int s=0;s<STATS_COUNTERS;s++){
          stats_count[s*PRINT_SIZE + local_idx] = counters_f[stats_id[s]];
          if (counters_f[stats_id[s]] > stats_max[s]) stats_max[s] = counters_f[stats_id[s]];
          if (counters_f[stats_id[s]] < stats_min[s]) stats_min[s] = counters_f[stats_id[s]];
          stats_avg[s] += counters_f[stats_id[s]];
        }

        #if PRINT>1
          if (last_print) cout << "X:"<<i<<"\t Y:"<<j<<"\tR: "<<counters_f[ROUTER_ACTIVE]<<"\%\t  C: "<< counters_f[IDLE]<< "%\t --- Tasks: "<< counters_f[TASK1]<<"\t| "<<counters_f[TASK2]<<"\t| "<<counters_f[TASK3]<<"\t| "<<counters_f[TASK4]<<"\t| LD:"<<counters_f[LOAD]<<"\t|ST: "<<counters_f[STORE]<<"\t|MSG1: "<<counters_f[MSG_1]<<"\t|MSG2: "<<counters_f[MSG_2]<<"\n";
        #endif
      }
    }
  }

  //  ==== PRINT MATRICES OF COUNTERS ====
  std::cout << std::setprecision(0) << std::fixed;
  for (uint32_t s = 0; s < STATS_COUNTERS; s++)
    #if PRINT<2
    if (false && last_print)
    #endif
    {
      cout << "\n"<< pre << "M" << stats_names[s] << "\n";
      printMatrixf(&stats_count[s*PRINT_SIZE]);
    }
  cout << "\n" << std::flush;

  //  ==== PRINT MAIN STATISTICS OF THESE COUNTERS ====
  for (int s = 0; s < STATS_COUNTERS; s++){
    stats_avg[s] = stats_avg[s]/PRINT_SIZE;
    double sd = calculateSD(PRINT_SIZE, stats_avg[s], &stats_count[s*PRINT_SIZE]);
    cout << pre << stats_names[s] << " Avg:"<<stats_avg[s]<<", SD:"<<sd<<", Max:"<< stats_max[s]<<", Min:"<< stats_min[s]<<"\n";
  }
  
    // Based on how many times T2 and T3 were invoked we can calculate 
  // the average number of hops per task (need to multiply #flits in the task)
  uint64_t t1_invocations = cumm_counters[T1];
  uint64_t t2_invocations = cumm_counters[T2];
  uint64_t t3_invocations = cumm_counters[T3];
  uint64_t t3b_invocations = cumm_counters[T3b];
  uint64_t t4_invocations = cumm_counters[T4];

  cout << "T1 invocations: " << t1_invocations << "\n";
  cout << "T2 invocations: " <<  t2_invocations << "\n";
  cout << "T3 invocations: " <<  t3_invocations << "\n";
  cout << "T3b invocations: " <<  t3b_invocations << "\n";
  u_int64_t both_t3_invocations = t3_invocations + t3b_invocations;
  cout << "Total3 invocations: " << both_t3_invocations << "\n";
  cout << "Total4 invocations: " << t4_invocations << "\n\n";

  cout << "T3/T2 ratio: " <<  (double)t3_invocations/t2_invocations << "\n";
  cout << "T3b/T3 ratio: " <<  (double)t3b_invocations/t3_invocations << "\n\n";

  cout << "T1 avg ex cycles: " <<  (double)cumm_counters[TASK1]/t1_invocations << "\n";
  cout << "T2 avg ex cycles: " <<  (double)cumm_counters[TASK2]/t2_invocations << "\n";
  cout << "T3&b avg ex cycles: " <<  (double)cumm_counters[TASK3]/both_t3_invocations << "\n";
  cout << "T4 avg ex cycles: " <<  (double)cumm_counters[TASK4]/t4_invocations << "\n";

  double T2_params_loaded = t2_invocations * num_task_params[1];
  double T3_params_loaded = t3_invocations * num_task_params[2];
  double T3b_params_loaded = t3b_invocations * num_task_params[3];
  cout << "Avg. hops NOC1: " <<  (double)cumm_counters[MSG_1]/T2_params_loaded << "\n";
  cout << "Avg. hops NOC2: " <<  (double)cumm_counters[MSG_2]/T3_params_loaded << "\n";
  cout << "Avg. hops NOC3: " <<  (double)cumm_counters[MSG_3]/T3b_params_loaded << "\n";

  cout << "Avg. latency NOC1 (T2): "<<(double)cumm_counters[MSG_1_LAT]/t2_invocations<<"\n";
  cout << "Avg. latency NOC2 (T3): "<<(double)cumm_counters[MSG_2_LAT]/t3_invocations<<"\n";
  cout << "Avg. latency NOC3 (T3b): "<<(double)cumm_counters[MSG_3_LAT]/t3b_invocations<<"\n";

  
  return;
}


void print_stats_frame(){
  u_int64_t this_counters[GLOBAL_COUNTERS];
  cout << "\n===PRINTING FRAME STATS\n";
  print_counter_stats(false, &this_counters[0], print_counters, PRINT_X, PRINT_Y);
}

void print_stats_acum(bool last_print){
  uint64_t wall_time = get_final_time();
  //Total time and time before the end of the cool down phase, when less than SQRT(cores) are active
  if (cool_down_time==0) cool_down_time=wall_time;
  double wall_time_div100 = wall_time/100;

  u_int64_t accum_counters[GLOBAL_COUNTERS];
  cout << "\n===PRINTING ACUM STATS at time: " << wall_time << "\n";
  print_counter_stats(last_print, &accum_counters[0], total_counters, GRID_X, GRID_Y);

  if (last_print){
    cout << "\nRuntime at program end  in K cycles: " <<  (wall_time/1000) << "\n";
    cout << "Runtime before cooldown in K cycles: " <<  (cool_down_time/1000) << "\n\n\n";
  }
  u_int64_t total_ops = accum_counters[TASK1]+accum_counters[TASK2]+accum_counters[TASK3]+accum_counters[TASK4];


  #if PCACHE
  cout << "\n---Pcache---\n";
  cout << "Pcache Size: "<<pcache_size<<"\n";
  cout << "Cached: "<<proxys_cached<<"\n";
  cout << "  PHits: "<<pcache_hits<<"\n";
  cout << "  PMisses: "<<pcache_misses<<"\n";
  uint64_t total_requests_pcache = pcache_hits + pcache_misses;
  cout << "  PHit Rate (hit/miss): "<<(double)pcache_hits*100/total_requests_pcache<<"%\n";
  cout << "  PEvictions: "<<pcache_evictions<<"\n";
  cout << "  PEviction Rate (evict/misses): "<<(double)pcache_evictions*100/pcache_misses<<"%\n";
  #endif

  #if DCACHE
  cout << "\n---DCache---\n";
  cout << "DCache Size: "<<dcache_size<<"\n";
  cout << "DRAM words: "<<dram_active_words<<"\n";
  cout << "  DHits: "<<dcache_hits<<"\n";
  cout << "  DMisses: "<<dcache_misses<<"\n";
  if (dram_active_words){
    u_int64_t total_transactions=0, total_writes=0, max_trans = 0;
    double avg_latency=0;
    int max_mc = 0;
    for (int i=0;i<total_hbm_channels;i++){
      u_int64_t transactions = mc_transactions[i];
      total_transactions += transactions;
      total_writes += mc_writebacks[i];

      double avg_latency_in_mc = ((double)mc_latency[i]/transactions);
      avg_latency += avg_latency_in_mc;
      if (transactions > max_trans){
        max_trans = transactions;
        max_mc = i;
      }
    }

    uint64_t total_requests_dcache = dcache_hits + dcache_misses;
    cout << "  DHit Rate (hit/miss): "<<(double)dcache_hits*100/total_requests_dcache<<"%\n";
    cout << "  DEvictions: "<<dcache_evictions<<"\n";
    cout << "  DEviction Rate (evict/misses): "<<(double)dcache_evictions*100/dcache_misses<<"%\n";
    cout << "  MC Transactions: "<<total_transactions<<"\n";
    cout << "  MC Writes: "<<total_writes<<"\n";
    cout << "Avg. Transactions: " << total_transactions/total_hbm_channels/wall_time_div100 <<"% , Writes: "<< total_writes/total_hbm_channels/wall_time_div100<< "%" <<", Avg. Latency: "<<avg_latency/total_hbm_channels<<"\n";
    cout << "Max MC id: "<< max_mc <<" of "<< total_hbm_channels <<". Transactions: " << max_trans/wall_time_div100 <<"% , Writes: "<< mc_writebacks[max_mc]/wall_time_div100<< "%" <<", MC mean Latency: "<<(double)mc_latency[max_mc]/max_trans<<"\n";
  }
  #endif

  u_int64_t dynamic_cores = 0;
  u_int64_t dynamic_mem = 0;
  // Separate compute and mem energy
  // Add IQ param loads, Queue pop/push

  // TASK3 is both T3 and T3b
  //Mem ops here only consider InstructionDecode+ALU (mem rw is accounted in dynamic_mem)
  #if APP==PAGE
  dynamic_cores+= accum_counters[TASK1]*6 + (accum_counters[TASK1]*24/15); // Mostly Integer Aritmetic and Loads, one in 15 ops takes 24 pJ more
  dynamic_cores+= accum_counters[TASK3]*6 + (accum_counters[TASK3]*19/24); // If, Load, Store, Logical ops, one in 24 ops takes 19 pJ more
  #else
  dynamic_cores+= accum_counters[TASK1]*8; // Mostly Integer Aritmetic and Loads, 2 integer mul
  dynamic_cores+= accum_counters[TASK3]*6; // If, Load, Store, Logical ops
  #endif

  dynamic_cores+= accum_counters[TASK2]*6; // Only Loads and Add
  // TASK4 is the frontier processing
  dynamic_cores+= accum_counters[TASK4]*6; // Load, CMP, IF.
  
  // Task2 params are read from ChannelQ to router and then written again into the TQ
  // Task3 params are read from ChannelQ to router and then written again into the TQ
  // Task1 and Task4 write directly into the TQ, so that energy is already accounted
  double all_loaded_params = accum_counters[T2]*num_task_params[1] + accum_counters[T3]*num_task_params[2] + accum_counters[T3b]*num_task_params[3];
  accum_counters[LOAD] += all_loaded_params;
  accum_counters[STORE]+= all_loaded_params;
  dynamic_mem += accum_counters[LOAD]*sram_read_word_energy;
  dynamic_mem += accum_counters[STORE]*sram_write_word_energy;

  double cache_lookup = sram_read_word_energy + CACHE_WAYS*0.5; // 5.8 pJ for tag read from SRAM + 0.5pJ per tag comparison
  #if PCACHE
    dynamic_mem += pcache_hits * cache_lookup; //pJ per cache lookup
    dynamic_mem += pcache_misses * cache_lookup; //miss in proxy cache doesn't cause a DRAM access
  #endif

  double mc_wire_energy_pj_25d = 0;
  double mc_wire_energy_pj_3d = 0;
  double dram_energy_pj_25d = 0;
  double dram_energy_pj_3d = 0;
  // HBM die has 1024 pins in 42mm of perimeter, 24 pins/mm
  #if DCACHE
    u_int64_t mc_requests = (dcache_evictions + dcache_misses);

    // The cost of a lookup regardless whether it hits or not
    u_int64_t dcache_lookup_energy = (dcache_hits + dcache_misses) * cache_lookup;

    // On average, the data and the header go through half the die on average
    
    double avg_distance_mm = (area["PER_TILE_LEN"] * DIE_W_HALF);
    mc_wire_energy_pj_25d = mc_requests * (hbm_channel_bit_width+noc_width) * avg_distance_mm;
    mc_wire_energy_pj_3d = mc_wire_energy_pj_25d/4; // 4x less energy in 3D
    // DRAM energy of HBM reads
    u_int64_t hbm_access_energy_pj = mc_requests * hbm_channel_bit_width * hbm_read_energy_pj_bit;
    
    // The background energy and refresh energy of the DRAM
    // Based on how long we run (walltime), we can estimate the number of refreshes
    // Consider dram_active_words
    u_int64_t hbm_refresh_periods = wall_time / hbm_refresh_time_ns;
    //double hbm_reads_during_refresh_periods = mc_requests / hbm_refresh_periods;
    u_int64_t bits_refreshed = ((dram_active_words<<5) * wall_time) / hbm_refresh_time_ns;
    u_int64_t hbm_refresh_energy_pj = bits_refreshed * (hbm_activation_energy_pj_bit + hbm_precharge_energy_pj_bit);
    cout << "HBM refresh energy (nJ): "<<hbm_refresh_energy_pj/1000 << "\n";
    cout << "HBM access energy (nJ): "<<hbm_access_energy_pj/1000 << "\n";
    cout << "MemC wire energy 2.5d (nJ): "<<mc_wire_energy_pj_25d/1000 << "\n";
    cout << "MemC wire energy 3d (nJ): "<<mc_wire_energy_pj_3d/1000 << "\n";
    dram_energy_pj_25d = dcache_lookup_energy + mc_wire_energy_pj_25d + hbm_access_energy_pj + hbm_refresh_energy_pj;
    dram_energy_pj_3d = dcache_lookup_energy + mc_wire_energy_pj_3d + hbm_access_energy_pj + hbm_refresh_energy_pj;
  #endif
  double dynamic_mem_25d = dynamic_mem + dram_energy_pj_25d;
  double dynamic_mem_3d = dynamic_mem + dram_energy_pj_3d;

   
  u_int64_t totalEdgesProcessed=0;
  u_int64_t totalFrontierNodesProcessed = 0;
  for (int i=0; i<COLUMNS; i++){
    totalEdgesProcessed += numEdgesProcessed[i];
    totalFrontierNodesProcessed += numFrontierNodesPushed[i];
  }
  if (totalEdgesProcessed == 0) totalEdgesProcessed = totalFrontierNodesProcessed;
  if (totalFrontierNodesProcessed == 0) totalFrontierNodesProcessed = totalEdgesProcessed;

  // Timing variables
  double freq = 1.0;
  double pow12 = pow(10,12);
  double nanoseconds = wall_time/freq;
  double miliseconds = nanoseconds/1000000;
  double seconds = miliseconds/1000;


  // === ROUTER ENERGY === 
  // Energy of moving 32 bits one hop, similar cost as an ALU op
  u_int64_t total_messages = accum_counters[MSG_1]+accum_counters[MSG_2]+accum_counters[MSG_3];
  u_int64_t dynamic_routers_25d = total_messages * energy_per_routing_flit;
  u_int64_t dynamic_routers_3d = dynamic_routers_25d;
  
  u_int64_t total_inter_board_traffic = 0;
  u_int64_t total_inter_die_traffic = 0;
  u_int64_t total_ruche_traffic = 0;
  u_int64_t total_inter_pack_traffic = 0;
  for (int i=0; i<BOARDS; i++) {total_inter_board_traffic += inter_board_traffic[i];}
  for (int i=0; i<PACKAGES; i++) {total_inter_pack_traffic += inter_pack_traffic[i];}
  for (int i=0; i<DIES; i++){total_inter_die_traffic += inter_die_traffic[i] + ruche_traffic[i];}

  u_int64_t tile_hop_messages = total_messages - all_loaded_params; //Remove the energy from getting from core to router as that is local
  tile_hop_messages = tile_hop_messages - total_inter_board_traffic - total_inter_die_traffic;

  // Energy of hop in the wire (pJ)
  double inter_tile_pJ_bit_25d = in_die_wire_pj_bit_mm * area["PER_TILE_LEN"];
  double inter_tile_pJ_bit_3d = in_die_wire_pj_bit_mm * ((area["PER_TILE_LEN"]*DIE_W+5)/DIE_W);
  #if TORUS>=1
    inter_tile_pJ_bit_25d *= 2; // Twice the distance in a torus
    inter_tile_pJ_bit_3d *= 2; // Twice the distance in a torus
  #endif

  dynamic_routers_25d += tile_hop_messages * noc_width * inter_tile_pJ_bit_25d;
  dynamic_routers_3d += tile_hop_messages * noc_width * inter_tile_pJ_bit_3d;
  // Adding the energy of the inter-die and inter-board traffic
  #if BOARD_W < GRID_W
    dynamic_routers_25d += total_inter_board_traffic * noc_width * inter_board_pJ_bit;
    dynamic_routers_3d  += total_inter_board_traffic * noc_width * inter_board_pJ_bit;
  #endif

  #if DCACHE
    dynamic_routers_25d += total_inter_die_traffic * noc_width * d2d_substrate_hop_energy_pj_per_bit;
  #else
    dynamic_routers_25d += total_inter_die_traffic * noc_width * d2d_interposer_hop_energy_pj_per_bit;
  #endif
  dynamic_routers_3d += total_inter_die_traffic * noc_width * d2d_interposer_hop_energy_pj_per_bit;

  #if PACK_W < BOARD_W
    dynamic_routers_25d += total_inter_pack_traffic * noc_width * inter_pack_pJ_bit;
    dynamic_routers_3d  += total_inter_pack_traffic * noc_width * inter_pack_pJ_bit;
  #endif
  u_int64_t dynamic_energy_pj_25d = dynamic_cores + dynamic_mem_25d + dynamic_routers_25d;
  u_int64_t dynamic_energy_pj_3d = dynamic_cores + dynamic_mem_3d + dynamic_routers_3d;


  // === Static power of silicon area === 
  double leakage_mem = (area["PER_TILE_ACTIVE_SRAM_KIB"]*GRID_SIZE)*625; //0.6nW/byte, 625nW/KiB, 0.64 mW/MiB, ~0.1mW/mm2
  // (Ariane=1mW leakage at CMOS22nm, 0.5mm2, 500kum2) ->  200nW per kum2 -> 0.2 mW per mm2
  // Leon2 Sparc, 0.066mW at 0.45V FinFET,  66.000nW for 35KGates ~ 2nW per Kum2, 0.002 mW per mm2
  double nw_leakage_per_mm2_area = 0.1*pow(10,6);
  double leakage_route = nw_leakage_per_mm2_area * area["TOT_ROUTER_AREA"];
  double leakage_cores = nw_leakage_per_mm2_area * area["TOT_CORE_AREA"];
  double leakage_power_nw = leakage_mem + leakage_route + leakage_cores;
  double leakage_energy_pj = leakage_power_nw*miliseconds;

  // === Energy breakdown === 
  double energy_cores_pj = leakage_cores*miliseconds + dynamic_cores;

  double energy_route_pj_25d = leakage_route*miliseconds + dynamic_routers_25d;
  double energy_route_pj_3d = leakage_route*miliseconds + dynamic_routers_3d;
  
  double energy_mem_pj_25d = leakage_mem*miliseconds + dynamic_mem_25d;
  double energy_mem_pj_3d = leakage_mem*miliseconds + dynamic_mem_3d;

  // === Total energy === 
  double leakage_energy_nj = leakage_energy_pj/1000;
  double dynamic_energy_nj = dynamic_energy_pj_25d/1000;
  double dynamic_energy_nj_3d = dynamic_energy_pj_3d/1000;

  double total_energy_nj    = leakage_energy_nj + dynamic_energy_nj;
  double total_energy_nj_3d = leakage_energy_nj + dynamic_energy_nj_3d;
  // Power in miliwatts
  double avg_power_mW = (1000*total_energy_nj / nanoseconds); // (J / s) is W

  if (last_print){
    std::cout << std::setprecision(2) << std::fixed;
    cout << "\nRectangle used was X: "<<GRID_X<<", Y:"<<GRID_Y<<", so "<< GRID_SIZE<<" cores\n";
    cout << "Area used (mm2): "<<area["TOT_SYSTEM_AREA"]<<"\n";
    cout << "Tile area (mm2): "<<area["PER_TILE_AREA"]<<"\n";
  }
  std::cout << std::setprecision(0) << std::fixed;
  cout << "\n---Energy---\n";
  cout << "Leakage Energy (nJ): "<< leakage_energy_nj <<"\n";
  cout << "Dynamic Energy (nJ): "<< dynamic_energy_nj <<"\n";
  cout << "Ratio of Dyn/Leak: "<< dynamic_energy_nj/leakage_energy_nj <<"\n";
  cout << "Total Energy (nJ): "<< total_energy_nj <<"\n";
  cout << "Total Energy (nJ) 3D: "<< total_energy_nj_3d <<"\n";
  cout << "Cores Energy (nJ): "<< energy_cores_pj / 1000 <<"\n";
  cout << "Route Energy (nJ): "<< energy_route_pj_25d / 1000 <<"\n";
  cout << "Route Energy (nJ) 3D: "<< energy_route_pj_3d / 1000 <<"\n";
  cout << "Mem Energy   (nJ): "<< energy_mem_pj_25d / 1000 <<"\n";
  cout << "Mem Energy   (nJ) 3D: "<< energy_mem_pj_3d / 1000 <<"\n";
  std::cout << std::setprecision(2) << std::fixed;
  cout << "Avg. Power (mW): "<< avg_power_mW <<"\n";
  cout << "Power Density (mW/mm2): "<< avg_power_mW/area["TOT_SYSTEM_AREA"] <<"\n";

  cout << "\n---Runtime---\n";
  cout << "Number of K Nodes Processed: " <<  (totalFrontierNodesProcessed /1000) << "\n";
  cout << "Number of K Edges Processed: " <<  (totalEdgesProcessed /1000) << "\n";
  cout << "Runtime in K cycles: " <<  (wall_time/1000) << "\n";
  cout << "Miliseconds (at "<<freq<<" Ghz): "<< miliseconds<<"\n";

  if (last_print){  
    // Considering two memory operations per tile per cycle
    cout << "\n---Inst---\n";
    cout << "K Loads: " <<  (accum_counters[LOAD]/1000) << "\n";
    cout << "K Stores: " <<  (accum_counters[STORE]/1000) << "\n";
    cout << "K Instructions: " <<  (total_ops/1000) << "\n";
    cout << "K Messages To Task2: " <<  (accum_counters[MSG_1]/1000) << "\n";
    cout << "K Messages To Task3: " <<  (accum_counters[MSG_2]/1000) << "\n";
    cout << "K Messages To Task3b: " <<  (accum_counters[MSG_3]/1000) << "\n";
  }
  
  cout << "\n---Throughput---\n";
  cout << "GOP/s (aka OPS/ns): "<< total_ops/nanoseconds<<"\n";
  // Bytes loaded/stored per cycle
  u_int64_t mem_ops = accum_counters[LOAD]+accum_counters[STORE];
  cout << "Avg. BW GB/s: "<< mem_ops*ld_st_op_byte_width/nanoseconds<<"\n";
  u_int64_t theoretical_max_mem_ops = GRID_SIZE*wall_time*2; // 2 mem ops per cycle per tile
  cout << "Max BW GB/s: "<< (theoretical_max_mem_ops*ld_st_op_byte_width)/nanoseconds<<"\n";
  printf("Node Re-explore factor: %.2f\n",(double)(totalFrontierNodesProcessed)/graph->nodes);
  printf("Edge Re-explore factor: %.2f\n",(double)(totalEdgesProcessed)/graph->edges);
  cout << "Nodes/cy: "<< (double)totalFrontierNodesProcessed/wall_time<<"\n";
  cout << "Edges/cy: "<< (double)totalEdgesProcessed/wall_time<<"\n";
  cout << "TEPS: "<< graph->edges_traversed/wall_time<<"\n";

  cout << "\n---Traffic---\n";
  double inter_board_per_cy = (double)total_inter_board_traffic / wall_time;
  double inter_die_per_cy = (double)total_inter_die_traffic / wall_time;
  double inter_pack_per_cy = (double)total_inter_pack_traffic / wall_time;
  double ruche_messages_per_cy = (double)total_ruche_traffic / wall_time;
  double all_messages_per_cy = (double)total_messages / wall_time;
  
  cout << "Inter-board traffic (msg/cy): "<< inter_board_per_cy<< ", per-link: "<<inter_board_per_cy/(double)(all_boards_border_links)<<"\n";
  cout << "Inter-pack traffic (msg/cy): "<< inter_pack_per_cy<< ", per-link: "<<inter_pack_per_cy/(double)(all_packs_border_links)<<"\n";
  cout << "Inter-die traffic (msg/cy): "<< inter_die_per_cy<< ", per-link: "<<inter_die_per_cy/(double)(all_dies_border_links)<<"\n";
  cout << "Ruche traffic (msg/cy): "<< ruche_messages_per_cy<< ", per-link: "<<ruche_messages_per_cy/(double)(all_dies_border_links)<<"\n";
  cout << "All traffic (msg/cy): "<< all_messages_per_cy << ", per-link: "<<all_messages_per_cy/GRID_SIZE<<"\n";
  cout << "Inter-die reduction wrt all: "<< all_messages_per_cy/inter_die_per_cy << "\n";
  cout << "Inter-board reduction wrt inter-die: "<< inter_die_per_cy/inter_board_per_cy << "\n";

  if (last_print){
    for (u_int32_t i=0; i<GRID_X;i++){
      for (u_int32_t j=0; j<GRID_Y;j++){
        delete routers[i][j];delete total_counters[i][j]; delete frame_counters[i][j];
      }
      delete routers[i];delete total_counters[i]; delete frame_counters[i];
    }

    for (u_int32_t i=0; i<PRINT_X;i++){
      for (u_int32_t j=0; j<PRINT_Y;j++){
        delete print_counters[i][j];
      }
      delete print_counters[i];
    }

    delete routers;delete total_counters; delete frame_counters; delete print_counters;
    cout << "\nVersion 17\n\n";
  }
}