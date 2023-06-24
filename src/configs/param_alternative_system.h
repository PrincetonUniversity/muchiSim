
void config_system(){
    on_die_hop_latency = 2;
    sram_read_latency = 3;
    
    // Modeling always going to the L2 for data
    u_int32_t average_hop_distance = DIE_H/2;
    u_int32_t l2_cache_latency = 8;
    l2_read_latency = l2_cache_latency + on_die_hop_latency*2; // *2 because of roundtrip
    hbm_read_latency = 128;
    
    smt_per_tile = 1; //16;
    d2d_beachfront_density_gbits_mm = d2d_interposer_beachfront_density_gbits_mm;
    dcache_words_in_line_log2 = 4; //2^3 words, i.e., 32B; 2^4 words, i.e., 64B
    hbm_channels = 1;
}