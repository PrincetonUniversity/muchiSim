#include "yield.cpp"

void cost_calculation(){
  // ==== COST CALCULATIONS ====
  cout << std::setprecision(2) << std::fixed;
  cout << "\nESTIMATED COST OF SILICON AND PACKAGING. ONLY VARIABLE, NO FIXED COSTS\n";

  double die_side_mm = vars["DIE_SIDE_MM"];
  // Yield of compute die. 3D is larger to accomodate the through-silicon vias (TSVs)
  int good_dies_per_wafer_3d = dieYieldData(die_side_mm, die_side_mm+5);
  int good_dies_per_wafer_2d = dieYieldData(die_side_mm, die_side_mm);
  cout << "Good dies per wafer 3d: " << good_dies_per_wafer_3d << endl;
  cout << "Good dies per wafer 2d: " << good_dies_per_wafer_2d << endl;

  // Die cost for 3D integration
  double single_processor_active_interposer_cost = (cost_per_7nm_wafer/good_dies_per_wafer_3d);
  double processor_active_interposer_cost = single_processor_active_interposer_cost * vars["DIES_PER_PACK"];
  cout << "--Dies 3D Cost: " << processor_active_interposer_cost << " $" << endl;

  // Die cost for 2D integration
  double single_processor_die_cost = (cost_per_7nm_wafer/good_dies_per_wafer_2d);
  double processor_die_cost = single_processor_die_cost * vars["DIES_PER_PACK"];
  cout << "--Dies 2.5D Cost: " << processor_die_cost << " $" << endl;
  cout << "--Dies 2D Cost: " << processor_die_cost << " $" << endl;

  double hbm_cost = hbm_cost_per_die * vars["DIES_PER_PACK"];

  // Bonding and packaging cost 3D
  double hbm_bonding_cost_3d = processor_die_cost * bonding_overhead_2d;
  double substrate_and_processor_bonding_cost_3d = processor_active_interposer_cost * (bonding_overhead_2d + substrate_overhead);
  double packaging_cost_3d =  hbm_bonding_cost_3d + substrate_and_processor_bonding_cost_3d;

  // Bonding and packaging cost 2.5D. Bonding first both to the interposer, and then the interposer to the substrate
  double interposer_and_hbm_bonding_cost = processor_die_cost * passive_interposer_and_hbm_bonding_overhead;
  // Now the passive interposer has twice the area, so we need to double the base cost
  double substrate_and_interposer_bonding_cost = (processor_die_cost*2) * (bonding_overhead_2d + substrate_overhead);
  double packaging_cost_25d = interposer_and_hbm_bonding_cost + substrate_and_interposer_bonding_cost;

  // 2D Cost. Bonding directly to the substrate
  double packaging_cost_2d = processor_die_cost * (bonding_overhead_2d + substrate_overhead);
  
  // IO die cost
  double good_dies_per_wafer_io = dieYieldData(IOdie_width, IOdie_height);
  double single_IOdie_cost = (cost_per_7nm_wafer/good_dies_per_wafer_io);
  double IOdie_cost = single_IOdie_cost * 2*vars["DIES_PACK_FACTOR"];
  double IOdie_packaging_cost = IOdie_cost * (bonding_overhead_2d + substrate_overhead);
  packaging_cost_3d += IOdie_packaging_cost;
  packaging_cost_25d += IOdie_packaging_cost;
  packaging_cost_2d += IOdie_packaging_cost;

  // Whole package cost
  double pkg_cost_3d  = processor_active_interposer_cost + hbm_cost + IOdie_cost + packaging_cost_3d;
  double pkg_cost_25d = processor_die_cost + hbm_cost + IOdie_cost + packaging_cost_25d;
  double pkg_cost_2d  = processor_die_cost + IOdie_cost + packaging_cost_2d;

  cout << "--HBM Cost: " << hbm_cost << " $" << endl;
  cout << "--IOdie Cost: " << IOdie_cost << " $" << endl;
  cout << "--Packaging Cost 3d: " << packaging_cost_3d << " $" << endl;
  cout << "--Packaging Cost 2.5d: " << packaging_cost_25d << " $" << endl;
  cout << "--Packaging Cost 2d: " << packaging_cost_2d << " $" << endl;
  cout << "SiP Cost 3d: " << pkg_cost_3d << " $" << endl;
  cout << "SiP Cost 2.5d: " << pkg_cost_25d << " $" << endl;
  cout << "SiP Cost 2d: " << pkg_cost_2d << " $" << endl;

  // Simplistic model that the PCB cost is proportional to the area, 1000 times less than silicon
  double board_area_mm2 = vars["TOT_SYSTEM_AREA"] / vars["BOARDS"];
  double board_cost = board_area_mm2 / 1000;
  cout << "--Board Cost: " << board_cost << " $" << endl;
  cout << "Total SiPs Cost 3d: " << pkg_cost_3d * vars["PACKAGES"] + board_cost << " $" << endl;
  cout << "Total SiPs Cost 2.5d: " << pkg_cost_25d * vars["PACKAGES"] + board_cost << " $" << endl;
  cout << "Total SiPs Cost 2d: " << pkg_cost_2d * vars["PACKAGES"] + board_cost << " $" << endl;
  cout << endl << std::flush;
}