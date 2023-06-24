u_int32_t sample_time = 1000;
const u_int32_t code_size_in_words = 150;

// ==== TESSERACT CONSTANTS ====
const u_int32_t interrupt_overhead = 50;

// ==== DIE CONSTANTS ====
u_int32_t on_die_hop_latency = 1;
const u_int32_t d2d_hop_latency_ns = 2;
// Max length is 5mm
const double d2d_interposer_hop_energy_pj_per_bit = 0.500;
// Max length is 25mm
const double d2d_substrate_hop_energy_pj_per_bit = 0.550;
const double in_die_wire_pj_bit_mm = 0.150;
u_int64_t energy_per_routing_flit = 3; //pJ energy of the routing itself (simular to ALU), no wire energy

// Hops occur already at the edge of the die or board, so this latency (in ns) only depends on a constant distance
// 2ns for PHI to cross the die, and 0.5 ns per mm of wire, 1 cycle on each side of the die to get a fresh clock
const u_int32_t die_hop_latency = d2d_hop_latency_ns + 2; 
const u_int32_t ruche_hop_latency = die_hop_latency;


// Interposer can have 20-50um bumps
const u_int32_t d2d_interposer_areal_density_gbits_mm2 = 1070;
const u_int32_t d2d_interposer_beachfront_density_gbits_mm = 1780;
// Substrate has 130um bumps (thus, less density)
const u_int32_t d2d_substrate_areal_density_gbits_mm2 = 690;
const u_int32_t d2d_substrate_beachfront_density_gbits_mm = 880;
u_int32_t d2d_beachfront_density_gbits_mm = d2d_substrate_beachfront_density_gbits_mm;


// ==== HBM CONSTANTS ====
u_int32_t ld_st_op_byte_width = 4;
u_int32_t ld_st_op_bit_width = ld_st_op_byte_width * 8;
u_int32_t dcache_words_in_line_log2 = 4; //16 words, i.e., 64B
u_int32_t dcache_words_in_line = 1 << dcache_words_in_line_log2;
u_int32_t hbm_channels = 8;
u_int32_t total_hbm_channels = hbm_channels * DIES;
u_int32_t hbm_channel_bit_width = dcache_words_in_line * ld_st_op_bit_width;
u_int32_t hbm_gbits = hbm_channels * hbm_channel_bit_width;
double hbm_mc_area_mm2 = 6 + hbm_gbits/d2d_interposer_areal_density_gbits_mm2;

const u_int32_t hbm_die_h = 11;
const u_int32_t hbm_die_w = 10;
const u_int32_t hbm_die_area_mm2 = hbm_die_h * hbm_die_w;
int hbm_read_latency = 50;
// Regresh time uses as double, so that the calculation is done in double precision
const double hbm_refresh_time_ns = 32000000;
const double hbm_read_energy_pj_bit = 3.7; //3.7 pJ/bit for HBM read data, including IO (fine_grain paper)


const double hbm_activation_energy_pj_bit = 0.011;
const double hbm_precharge_energy_pj_bit = 0.011;
const u_int32_t hbm_die_capacity_mib = 8192;
const u_int32_t hbm_capacity_per_tile_mib = hbm_die_capacity_mib / TILES_PER_DIE;
const double hbm_usd_per_gib = 7.5;
const double hbm_cost_per_die = (hbm_die_capacity_mib/1024)*hbm_usd_per_gib;

// ==== SRAM CONSTANTS ====
const double sram_density_mm2_mib = 0.287;
const double sram_read_word_energy = 5.8;
const double sram_write_word_energy = 9.1;

// ==== BOARD CONSTANTS ====
const u_int32_t inter_board_distance_mm = 54; //nvidia
const u_int32_t inter_board_latency_ns = 20; // transmitter + receiver + interposer + substrate
const u_int32_t inter_pack_latency_ns = inter_board_latency_ns;
const double inter_board_pJ_bit = 1.17; //nvidia
const double inter_pack_pJ_bit = inter_board_pJ_bit;

const u_int32_t inter_board_gbits = 12800; // nvidia: from the bottom of the board
const u_int32_t all_boards_border_links = BOARDS * ((BOARD_H*2 + BOARD_W*2) / MUX_BUS);
const u_int32_t all_packs_border_links = PACKAGES * ((PACK_H*2 + PACK_W*2));
const u_int32_t all_dies_border_links = DIES * BORDER_TILES_PER_DIE;

// ==== RUCHE CONSTANTS ====
// Left number is the relation between networks and mesh, and right number the calculated size in kum2
// for different configs (obtained from Ruche paper from Batten)
// Mesh	    1.00	2.76
// M Ruche2	2.52	6.95
// M Ruche3	3.00	8.27 (M Ruche2 * 1.19), use this value increase for each Ruche Dist
// Torus	  1.50	4.14
// T Ruche2	3.78	10.42
// T Ruche3	4.50	12.40 (T Ruche2 * 1.19)
const double no_torus_no_ruche_router_area_mm2 = 0.00276;
const double no_torus_ruche_router_area_mm2 = 0.00695;
const double torus_no_ruche_router_area_mm2 = 0.00414;
const double torus_ruche_router_area_mm2 = 0.01042;

// ==== NETWORK CONSTANTS ====
const u_int32_t IOdie_area_mm2 = 40; // 4mm x 10mm
const u_int32_t noc_width = 32;

// ==== SYSTEM PARAMETERS ====
u_int32_t sram_read_latency = 1;
u_int32_t l2_read_latency = 25;
u_int32_t smt_per_tile = 1;
u_int32_t access_miss_latency = 0;

#if APP<6
    #include "param_tascade_system.h"
#else
    #include "param_alternative_system.h"
#endif

void calculate_derived_param(){
    config_system();
    dcache_words_in_line = 1 << dcache_words_in_line_log2;
    hbm_channel_bit_width = dcache_words_in_line * ld_st_op_bit_width;
    hbm_gbits = hbm_channels * hbm_channel_bit_width;
    hbm_mc_area_mm2 = 6 + hbm_gbits/d2d_interposer_areal_density_gbits_mm2;
    total_hbm_channels = hbm_channels * DIES;
}