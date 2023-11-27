
void config_system(){
    sram_read_latency = ceil_macro(3*pu_mem_ratio);
    smt_per_tile = 1; //16;
    per_core_logic_mm2   = per_physical_thread_area_mm2 * smt_per_tile;
    
    d2d_beachfront_density_gbits_mm = d2d_interposer_beachfront_density_gbits_mm;
    dcache_words_in_line_log2 = 4; //2^3 words, i.e., 32B; 2^4 words, i.e., 64B
    hbm_channels = 1;
}