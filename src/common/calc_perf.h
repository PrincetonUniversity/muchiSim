
void print_perf(bool last_print, u_int64_t total_mc_transactions, u_int64_t total_noc_messages, u_int64_t total_inter_board_traffic, u_int64_t total_inter_die_traffic, u_int64_t total_inter_pack_traffic, u_int64_t total_ruche_traffic, u_int64_t total_ops, u_int64_t total_flops, u_int64_t total_loads, u_int64_t total_stores, double sim_time, double nanoseconds){
  double wall_pu_cycles = nanoseconds * pu_freq; 
  double wall_mem_cycles = wall_pu_cycles/pu_mem_ratio;
  double wall_noc_cycles = pu_to_noc_cy(wall_pu_cycles);
  double microseconds = nanoseconds/1000;
  double miliseconds = nanoseconds/1000000;

  cout << "\n---Runtime---\n";
  cout << "Runtime in K cycles: " <<  (wall_pu_cycles/1000) << "\n";
  cout << "Microseconds: "<< microseconds << endl;
  cout << "Time per iteration (ms): "<< miliseconds << endl;

  if (last_print){  
    // Considering two memory operations per tile per cycle
    cout << "\n---Inst---\n";
    cout << "K Loads: " <<  (total_loads/1000) << "\n";
    cout << "K Stores: " <<  (total_stores/1000) << "\n";
    cout << "K Instructions: " <<  (total_ops/1000) << "\n";
  }

  const u_int32_t total_tiles = vars["GRID_SIZE"];
  const u_int32_t physical_threads = total_tiles * vars["smt_per_tile"];
  const u_int32_t total_dies = vars["DIES"];
  const u_int32_t ruche_x = vars["RUCHE_X"];

  cout << "\n---Throughput---\n";
  const double gops = total_ops/nanoseconds;
  cout << "GOP/s (aka OPS/ns): "<< gops  << endl;
  cout << "GOP/s per core: "<< gops/total_tiles << endl;
  const double flops = total_flops/nanoseconds;
  // This means Giga FLOP/s, not flop/s
  cout << "FLOP/s: "<< flops << endl;
  cout << "FLOP/s per core: "<< flops/total_tiles << endl;
  cout << "Max Peak FLOP/s: "<< (physical_threads * wall_pu_cycles) / nanoseconds << endl; // 1 flop per physical thread per PU cycle
  // Bytes loaded/stored per cycle
  u_int64_t mem_ops = total_loads + total_stores;
  cout << "Avg. BW GB/s: "<< (mem_ops * vars["ld_st_op_byte_width"]) / nanoseconds << endl;
  u_int64_t theoretical_max_mem_ops = physical_threads * wall_mem_cycles; // 1 load per thread per mem cycle
  cout << "Max BW GB/s: "<< (theoretical_max_mem_ops * vars["ld_st_op_byte_width"]) / nanoseconds << endl;

  u_int64_t total_messages = total_noc_messages + (total_mc_transactions * hops_to_mc(vars["DIE_W"]));
  cout << "\n---Traffic---\n";
  double inter_board_per_ns = (double)total_inter_board_traffic / nanoseconds;
  double inter_die_per_ns = (double)total_inter_die_traffic / nanoseconds;
  double inter_pack_per_ns = (double)total_inter_pack_traffic / nanoseconds;
  double ruche_messages_per_ns = (double)total_ruche_traffic / nanoseconds;
  double all_messages_per_ns = (double)total_messages / nanoseconds;
  
  int single_noc_byte_width = (vars["noc_width"]/8);
  int link_bytes = vars["PHY_NOCS"] * single_noc_byte_width;
  if (vars["NOC_CONF"] > 0) link_bytes += single_noc_byte_width; // If one NOC is twice as wide intra-die

  double bisection_bytes = vars["DIE_W"] * link_bytes;
  if (vars["TORUS"] == 1) bisection_bytes *= 2;
  if (ruche_x == 2 || ruche_x == 3) bisection_bytes *= ruche_x;
  cout << "In-Die bisection_bytes BW (GB/s): "<< bisection_bytes * wall_noc_cycles / nanoseconds << endl;

  const u_int32_t all_boards_border_links = vars["BOARDS"] * ((vars["BOARD_H"]*2 + vars["BOARD_W"]*2) / vars["MUX_BUS"]);
  const u_int32_t all_packs_border_links = vars["PACKAGES"] * ((vars["PACK_H"]*2 + vars["PACK_W"]*2));
  const u_int32_t all_dies_border_links = total_dies * vars["BORDER_TILES_PER_DIE"];
  cout << "Inter-board traffic (msg/ns): "<< inter_board_per_ns << ", per-link: "<<inter_board_per_ns / (double)(all_boards_border_links) << endl;
  cout << "Inter-pack traffic (msg/ns): "<< inter_pack_per_ns << ", per-link: "<<inter_pack_per_ns / (double)(all_packs_border_links) << endl;
  cout << "Inter-die traffic (msg/ns): "<< inter_die_per_ns << ", per-link: "<<inter_die_per_ns / (double)(all_dies_border_links) << endl;
  cout << "Ruche traffic (msg/ns): "<< ruche_messages_per_ns << ", per-link: "<<ruche_messages_per_ns / (double)(all_dies_border_links) << endl;
  cout << "All traffic (msg/ns): "<< all_messages_per_ns << ", per-link: "<<all_messages_per_ns / total_tiles << endl;
  cout << "Inter-die reduction wrt all: "<< all_messages_per_ns / inter_die_per_ns << "\n";
  cout << "Inter-board reduction wrt inter-die: "<< inter_die_per_ns / inter_board_per_ns << "\n";

  cout << "\n---Data Movement---\n";
  cout << "All traffic (msg): "<< total_messages << "\n";
  double total_flops_d = total_flops;
  cout << "Arith. Intensity FLOP/Loads: "<< total_flops_d/total_loads << "\n";
  cout << "Arith. Intensity FLOP/Msgs: "<< total_flops_d/total_messages << "\n";
  double total_ops_d = total_ops;
  cout << "Arith. Intensity OPS/Loads: "<< total_ops_d/total_loads << "\n";
  cout << "Arith. Intensity OPS/Msgs: "<< total_ops_d/total_messages << "\n";

  if (last_print){
    cout << "\nSimulation time: " << sim_time << "\n";
    cout << "Sim noc_cy/s: " << wall_noc_cycles / sim_time << "\n";
    cout << "Sim Msg/s: " << total_messages / sim_time << "\n";
  }
}