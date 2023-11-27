
void area_calculation(){
  // ==== Scratchpad storage ==== 
  u_int64_t total_storage_kibibytes = vars["PER_TILE_SRAM_KIB"]*GRID_SIZE;
  u_int64_t total_storage_mebibytes = total_storage_kibibytes>>10;

  cout << "Active SRAM Storage per tile: "<<vars["PER_TILE_ACTIVE_SRAM_KIB"]<<" KiB\n"; 
  cout << "Total SRAM Storage per tile: "<<vars["PER_TILE_SRAM_KIB"]<<" KiB\n"; 
  cout << "Agreggated SRAM Storage: "<< total_storage_mebibytes<<" MiB\n\n";

  #if DCACHE
    uint64_t active_dram_kib = dram_active_words>>8;
    cout << "Active DRAM Storage per tile: "<<(double)active_dram_kib / 1024<<" MiB\n"; 
    const u_int32_t hbm_capacity_per_tile_mib = hbm_die_capacity_mib / TILES_PER_DIE;
    cout << "Total DRAM Storage per tile: "<<hbm_capacity_per_tile_mib<<" MiB\n"; 
    cout << "Agreggated DRAM Storage: "<< hbm_die_capacity_mib*DIES / 1024 <<" GiB\n\n";
  #endif

  // Make mem area fixed per Tile
  double total_sram_area_mm2 = total_storage_mebibytes*sram_density_mm2_mib;
  double per_core_sram_area_mm2 = total_sram_area_mm2/GRID_SIZE;

  double total_core_area_mm2 = per_core_logic_mm2 * GRID_SIZE;
  vars["TOT_CORE_AREA"] = total_core_area_mm2;

  
  uint64_t die_links = 1;
  #if TORUS==0 // MESH
    #if RUCHE_X == 0
      double per_router_area_mm2 = no_torus_no_ruche_router_area_mm2;
    #elif RUCHE_X == DIE_W_HALF  // No Full Ruche
      double per_router_area_mm2 = (BORDER_TILES_PER_DIE * no_torus_ruche_router_area_mm2 + INNER_TILES_PER_DIE * no_torus_no_ruche_router_area_mm2)/TILES_PER_DIE;
    #else //Ruche2
      die_links = 2; //Twice the links with Full Ruche
      double per_router_area_mm2 = no_torus_ruche_router_area_mm2;
    #endif

  #else // TORUS
    // This Ruche area considers that every link has a ruche channel
    #if RUCHE_X == 0
      die_links = 2; //Twice the links with Torus and no Ruche
      double per_router_area_mm2 = torus_no_ruche_router_area_mm2;
    #elif RUCHE_X == DIE_W_HALF  // No Full Ruche, only skip links between dies
      die_links = 2; //Twice the links with Torus and no Ruche
      double per_router_area_mm2 = (BORDER_TILES_PER_DIE * torus_ruche_router_area_mm2 + INNER_TILES_PER_DIE * torus_no_ruche_router_area_mm2)/TILES_PER_DIE;
    #else //Ruche2
      die_links = 4; //Four times the links with Torus and Ruche
      double per_router_area_mm2 = torus_ruche_router_area_mm2;
      #if RUCHE_X>=3
        per_router_area_mm2 = per_router_area_mm2*pow(1.19,RUCHE_X-2);
      #endif
      // Area model is likely less acurate for Full-Ruche > 3
      assert(RUCHE_X<=3);
    #endif
  #endif


  // Multiply by the number of NoCs (a wide NoC counts as an entra NoC)
  #if NOC_CONF==0
    u_int16_t num_32bit_nocs_inter_die = 1;
    u_int16_t num_32bit_nocs_intra_die = PHY_NOCS;
  #elif NOC_CONF==1
    u_int16_t num_32bit_nocs_inter_die = 1;
    u_int16_t num_32bit_nocs_intra_die = PHY_NOCS + 1; // One of the NoCs is wider, assume twice the silicon area
  #elif NOC_CONF==2
    u_int16_t num_32bit_nocs_inter_die = PHY_NOCS;
    u_int16_t num_32bit_nocs_intra_die = PHY_NOCS + 1;
  #elif NOC_CONF==3 // One NoC is wider inter-die too
    u_int16_t num_32bit_nocs_inter_die = PHY_NOCS + 1;
    u_int16_t num_32bit_nocs_intra_die = PHY_NOCS + 1;
  #else
    cout << "ERROR: NOC_CONF not defined\n" << flush; exit(1);
  #endif

  per_router_area_mm2 *= num_32bit_nocs_intra_die;

  die_links *= num_32bit_nocs_inter_die;
  die_links += 2; //I/O link on the border of the die
  die_links *= BORDER_TILES_PER_DIE; // Multiplied by the tiles on the edge of the die
  die_links *= 2; //Links are bi-directional

  double tile_area_mm2 = per_router_area_mm2 + per_core_logic_mm2 + per_core_sram_area_mm2;
  vars["PER_TILE_AREA"] = tile_area_mm2;

  double tile_len_mm = sqrt(tile_area_mm2);
  vars["PER_TILE_LEN"] = tile_len_mm;

  double die_area_mm2 = tile_area_mm2 * TILES_PER_DIE;

  // Die wires contribute to the area of the PHI.
  // Adjusted for frequency. Bits/cy * cy/ns = bits/ns, i.e, Gbits/s
  double die_gbits = die_links * noc_width * noc_freq;
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
  double IOdie_area_mm2 = IOdie_width * IOdie_height;
  double pkg_IOdie_area_mm2 = 2*DIES_PACK_FACTOR*IOdie_area_mm2;

  double pack_area_mm2 = pkg_processor_dies_area_mm2 + pkg_hbm_dies_area_mm2 + pkg_IOdie_area_mm2; 
  double pack_side_mm = sqrt(pack_area_mm2);

  double board_area_mm2 = pack_area_mm2 * PACKAGES_PER_BOARD;
  double board_side_mm = sqrt(board_area_mm2);

  double die_side_mm = sqrt(die_area_mm2);
  double die_border_wire_density = die_gbits / (4*die_side_mm);
  if(die_border_wire_density > d2d_beachfront_density_gbits_mm){
    cout << "WARNING: Die border wire density > Beachfront density\n";
    cout << "Die border wire density: " << die_border_wire_density << "\n";
    cout << "Beachfront density: " << d2d_beachfront_density_gbits_mm << "\n";
    cout << "Die Gbits: " << die_gbits << "\n";
    cout << "Die side mm: " << die_side_mm << "\n"<<flush;
  }

  cout << "Inter-Die #links: "<<die_links<<"\n";
  cout << "Inter-Die (Gbits): "<<(u_int32_t)die_gbits<<"\n";
  cout << "Inter-Die (Gbits/mm): "<<die_border_wire_density<<"\n";
  cout << "Bisection BW of Board (Gbits): " << (die_gbits/4)*DIES_BOARD_FACTOR << "\n";
  cout << "Border IO #Dies: "<<vars["IO_DIES"]<<"\n";
  cout << "Border IO Die BW (Gbits): "<<vars["DIE_IO_GBITS"]<<"\n";
  cout << "Off-Board BW Total (Gbits): "<<vars["IO_GBITS"]<<"\n";
  cout << "Off-Board BW Density (Gbits/mm): "<< (u_int32_t)(vars["IO_GBITS"]/(board_side_mm*4))<<"\n";


  double hbm_wire_density = hbm_gbits/die_side_mm;
  assert(hbm_wire_density < d2d_interposer_beachfront_density_gbits_mm);

  std::cout << std::setprecision(2) << std::fixed;
  cout << "Die side: " << die_side_mm << " mm" << endl;
  cout << "Die border wire density: " << die_border_wire_density << " Gbits/mm" << endl;
  cout << "HBM wire density: " << hbm_wire_density << " Gbits/mm\n" << endl;
  vars["DIE_SIDE_MM"] = die_side_mm;

  double total_router_area_mm2 = per_router_area_mm2*GRID_SIZE + per_die_phy_area_mm2*DIES + active_mc_area_mm2*DIES + PACKAGES*pkg_IOdie_area_mm2;
  vars["TOT_ROUTER_AREA"] = total_router_area_mm2;
  
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
  vars["TOT_SYSTEM_AREA"] = total_area_used_mm2_a;
}