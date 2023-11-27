// Reference values
// (Ariane=1mW leakage at CMOS22nm, 0.5mm2, 500kum2) ->  200nW per kum2 -> 0.2 mW per mm2
// Leon2 Sparc, Leakage power is 66.000nW for a circuit of 35.000 Gates with FinFet transistors of 7nm, at vdd=0.45V
const double P_leak_ref = 66.000e-9;  // Leakage power in watts
const double trans_count_ref = 35000*5;  // Number of transistors (assuming each gate has 5 transistor)
const double leak_vdd_ref = 0.45;  // Supply voltage in volts
const double width_ref = 7;  // Transistor width in nanometers 

// intensity_single_trans = (voltage * k * transistor_size)
// total_leakage_power = intensity_single_trans * voltage * num_transistors
const double K_calc = P_leak_ref / (trans_count_ref * leak_vdd_ref * leak_vdd_ref * width_ref); // units A/(V*nm)

double calculate_leakage_power(double num_transistors, double vdd, double transistor_width) {
    /*
        Calculate the leakage power of a circuit based on the number of transistors,
        supply voltage (Vdd), transistor width, and a constant (K).
        Parameters:
            num_transistors (double): Number of transistors in the circuit.
            vdd (double): Supply voltage in volts.
            transistor_width (double): Width of the transistors in micrometers.
        Returns:
            double: Leakage power in watts.
    */
    // K (double): Constant (determined empirically).
    // Transistor current depends on the transistor process and vdd
    double I_leak = K_calc * vdd * transistor_width;  // Leakage current per transistor
    // Total circuit leakage power is given by transistor current, voltage and number of transistors
    double P_leak = num_transistors * vdd * I_leak;  // Leakage power
    return P_leak;
}

void print_energy(u_int64_t total_noc_messages, u_int64_t total_inter_board_traffic, u_int64_t total_inter_die_traffic, u_int64_t total_inter_pack_traffic, u_int64_t total_ruche_traffic, u_int64_t total_ops, u_int64_t total_flops, u_int64_t total_loads, u_int64_t total_stores, u_int64_t all_loaded_params, u_int64_t l_pcache_hits, u_int64_t l_pcache_misses, u_int64_t l_dcache_hits, u_int64_t l_dcache_misses, u_int64_t total_mc_transactions, u_int64_t l_dram_active_words, double nanoseconds, ostream& fout)
{  
  u_int64_t all_non_contiguous_tile_traffic = total_ruche_traffic + total_inter_die_traffic + total_inter_pack_traffic + total_inter_board_traffic;

  // Energy of hop in the wire (pJ)
  u_int32_t die_w = vars["DIE_W"];
  u_int32_t ruche_x = vars["RUCHE_X"];
  double inter_tile_pJ_bit_25d = in_die_wire_pj_bit_mm * vars["PER_TILE_LEN"];
  double inter_tile_pJ_bit_3d = in_die_wire_pj_bit_mm * ((vars["PER_TILE_LEN"]*die_w+5)/die_w);
  if (vars["TORUS"] >= 1){
    inter_tile_pJ_bit_25d *= 2; // Twice the distance in a torus
    inter_tile_pJ_bit_3d *= 2; // Twice the distance in a torus
  }

  // Number of hops that only cross one tile (the other longer hops are counted separately)
  u_int64_t tile_hop_messages = total_noc_messages - all_loaded_params - all_non_contiguous_tile_traffic;


  if (ruche_x == (die_w/2)){
    // In the case we use ruche to connect one tile per die
    total_inter_die_traffic += total_ruche_traffic;
  }else{
    // Normal ruche case, the jump is RUCHE_X times the regular hop length
    tile_hop_messages += total_ruche_traffic * ruche_x;
  }

  u_int32_t hbm_channel_bit_w = vars["hbm_channel_bit_width"];
  u_int32_t noc_w = vars["noc_width"];
  u_int64_t dynamic_routers_25d = tile_hop_messages * noc_w * inter_tile_pJ_bit_25d;
  u_int64_t dynamic_routers_3d  = tile_hop_messages * noc_w * inter_tile_pJ_bit_3d;


  // Adding the energy of the inter-die and inter-board traffic
  if (vars["BOARD_W"] < vars["GRID_X"]){
    dynamic_routers_25d += total_inter_board_traffic * noc_w * inter_board_pJ_bit;
    dynamic_routers_3d  += total_inter_board_traffic * noc_w * inter_board_pJ_bit;
  }

  if (total_mc_transactions > 0){
    // If we don't have HBM we use an organic substrate to connect compute dies
    dynamic_routers_25d += total_inter_die_traffic * noc_w * d2d_substrate_hop_energy_pj_per_bit;
  } else {
    dynamic_routers_25d += total_inter_die_traffic * noc_w * d2d_interposer_hop_energy_pj_per_bit;
  }
  dynamic_routers_3d += total_inter_die_traffic * noc_w * d2d_interposer_hop_energy_pj_per_bit;

  if (vars["PACK_W"] < vars["BOARD_W"]){
    dynamic_routers_25d += total_inter_pack_traffic * noc_w * inter_pack_pJ_bit;
    dynamic_routers_3d  += total_inter_pack_traffic * noc_w * inter_pack_pJ_bit;
  }


  double miliseconds = nanoseconds/1000000;
  u_int64_t dynamic_pu = 0;
  u_int64_t dynamic_mem = 0;
  // Separate compute and mem energy
  // Add IQ param loads, Queue pop/push

  const u_int64_t total_intops = total_ops - total_flops;
  dynamic_pu += total_intops * pj_per_intop_ref;
  dynamic_pu += total_flops * pj_per_flop_ref;
  

  // In our model we keep the bank width constant and scaling the height and number of banks with SRAM size
  // Simple model for dynamic energy of muxes coming and going to SRAM banks: 3pJ per 32-bit word with 32KB SRAM block, growing at a rate of 50% per 2x SRAM size
  //double sram_bank_mux_tree_energy = 0.2694 * pow(1.3, log2(vars["PER_TILE_SRAM_KIB"]));
  double sram_bank_mux_tree_energy_at_32kib = 3.0;
  double sram_bank_mux_tree_energy_growth_rate = 0.1317 * pow(1.5, log2(vars["PER_TILE_SRAM_KIB"]));
  double sram_bank_mux_tree_energy = sram_bank_mux_tree_energy_at_32kib * sram_bank_mux_tree_energy_growth_rate;

  dynamic_mem += total_loads  * (sram_read_word_energy + sram_bank_mux_tree_energy);
  dynamic_mem += total_stores * (sram_write_word_energy + sram_bank_mux_tree_energy);
  
  // Multiply the number of words read by the energy per word
  double cache_lookup_pj = (sram_read_word_energy + sram_bank_mux_tree_energy) * vars["CACHE_WAYS"];
  
  dynamic_mem += l_pcache_hits * cache_lookup_pj;
  dynamic_mem += l_pcache_misses * cache_lookup_pj; //miss in proxy cache doesn't cause a DRAM access

  double mc_wire_energy_pj_25d = 0;
  double mc_wire_energy_pj_3d = 0;
  double dram_energy_pj_25d = 0;
  double dram_energy_pj_3d = 0;

  if (total_mc_transactions){
    // The cost of a lookup regardless whether it hits or not
    u_int64_t dcache_lookup_energy = (l_dcache_hits + l_dcache_misses) * cache_lookup_pj;

    // On average, the data and the header go through half the die on average
    double avg_distance_mm = hops_to_mc(die_w) * vars["PER_TILE_LEN"];
    double mc_wire_energy_pj_bit = in_die_wire_pj_bit_mm * avg_distance_mm;
    double mc_req_bit_width = hbm_channel_bit_w + noc_w;
    mc_wire_energy_pj_25d = total_mc_transactions * mc_req_bit_width * mc_wire_energy_pj_bit;
    mc_wire_energy_pj_3d = mc_wire_energy_pj_25d/4; // 4x less energy in 3D
    // DRAM energy of HBM reads
    u_int64_t hbm_access_energy_pj = total_mc_transactions * hbm_channel_bit_w * hbm_read_energy_pj_bit;
    
    // The background energy and refresh energy of the DRAM
    // Based on how long we run (walltime), we can estimate the number of refreshes
    u_int64_t hbm_refresh_periods = nanoseconds / hbm_refresh_time_ns;
    //double hbm_reads_during_refresh_periods = total_mc_transactions / hbm_refresh_periods;
    u_int64_t bits_refreshed = ((l_dram_active_words<<5) * nanoseconds) / hbm_refresh_time_ns;
    u_int64_t hbm_refresh_energy_pj = bits_refreshed * (hbm_activation_energy_pj_bit + hbm_precharge_energy_pj_bit);
    fout << "HBM refresh energy (nJ): "<<hbm_refresh_energy_pj/1000 << "\n";
    fout << "HBM access energy (nJ): "<<hbm_access_energy_pj/1000 << "\n";
    fout << "MemC wire energy 2.5d (nJ): "<<mc_wire_energy_pj_25d/1000 << "\n";
    fout << "MemC wire energy 3d (nJ): "<<mc_wire_energy_pj_3d/1000 << "\n";
    dram_energy_pj_25d = dcache_lookup_energy + mc_wire_energy_pj_25d + hbm_access_energy_pj + hbm_refresh_energy_pj;
    dram_energy_pj_3d = dcache_lookup_energy + mc_wire_energy_pj_3d + hbm_access_energy_pj + hbm_refresh_energy_pj;
  }

  double dynamic_mem_25d = dynamic_mem + dram_energy_pj_25d;
  double dynamic_mem_3d = dynamic_mem + dram_energy_pj_3d;

  u_int64_t dynamic_energy_pj_25d = dynamic_pu + dynamic_mem_25d + dynamic_routers_25d;
  u_int64_t dynamic_energy_pj_3d = dynamic_pu + dynamic_mem_3d + dynamic_routers_3d;


  // === Static power of silicon area === 
  double leakage_mem = vars["PER_TILE_ACTIVE_SRAM_KIB"] * vars["GRID_SIZE"] * sram_leakage_nw_per_kib;

  // https://en.wikichip.org/wiki/7_nm_lithography_process
  double transistors_at_7nm = 91.2e6;  // 91 million transistors per mm2 at 7nm
  double num_transistors = transistors_at_7nm * pow((7 / transistor_node), 2);
  // printf("Transistors per mm2 at process node: %.2e\n", num_transistors);
  double nw_leakage_pu_per_mm2 = calculate_leakage_power(num_transistors, pu_vdd, transistor_node) * 1e9;  // Convert to nW
  // printf("Leakage power PU: %.2f nW per mm2\n", nw_leakage_pu_per_mm2);
  double nw_leakage_noc_per_mm2 = calculate_leakage_power(num_transistors, noc_vdd, transistor_node) * 1e9;  // Convert to nW

  double leakage_route = nw_leakage_noc_per_mm2 * vars["TOT_ROUTER_AREA"];
  double leakage_cores = nw_leakage_pu_per_mm2 * vars["TOT_CORE_AREA"];
  double leakage_power_nw = leakage_mem + leakage_route + leakage_cores;
  double leakage_energy_pj = leakage_power_nw*miliseconds;

  // === Energy breakdown === 
  double energy_cores_pj = leakage_cores*miliseconds + dynamic_pu;

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
  

  fout << std::setprecision(0) << std::fixed;
  fout << "\n---Energy---\n";
  fout << "Leakage Energy (nJ): "<< leakage_energy_nj << endl;
  //
  fout << "PU Dyn E (nJ): "<< dynamic_pu/1000 << endl;
  fout << "Mem Dyn E (nJ): "<< dynamic_mem_25d/1000 << endl;
  fout << "Route Dyn E (nJ): "<< dynamic_routers_25d/1000 << endl;
  //
  fout << "Dynamic Energy (nJ): "<< dynamic_energy_nj << endl;
  fout << "Ratio of Dyn/Leak: "<< dynamic_energy_nj/leakage_energy_nj << endl;
  //
  fout << "Total Energy (nJ): "<< total_energy_nj << endl;
  fout << "Total Energy (nJ) 3D: "<< total_energy_nj_3d << endl;
  fout << "Cores Energy (nJ): "<< energy_cores_pj / 1000 << endl;
  fout << "Route Energy (nJ): "<< energy_route_pj_25d / 1000 << endl;
  fout << "Route Energy (nJ) 3D: "<< energy_route_pj_3d / 1000 << endl;
  fout << "Mem Energy   (nJ): "<< energy_mem_pj_25d / 1000 << endl;
  fout << "Mem Energy   (nJ) 3D: "<< energy_mem_pj_3d / 1000 << endl;
  fout << std::setprecision(2) << std::fixed;
  fout << "Avg. Power (mW): "<< avg_power_mW << endl;
  fout << "Power Density (mW/mm2): "<< avg_power_mW/vars["TOT_SYSTEM_AREA"] << endl;
}