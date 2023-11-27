#include "param_energy.h"
#include "param_cost.h"

// ==== PARAMETERS ====
// Physical threads per tile
u_int32_t smt_per_tile = 1;

// Latency of one hop inside the die
u_int32_t on_die_hop_latency = 1; // in cycles of noc clk
const u_int32_t d2d_hop_latency_ns = 2; // in ns
const u_int32_t noc_width = 32;

// Hops occur already at the edge of the die or board, so this latency (in ns) only depends on a constant distance
// 2ns for PHI to cross the die, and 0.5 ns per mm of wire, 1 cycle on each side of the die to get a fresh clock
const u_int32_t die_hop_latency = d2d_hop_latency_ns*noc_freq + 2; 
const u_int32_t ruche_hop_latency = die_hop_latency;

// PU pipeline stalls from reading the SRAM (after pipelining)
#if SRAM_SIZE > 524288
    u_int32_t sram_read_latency = ceil_macro(3*pu_mem_ratio);
#elif SRAM_SIZE > 131072
    u_int32_t sram_read_latency = ceil_macro(2*pu_mem_ratio);
#else
    u_int32_t sram_read_latency = ceil_macro(1*pu_mem_ratio);
#endif
// SMT may increase latency and SRAM size due to banking... area of Muxes

const u_int32_t average_die_to_edge_distance = hops_to_mc(DIE_W);
// Latency inside the L2 cache from the point of view of the PU
u_int32_t l2_cache_latency = ceil_macro(8*pu_mem_ratio);
// Derived parameters, default values (can be overwridden by the user inside calculate_derived_param)
// Average latency (from the view of the PU) to get to the L2 cache from a tile within a die. L2 assumed at the edge of the die
u_int32_t l2_read_latency = l2_cache_latency + ceil_macro(noc_to_pu_ratio*average_die_to_edge_distance*on_die_hop_latency);

// Latency inside the HBM from the point of view of the PU
u_int32_t hbm_device_latency = ceil_macro(30*pu_mem_ratio);
u_int32_t hbm_read_latency = hbm_device_latency + ceil_macro(noc_to_pu_ratio*average_die_to_edge_distance*on_die_hop_latency);

u_int32_t barrier_penalty = GRID_X*3;

// ==== CHIPLET MODEL ====
// Parameters from https://www.opencompute.org/events/past-events/hipchips-chiplet-workshop-isca-conference
// Interposer can have 20-50um bumps
const u_int32_t d2d_interposer_areal_density_gbits_mm2 = 1070;
const u_int32_t d2d_interposer_beachfront_density_gbits_mm = 1780;
// Substrate has 130um bumps (thus, less density) Multi-Chip-Module (MCM)
const u_int32_t d2d_substrate_areal_density_gbits_mm2 = 690;
const u_int32_t d2d_substrate_beachfront_density_gbits_mm = 880;
// Set silicon interposer by default!
u_int32_t d2d_beachfront_density_gbits_mm = d2d_interposer_beachfront_density_gbits_mm;

const u_int32_t inter_board_distance_mm = 54; // hipchips slides
const u_int32_t inter_board_latency_ns = 20; // transmitter + receiver + interposer + substrate
const u_int32_t inter_pack_latency_ns = inter_board_latency_ns;


// ==== HBM PARAMETERS ====
// We don't change the frequency of HBM. So these are not freq or voltage adjusted
u_int32_t ld_st_op_byte_width = 4;
u_int32_t ld_st_op_bit_width = ld_st_op_byte_width * 8;
u_int32_t dcache_words_in_line_log2 = 4; //16 words, i.e., 64B
u_int32_t dcache_words_in_line = 1 << dcache_words_in_line_log2;
u_int32_t hbm_channels = 8;
u_int32_t total_hbm_channels = hbm_channels * DIES;
// hbm_channel same width as the dcache
u_int32_t hbm_channel_bit_width = dcache_words_in_line * ld_st_op_bit_width;
u_int32_t hbm_gbits = hbm_channels * hbm_channel_bit_width;
u_int32_t mc_fixed_area_mm2 = 6; // + add variable area based on the bw required
double hbm_mc_area_mm2 = mc_fixed_area_mm2 + hbm_gbits/d2d_interposer_areal_density_gbits_mm2;


// ==== AREA MODEL ====
// Parameters at 1GHz, adjust for frequency
const double per_physical_thread_area_mm2 = pu_area(0.043);
double per_core_logic_mm2   = per_physical_thread_area_mm2 * smt_per_tile;
const double sram_density_mm2_mib = mem_area(0.287); //287kum2 per MiB

// Size of the code section of the program within the SRAM
u_int32_t code_size_in_words = 150;

// RUCHE NETWORKS
// Left number is the relation between networks and mesh, and right number the calculated size in kum2
// for different configs (obtained from Ruche paper from Batten)
// Mesh	    1.00	2.76
// M Ruche2	2.52	6.95
// M Ruche3	3.00	8.27 (M Ruche2 * 1.19), use this value increase for each Ruche Dist
// Torus	  1.50	4.14
// T Ruche2	3.78	10.42
// T Ruche3	4.50	12.40 (T Ruche2 * 1.19)
const double no_torus_no_ruche_router_area_mm2  = noc_area(0.00276);
const double no_torus_ruche_router_area_mm2     = noc_area(0.00695);
const double torus_no_ruche_router_area_mm2     = noc_area(0.00414);
const double torus_ruche_router_area_mm2        = noc_area(0.01042);


#if APP<ALTERNATIVE
    #include "config_system_default.h"
#else
    #include "config_system_alternative.h"
#endif

// SAMPLE TIME: how often (in num PU cycles) to report logs on verbose mode
#if (GRID_X_LOG2 < 3) // < 8x8
    u_int32_t powers_sample_time = 7;
#elif (GRID_X_LOG2 < 4) // < 16x16
    u_int32_t powers_sample_time = 6;
#elif (GRID_X_LOG2 < 6) // < 64x64
    u_int32_t powers_sample_time = 5;
#elif (GRID_X_LOG2 < 8) // < 256x256
    u_int32_t powers_sample_time = 4;
#elif (GRID_X_LOG2 < 10) // < 1024x1024
    u_int32_t powers_sample_time = 3;
#elif (GRID_X_LOG2 < 12) // < 4096x4096
    u_int32_t powers_sample_time = 2;
#endif
u_int32_t sample_time = 1000;


void calculate_derived_param(){
    config_system();
    dcache_words_in_line = 1 << dcache_words_in_line_log2;
    hbm_channel_bit_width = dcache_words_in_line * ld_st_op_bit_width;
    hbm_gbits = hbm_channels * hbm_channel_bit_width;
    hbm_mc_area_mm2 = mc_fixed_area_mm2 + hbm_gbits/d2d_interposer_areal_density_gbits_mm2;
    total_hbm_channels = hbm_channels * DIES;
    sample_time = std::pow(10, powers_sample_time);
}