void print_configuration(ostream& fout){
  //Print with format thousands separator
  fout.imbue(locale(""));

  if (nodePerTile>0){
    fout << "Node per tile: "<<nodePerTile << endl;
    fout << "Edge per tile: "<<edgePerTile << endl;
  }
  
  // Print Configuration Run
  fout << "GRID_X: "<<GRID_X << endl;
  fout << "GRID_SIZE: "<<GRID_X*GRID_Y << endl;
  fout << "smt_per_tile: "<<smt_per_tile << endl;
  fout << "noc_freq: "<<noc_freq << endl;
  fout << "pu_freq: "<<pu_freq << endl;

  fout << "RUCHE_X: "<<RUCHE_X << endl;
  fout << "PROXY_W: "<<PROXY_W << endl;

  // Dies
  fout << "DIE_W: "<<DIE_W << endl;
  fout << "DIE_H: "<<DIE_H << endl;
  fout << "DIES: "<<DIES << endl;
  fout << "BORDER_TILES_PER_DIE: "<<BORDER_TILES_PER_DIE << endl;

  // Packages
  fout << "PACK_W: "<<PACK_W << endl;
  fout << "PACK_H: "<<PACK_H << endl;
  fout << "DIES_PACK_FACTOR: "<<DIES_PACK_FACTOR << endl;
  fout << "DIES_PER_PACK: "<<DIES_PER_PACK << endl;
  fout << "PACKAGES: "<<PACKAGES << endl;

  // Board
  fout << "BOARD_W: "<<BOARD_W << endl;
  fout << "BOARD_H: "<<BOARD_H << endl;
  fout << "BOARDS: "<<BOARDS << endl;
  fout << "MUX_BUS: "<<MUX_BUS << endl;

  // Caches
  fout << "DCACHE: "<<DCACHE << endl;
  fout << "PCACHE: "<<PCACHE << endl;
  fout << "PROXY_FACTOR: "<<PROXY_FACTOR << endl;
  fout << "CACHE_WAYS: "<<CACHE_WAYS << endl;

  // Network
  fout << "TORUS: "<< TORUS << endl;
  fout << "PHY_NOCS: "<< PHY_NOCS << endl;
  fout << "WIDE_NOC_ID: "<< WIDE_NOC_ID << endl;
  fout << "NOC_CONF: "<< NOC_CONF << endl;
  fout << "noc_width: "<< noc_width << endl;
  fout << "hbm_channel_bit_width: "<< hbm_channel_bit_width << endl;
  fout << "ld_st_op_byte_width: "<< ld_st_op_byte_width << endl;

  fout << "PROXY_ROUTING: "<< PROXY_ROUTING << endl;
  fout << "WRITE_THROUGH: "<< WRITE_THROUGH << endl;
  fout << "ASSERT_MODE: "<<ASSERT_MODE << endl;
  fout << "GLOBAL_BARRIER: "<<GLOBAL_BARRIER << endl;
  if (&fout == &cout) {
    cout << "OQ flits: "<<oq_sizes[1]<<" ; "<<oq_sizes[2]<<" ; "<<oq_sizes[3] << endl;
    cout << "IQ flits: "<<iq_sizes[1]<<" ; "<<iq_sizes[2]<<" ; "<<iq_sizes[3] << endl;
    cout << "OQ messages: "<<oq_sizes[1]/num_task_params[1]<<" ; "<<oq_sizes[2]/num_task_params[2]<<" ; "<<oq_sizes[3]/num_task_params[3] << endl;
    cout << "IQ messages: "<<iq_sizes[1]/num_task_params[1]<<" ; "<<iq_sizes[2]/num_task_params[2]<<" ; "<<iq_sizes[3]/num_task_params[3] << endl;
    cout << "max_task2 size: "<<max_task_chunk << endl;
    cout << "io_factor_t2: "<<io_factor_t2 << endl;
    cout << "io_factor_t3: "<<io_factor_t3 << endl;
    cout << "io_factor_t3b: "<<io_factor_t3b << endl;
  }
  fout << endl << flush;

  // Needed from the energy model
  vars["GRID_SIZE"] = GRID_SIZE;
  vars["GRID_X"] = GRID_X;
  vars["DIES_PER_PACK"] = DIES_PER_PACK;
  vars["DIES_PACK_FACTOR"] = DIES_PACK_FACTOR;
  vars["BORDER_TILES_PER_DIE"] = BORDER_TILES_PER_DIE;
  vars["DIE_W"] = DIE_W;
  vars["DIE_H"] = DIE_H;
  vars["DIES"] = DIES;
  vars["PACK_W"] = PACK_W;
  vars["PACK_H"] = PACK_H;
  vars["PACKAGES"] = PACKAGES;
  vars["BOARD_W"] = BOARD_W;
  vars["BOARD_H"] = BOARD_H;
  vars["BOARDS"] = BOARDS;
  vars["MUX_BUS"] = MUX_BUS;

  vars["TORUS"] = TORUS;
  vars["PHY_NOCS"] = PHY_NOCS;
  vars["NOC_CONF"] = NOC_CONF;
  vars["RUCHE_X"] = RUCHE_X;
  vars["CACHE_WAYS"] = CACHE_WAYS;

  vars["smt_per_tile"] = smt_per_tile;
  vars["noc_width"] = noc_width;
  vars["hbm_channel_bit_width"] = hbm_channel_bit_width;
  vars["ld_st_op_byte_width"] = ld_st_op_byte_width;
}


#define STATS_COUNTERS 8
const string stats_names[STATS_COUNTERS] = {"Collision In", "Collision Out", "Collision End","Task1","Task2","Task3", "Core Active", "Router Active"};
const u_int64_t stats_id[STATS_COUNTERS] = {MSG_IN_COLLISION, MSG_OUT_COLLISION, MSG_END_COLLISION, TASK1, TASK2, TASK3, IDLE, ROUTER_ACTIVE};


void print_counter_stats(bool last_print, u_int64_t * cumm_counters, u_int64_t *** counters, int len_x, int len_y){
  bool frame = (counters == print_counters);
  string pre;
  // A stands for "Accum" and F for "Frame"
  if (frame) pre = "F";
  else pre = "A";

  // Arrays to store the stats
  double stats_count[STATS_COUNTERS*PRINT_SIZE];
  double stats_max[STATS_COUNTERS];
  double stats_min[STATS_COUNTERS];
  double stats_avg[STATS_COUNTERS];
  for (int i=0;i<STATS_COUNTERS;i++){
    stats_max[i] = 0;
    stats_min[i] = numeric_limits<double>::max();
    stats_avg[i] = 0;
  }

  double counters_f[GLOBAL_COUNTERS];
  for (int c = 0; c <GLOBAL_COUNTERS; c++) cumm_counters[c] = 0;

  for (u_int32_t i=0; i<len_x; i++){
    for (u_int32_t j=0; j<len_y; j++){

      u_int64_t pu_cycles;
      // To distinguish between frame and accum counting
      if (frame) pu_cycles = span_timer[global(i,j)];
      else if (last_print){
        pu_cycles = get_final_time();
      }
      else pu_cycles = prev_timer[global(i,j)];
      

      double pu_cycles_div100 = pu_cycles/100.0;
      double noc_cycles_div100 = pu_to_noc_cy(pu_cycles_div100);
      for (int c = 0; c <GLOBAL_COUNTERS; c++){
        u_int64_t counter;
        // Calculate active core cycles as total cycles minus idle cycles
        if (c==IDLE) counter = pu_cycles - (counters[i][j][c] / smt_per_tile);
        else counter = counters[i][j][c];
        if (c==ROUTER_ACTIVE || c==MSG_IN_COLLISION || c==MSG_END_COLLISION || c==MSG_OUT_COLLISION) counters_f[c] = (double)counter/noc_cycles_div100;
        else counters_f[c] = (double)counter/pu_cycles_div100;
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
          if (last_print) cout << "X:"<<i<<"\t Y:"<<j<<"\tR: "<<counters_f[ROUTER_ACTIVE]<<"\%\t  C: "<< counters_f[IDLE]<< "%\t --- Tasks: "<< counters_f[TASK1]<<"\t| "<<counters_f[TASK2]<<"\t| "<<counters_f[TASK3]<<"\t| "<<counters_f[TASK4]<<"\t| LD:"<<counters_f[LOAD]<<"\t|ST: "<<counters_f[STORE] << endl;
        #endif
      }
    }
  }
  //  ==== PRINT MATRICES OF COUNTERS ====
  cout << setprecision(0) << fixed;
  for (uint32_t s = 0; s < STATS_COUNTERS; s++)
    #if PRINT<2
    if (false && last_print)
    #endif
    {
      cout << "\n"<< pre << "M" << stats_names[s] << "\n";
      printMatrixf(&stats_count[s*PRINT_SIZE]);
    }
  cout << "\n" << flush;

  //  ==== PRINT MAIN STATISTICS OF THESE COUNTERS ====
  for (int s = 0; s < STATS_COUNTERS; s++){
    stats_avg[s] = stats_avg[s]/PRINT_SIZE;
    double sd = calculateSD(PRINT_SIZE, stats_avg[s], &stats_count[s*PRINT_SIZE]);
    cout << pre << stats_names[s] << " Avg:"<<stats_avg[s]<<", SD:"<<sd<<", Max:"<< stats_max[s]<<", Min:"<< stats_min[s] << endl;
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
  cout << "Avg. latency NOC1 (T2): "<<(double)cumm_counters[MSG_1_LAT]/t2_invocations << endl;
  #if PROXY_FACTOR==1
    cout << "Avg. hops NOC2: " <<  (double)cumm_counters[MSG_2]/T3_params_loaded << "\n";
    // cout << "Avg. hops NOC3: " <<  (double)cumm_counters[MSG_3]/T3b_params_loaded << "\n";
    cout << "Avg. latency NOC2 (T3): "<<(double)cumm_counters[MSG_2_LAT]/t3_invocations << endl;
    // cout << "Avg. latency NOC3 (T3b): "<<(double)cumm_counters[MSG_3_LAT]/t3b_invocations << endl;
  #else
    double total_t3_params = T3_params_loaded + T3b_params_loaded;
    double total_t3_msg = cumm_counters[MSG_2] + cumm_counters[MSG_3];
    cout << "Avg. hops NOC2: " << total_t3_msg/total_t3_params<<"\n";
    // cout << "Avg. hops NOC3: " << total_t3_msg/total_t3_params<<"\n";
    double total_t3_latency = cumm_counters[MSG_2_LAT] + cumm_counters[MSG_3_LAT];
    cout << "Avg. latency NOC2 (T3): "<< total_t3_latency/both_t3_invocations << endl;
  #endif
  return;
}


void print_stats_frame(){
  u_int64_t this_counters[GLOBAL_COUNTERS];
  cout << "\n===PRINTING FRAME STATS\n";
  print_counter_stats(false, &this_counters[0], print_counters, PRINT_X, PRINT_Y);
}


#include "calc_energy.h"
#include "calc_perf.h"


void print_stats_acum(bool last_print, double sim_time){
  uint64_t wall_pu_cycles = get_final_time();
  //Total pu_cycles and pu_cycles before the end of the cool down phase, when less than SQRT(cores) are active
  if (cool_down_time==0) cool_down_time=wall_pu_cycles;
  double wall_pu_cycles_div100 = wall_pu_cycles/100;

  u_int64_t accum_counters[GLOBAL_COUNTERS];
  cout << "\n===PRINTING ACUM STATS at pu_cycles: " << wall_pu_cycles << "\n";
  print_counter_stats(last_print, &accum_counters[0], total_counters, GRID_X, GRID_Y);

  ofstream count_out;
  if (last_print){
    cout << "\nRuntime at program end in K cycles: " <<  (wall_pu_cycles/1000) << "\n";
    cout << "Runtime before cooldown in K cycles: " <<  (cool_down_time/1000) << "\n\n\n";
  }
  
  double nanoseconds = wall_pu_cycles / pu_freq;

  if (graph != NULL){
    u_int64_t totalEdgesProcessed=0;
    u_int64_t totalFrontierNodesProcessed = 0;
    for (int i=0; i<COLUMNS; i++){
      totalEdgesProcessed += numEdgesProcessed[i];
      totalFrontierNodesProcessed += numFrontierNodesPushed[i];
    }
    if (totalEdgesProcessed == 0) totalEdgesProcessed = totalFrontierNodesProcessed;
    if (totalFrontierNodesProcessed == 0) totalFrontierNodesProcessed = totalEdgesProcessed;
    
    cout << "Number of K Nodes Processed: " <<  (totalFrontierNodesProcessed /1000) << "\n";
    cout << "Number of K Edges Processed: " <<  (totalEdgesProcessed /1000) << "\n";
    printf("Node Re-explore factor: %.2f\n",(double)(totalFrontierNodesProcessed)/graph->nodes);
    printf("Edge Re-explore factor: %.2f\n",(double)(totalEdgesProcessed)/graph->edges);
    cout << "Nodes/cy: "<< (double)totalFrontierNodesProcessed/wall_pu_cycles << endl;
    cout << "Edges/cy: "<< (double)totalEdgesProcessed/wall_pu_cycles << endl;
    cout << "TEPS: "<< graph->edges_traversed/nanoseconds << endl;
  }

  #if PCACHE
  cout << "\n---Pcache---\n";
  cout << "Pcache Size: "<<pcache_size << endl;
  cout << "Cached: "<<proxys_cached << endl;
  cout << "  PHits: "<<pcache_hits << endl;
  cout << "  PMisses: "<<pcache_misses << endl;
  uint64_t total_requests_pcache = pcache_hits + pcache_misses;
  cout << "  PHit Rate (hit/miss): "<<(double)pcache_hits*100/total_requests_pcache<<"%\n";
  cout << "  PEvictions: "<<pcache_evictions << endl;
  cout << "  PEviction Rate (evict/misses): "<<(double)pcache_evictions*100/pcache_misses<<"%\n";
  #endif

  u_int64_t total_mc_transactions=0;
  #if DCACHE
  cout << "\n---DCache---\n";
  cout << "DCache Size: "<<dcache_size << endl;
  cout << "DRAM words: "<<dram_active_words << endl;
  cout << "  DHits: "<<dcache_hits << endl;
  cout << "  DMisses: "<<dcache_misses << endl;
  if (dram_active_words){
    u_int64_t total_writes=0, max_trans = 0;
    double avg_latency=0;
    int max_mc = 0;
    for (int i=0;i<total_hbm_channels;i++){
      u_int64_t transactions = mc_transactions[i];
      total_mc_transactions += transactions;
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
    cout << "  DEvictions: "<<dcache_evictions << endl;
    cout << "  DEviction Rate (evict/misses): "<<(double)dcache_evictions*100/dcache_misses<<"%\n";
    cout << "  MC Transactions: "<<total_mc_transactions << endl;
    cout << "  MC Writes: "<<total_writes << endl;
    cout << "Avg. Transactions: " << total_mc_transactions/total_hbm_channels/wall_pu_cycles_div100 <<"% , Writes: "<< total_writes/total_hbm_channels/wall_pu_cycles_div100<< "%" <<", Avg. Latency: "<<avg_latency/total_hbm_channels << endl;
    cout << "Max MC id: "<< max_mc <<" of "<< total_hbm_channels <<". Transactions: " << max_trans/wall_pu_cycles_div100 <<"% , Writes: "<< mc_writebacks[max_mc]/wall_pu_cycles_div100<< "%" <<", MC mean Latency: "<<(double)mc_latency[max_mc]/max_trans << endl;
  }
  #endif
  
  // Total number of hops in the network
  u_int64_t total_inter_board_traffic = 0;
  u_int64_t total_inter_die_traffic = 0;
  u_int64_t total_ruche_traffic = 0;
  u_int64_t total_inter_pack_traffic = 0;
  for (int i=0; i<BOARDS; i++) {total_inter_board_traffic += inter_board_traffic[i];}
  for (int i=0; i<PACKAGES; i++) {total_inter_pack_traffic += inter_pack_traffic[i];}
  for (int i=0; i<DIES; i++){total_inter_die_traffic += inter_die_traffic[i]; total_ruche_traffic += ruche_traffic[i];}

  // Task2 params are read from ChannelQ to router and then written again into the TQ
  // Task3 params are read from ChannelQ to router and then written again into the TQ
  // Task1 and Task4 write directly into the TQ, so that energy is already accounted
  const u_int64_t all_loaded_params = accum_counters[T2]*num_task_params[1] + accum_counters[T3]*num_task_params[2] + accum_counters[T3b]*num_task_params[3];
  accum_counters[LOAD] += all_loaded_params;
  accum_counters[STORE]+= all_loaded_params;

  u_int64_t total_waits = accum_counters[MEM_WAIT];
  u_int64_t total_ops = accum_counters[TASK1]+accum_counters[TASK2]+accum_counters[TASK3]+accum_counters[TASK4];
  total_ops -= total_waits;
  const u_int64_t total_flops = accum_counters[FLOPS];
  const u_int64_t total_intops = total_ops - total_flops;
  const u_int64_t total_loads = accum_counters[LOAD];
  const u_int64_t total_stores = accum_counters[STORE];
  u_int64_t total_noc_messages = accum_counters[MSG_1]+accum_counters[MSG_2]+accum_counters[MSG_3];

  if (last_print){
    // Remove last character of dataset_filename
    string sub = dataset_filename.substr(0, dataset_filename.find_last_of("/"));
    sub = sub.substr(sub.find_last_of("/")+1);
    string filename = "sim_counters/COUNT-"+ sub + "--" + to_string(GRID_X) + "-X-" + to_string(GRID_Y) +"--B" + binary_filename + "-A" + to_string(APP) + ".log";
    count_out.open(filename);
    if (!count_out) {cerr << "Unable to open file " << filename << endl; exit(1);}

    print_configuration(count_out);
    //cout << setprecision(0) << fixed;
    count_out << "total_noc_messages: "<<total_noc_messages << endl;
    count_out << "total_inter_board_traffic: "<<total_inter_board_traffic << endl;
    count_out << "total_inter_die_traffic: "<<total_inter_die_traffic << endl;
    count_out << "total_inter_pack_traffic: "<<total_inter_pack_traffic << endl;
    count_out << "total_ruche_traffic: "<<total_ruche_traffic << endl;
    count_out << "all_loaded_params: "<<all_loaded_params << endl;

    count_out << "total_loads: " << total_loads << endl;
    count_out << "total_stores: " << total_stores << endl;
    count_out << "total_flops: " << total_flops << endl;
    count_out << "total_ops: " << total_ops << endl;
    count_out << "total_waits: " << total_waits << endl;

    count_out << "DIE_SIDE_MM: " << vars["DIE_SIDE_MM"] << endl;
    count_out << "PER_TILE_SRAM_KIB: " << vars["PER_TILE_SRAM_KIB"] << endl;
    count_out << "PER_TILE_AREA: "<<vars["PER_TILE_AREA"] << endl;
    count_out << "PER_TILE_ACTIVE_SRAM_KIB: " << vars["PER_TILE_ACTIVE_SRAM_KIB"] << endl;
    count_out << "PER_TILE_LEN: " << vars["PER_TILE_LEN"] << endl;
    count_out << "TOT_ROUTER_AREA: " << vars["TOT_ROUTER_AREA"] << endl;
    count_out << "TOT_CORE_AREA: " << vars["TOT_CORE_AREA"] << endl;
    count_out << "TOT_SYSTEM_AREA: " << vars["TOT_SYSTEM_AREA"] << endl;
    count_out << "nanoseconds: " << nanoseconds << endl;
    count_out << "dcache_hits: " << dcache_hits << endl;
    count_out << "dcache_misses: " << dcache_misses << endl;
    count_out << "dcache_evictions: " << dcache_evictions << endl;
    count_out << "total_mc_transactions: " << total_mc_transactions << endl;
    count_out << "dram_active_words: " << dram_active_words << endl;
    count_out << "pcache_hits: " << pcache_hits << endl;
    count_out << "pcache_misses: " << pcache_misses << endl;
    count_out << "sim_time: " << sim_time << endl;

    count_out.close();
  }
  
  print_energy(total_noc_messages, total_inter_board_traffic, total_inter_die_traffic, total_inter_pack_traffic, total_ruche_traffic, total_ops, total_flops, total_loads, total_stores, all_loaded_params, pcache_hits, pcache_misses, dcache_hits, dcache_misses, total_mc_transactions, dram_active_words, nanoseconds, cout);
  //
  print_perf(last_print, total_mc_transactions, total_noc_messages, total_inter_board_traffic, total_inter_die_traffic, total_inter_pack_traffic, total_ruche_traffic, total_ops, total_flops, total_loads, total_stores, sim_time, nanoseconds);

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
  }
}