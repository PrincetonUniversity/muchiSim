
// ==== COST PARAMETERS ====
const double cost_per_7nm_wafer = 6047.0;

// IO DIE
const u_int32_t IOdie_width =   4 * noc_area(noc_freq);
const u_int32_t IOdie_height = 10 * noc_area(noc_freq);

// HBM DIE
const u_int32_t hbm_die_h = 11;
const u_int32_t hbm_die_w = 10;
const u_int32_t hbm_die_area_mm2 = hbm_die_h * hbm_die_w;
const u_int32_t hbm_die_capacity_mib = 8192;

const double hbm_usd_per_gib = 7.5;
const double hbm_cost_per_die = (hbm_die_capacity_mib/1024)*hbm_usd_per_gib;
// % overhead cost over the silicon cost
const double bonding_overhead_2d = 0.05;
const double substrate_overhead = 0.10;
const double passive_interposer_and_hbm_bonding_overhead = 0.20;

