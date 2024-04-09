int print_processor_info() {    
    FILE *file;
    char buffer[1024];

    file = fopen("/proc/cpuinfo", "r");
    if (file == NULL) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    while (fgets(buffer, sizeof(buffer), file)) {
        printf("%s", buffer);
    }

    fclose(file);
    return EXIT_SUCCESS;
}


void init_perf_counters() {
  mc_transactions = (u_int64_t *) calloc(total_hbm_channels, sizeof(u_int64_t));
  mc_writebacks = (u_int64_t *) calloc(total_hbm_channels, sizeof(u_int64_t));
  mc_latency = (u_int64_t *) calloc(total_hbm_channels, sizeof(u_int64_t));

  for (int i=0;i<BOARDS;i++){
    inter_board_traffic[i] = 0;
  }
  for (int i=0;i<DIES;i++){
    inter_die_traffic[i] = 0;
  }
  for (int i=0;i<PACKAGES;i++){
    inter_pack_traffic[i] = 0;
  }

  total_counters = new u_int64_t**[GRID_X];
  frame_counters = new u_int32_t**[GRID_X];
  for (int i=0; i < GRID_X; i++){
    total_counters[i] = new u_int64_t*[GRID_Y];
    frame_counters[i] = new u_int32_t*[GRID_Y];
    for (int j=0; j < GRID_Y; j++){
      total_counters[i][j] = new u_int64_t[GLOBAL_COUNTERS];
      frame_counters[i][j] = new u_int32_t[GLOBAL_COUNTERS];
      for (int c = 0; c < GLOBAL_COUNTERS; c++){
        total_counters[i][j][c] = 0;
        frame_counters[i][j][c] = 0;
      }
    }
  }
  print_counters = new u_int64_t**[PRINT_X];
  for (u_int32_t i=0; i<PRINT_X;i++){
    print_counters[i] = new u_int64_t*[PRINT_Y];
    for (u_int32_t j=0; j<PRINT_Y;j++){
      print_counters[i][j] = new u_int64_t[GLOBAL_COUNTERS];
    }
  }
}



double calculateSD(int total, double mean, double * data) {
  double sd = 0.0;
  int i;
  for (i = 0; i < total; ++i) {
      sd += pow(data[i] - mean, 2);
  }
  return sqrt(sd / total);
}


void printMatrixf(double * data){
  for (uint32_t j=0; j<PRINT_Y; j++){
    for (uint32_t i=0; i<PRINT_X; i++){
      uint32_t idx = j*PRINT_X + i;
      cout << ((uint32_t) data[idx]) << "\t";
    }
   cout << "\n" << flush;
  }
}

u_int64_t get_final_time(){
  //Get max value from core_timer array
  u_int64_t max = 0;
  for (int i=0; i<GRID_SIZE*smt_per_tile; i++){
    if (core_timer[i] > max) max = core_timer[i];
  }
  return max;
}