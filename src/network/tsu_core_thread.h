void update_timer(u_int32_t global_cid, int delay, u_int64_t compute_cycles){
  if (delay>sample_time) sample_time*=2;
  u_int32_t base = global_cid*smt_per_tile;
  core_timer[base]+=delay;
  assert(compute_cycles<delay);

  // Add compute cycles to all core_timers
  for (int i=1; i<smt_per_tile; i++){
    core_timer[base+i]+=compute_cycles;
  }

  //Calculate min from all core_timer[global_cid]
  u_int64_t min = core_timer[base];
  int min_index = 0;
  for (int i=1; i<smt_per_tile; i++){
    if (core_timer[base+i]<min){
      min = core_timer[base+i];
      min_index = i;
    }
  }
  // Swap min with the first element
  core_timer[base+min_index] = core_timer[base];
  core_timer[base] = min;
}

void count_delay(u_int32_t * counter, u_int32_t global_cid, int task_id, int delay, u_int64_t compute_cycles){
  #if QUEUE_PRIO==3
    delay=1;
  #endif
  counter[T1+task_id]++;
  if (task_id>2) task_id--;
  counter[TASK1+task_id]+=delay;
  update_timer(global_cid, delay, compute_cycles);
}

void count_msg_latency(int tX,int tY, u_int32_t qid, u_int64_t timer){
  R_queue<Msg> * queue = routers[tX][tY]->output_q[C];
  Msg msg = queue[qid+1].peek_next();
  u_int64_t time = msg.time;
  frame_counters[tX][tY][MSG_1_LAT+qid] += (timer - time);
}

int calculate_frontier_occ(u_int32_t global_cid){
  // Calculate occupancy of frontier
  if (frontier_list==NULL) return 0;
  u_int32_t frontier_list_base = global_cid * frontier_list_len;
  int tail = frontier_list[frontier_list_base + fr_tail_offset];
  int head = frontier_list[frontier_list_base + fr_head_offset];
  return (head < tail) ? fr_head_offset - (tail - head) : head - tail;
}

struct task_entry
{
  bool invokable;
  bool runnable;
  bool input_not_almost_full;
  bool output_pri;
  bool iqueue_active;
  bool oqueue_active;
  bool local = false;
  int (*task)(int, int, u_int64_t, u_int64_t &);
};

void tsu_core_thread(u_int32_t wtid){
  // A thread is assigned to advance a number of columns, and each column has 'GRID_Y' cores
  u_int32_t cores_samples = 0;

  u_int32_t * last_sample = (u_int32_t *) calloc(CORES_PER_TH, sizeof(u_int32_t));
  u_int32_t * epoch_begin = (u_int32_t *) calloc(CORES_PER_TH, sizeof(u_int32_t));
  
  struct task_entry tasks[NUM_TASKS];
  tasks[0].task = task1_kernel; tasks[0].local = true;
  tasks[1].task = task2_kernel;
  tasks[2].task = task3_kernel;
  tasks[3].task = task3bis_kernel;
  tasks[4].task = task4_kernel; tasks[4].local = true;

  u_int64_t cumm_frame_counters[GLOBAL_COUNTERS];
  for (u_int32_t cid = 0; cid < CORES_PER_TH; cid++) last_sample[cid] = 0;

  bool current_epoch_ongoing = true;
  while (current_epoch_ongoing){
    cout << "TASK INIT epoch: " << epoch_counter << endl << flush;
    for (u_int32_t cid = 0; cid < CORES_PER_TH; cid++){
      u_int32_t x = wtid*COLUMNS_PER_TH + cid%COLUMNS_PER_TH;
      u_int32_t y = cid/COLUMNS_PER_TH;
      task_init(x,y);
    }

    while(global_router_active){

      for (u_int32_t cid = 0; cid < CORES_PER_TH; cid++){
        u_int32_t x = wtid*COLUMNS_PER_TH + cid%COLUMNS_PER_TH;
        u_int32_t y = cid/COLUMNS_PER_TH;
        u_int32_t global_cid = global(x,y);

        // Check what's the localtimer of the core after executing the previous task
        u_int64_t timer = core_timer[global_cid*smt_per_tile];
        if (timer <= router_timer[wtid]){

          u_int32_t current_sample = (timer / sample_time);
          //SAMPLING EVERY TIMESTEP, cores can only enter here once regardless of Thread count
          if (current_sample > last_sample[cid]){
            last_sample[cid]=current_sample;
            // Cycles spanned since last sample
            span_timer[global_cid] = timer - prev_timer[global_cid];
            // Update Previous timestamp
            prev_timer[global_cid] = timer;

            // Update Print and Frame Counters
            if ((x < PRINT_X) && (y < PRINT_Y)){
              for (int c = 0; c < GLOBAL_COUNTERS; c++){
                print_counters[x][y][c] = frame_counters[x][y][c];
              }
            }
            for (int c = 0; c < GLOBAL_COUNTERS; c++){
              total_counters[x][y][c] += frame_counters[x][y][c];
              frame_counters[x][y][c] = 0;
            }

            // PRINT STATS AT EVERY SAMPLE (only thread 0 of the simulator)
            #if PRINT>=1
              if (wtid==0){
                cores_samples++;
                // When we have explored all the cores, we can print the stats
                if (cores_samples == CORES_PER_TH){
                  cores_samples = 0;

                  // Counting how many cores are active (within the ones this threads is responsible for)
                  int count_active = 0;
                  for (u_int32_t j=0; j<GRID_Y; j++){
                    for (u_int32_t i=wtid*COLUMNS_PER_TH; i<(wtid+1)*COLUMNS_PER_TH; i++){
                      if (isActive[global(i,j)]) count_active++;
                    }
                  }
                  bool is_cold = (count_active<=(CORES_PER_TH/2));

                  LK;
                    print_stats_frame();
                    print_stats_acum(false);

                    cout << "\n\n\n===Core Utilization Total=== CORE-S:"<<current_sample<<";; TIME K cy:"<<timer/1000<<" ;; Nodes Proc: "<< numFrontierNodesPushed[wtid] << " -- Edges Proc: "<< numEdgesProcessed[wtid] << "\n" << std::flush;

                    if (is_cold) cout << "======== IT IS COLD ======= Only "<< count_active <<" Tiles active\n";
                    else cool_down_time = get_final_time();
                  ULK;
                }
              }
            #endif
          } // End of Sample IF

          R_queue<Msg> * core_readq  = routers[x][y]->output_q[C];
          R_queue<Msg> * core_writeq = routers[x][y]->input_q[C];

          // Check that a task has enough input parameters to trigger (done in HW)
          u_int32_t router_collision = 0;
          for (int i=0; i<NUM_TASKS-1; i++){
            tasks[i].invokable = core_readq[i].occupancy() >= num_task_params[i];
            tasks[i].iqueue_active = core_readq[i].occupancy() > 0;
            tasks[i].oqueue_active = core_writeq[i].occupancy() > 0;
            router_collision += routers[x][y]->input_collision[i] + routers[x][y]->output_collision[i];
            tasks[i].input_not_almost_full = (core_readq[i].capacity() > (iq_sizes[i]/4) );
          }

          tasks[0].runnable   = tasks[0].invokable && (core_writeq[task1_dest].capacity() > 8);
          tasks[0].output_pri = tasks[0].invokable && core_writeq[task1_dest].is_empty();
          tasks[1].runnable   = tasks[1].invokable && (core_writeq[dest_qid].capacity() > max_task_chunk*num_task_params[dest_qid]+2);
          tasks[1].output_pri = tasks[1].invokable && core_writeq[dest_qid].is_empty();

          tasks[2].output_pri = false;
          tasks[3].output_pri = false;
          tasks[2].runnable = tasks[2].invokable;
          #if WRITE_THROUGH==1
            tasks[3].runnable = tasks[3].invokable && (core_writeq[2].capacity() > num_task_params[2]);
          #else
            #if CASCADE_WRITEBACK
              int dest_pcache_writeback = 3;
            #else
              int dest_pcache_writeback = 2;
            #endif
            tasks[3].runnable = tasks[3].invokable && (!proxys_cached || (core_writeq[dest_pcache_writeback].capacity() > num_task_params[2]) );
          #endif


          // Consider a tile as active if it can execute tasks
          bool iqueue123_active = false, oqueue123_active = false;
          for (int i=0; i<NUM_QUEUES; i++){
            iqueue123_active |= tasks[i].iqueue_active;
            oqueue123_active |= tasks[i].oqueue_active;
          }
          bool wants_to_flush_cache = !oqueue123_active && !iqueue123_active && (router_collision==0);

          bool frontier_active = calculate_frontier_occ(global_cid) > 0;

          #if GLOBAL_BARRIER>=1 // look at whether it's the beginning of the frontier
            frontier_active  = frontier_active && (epoch_begin[cid]>0);
          #else
            if (wants_to_flush_cache) epoch_begin[cid]++;
            else epoch_begin[cid] = 0;
          #endif
          isActive[global_cid] = oqueue123_active|| iqueue123_active || frontier_active || need_flush_pcache(global_cid);

          // T1 high priority if its OQ is empty, AND if the IQ of T2 is not full
          // Only prioritize task2 over task3 if task3 has enough space in its input queue
          // otherwise, give priority to task3 since we don't want noc2 to be blocked
          int i = 0;
          int next = -1;
          while (i < (NUM_TASKS-1) && (next < 0) ){
            bool others_not_runnable = true;
            bool others_input_not_almost_full = true;
            bool others_output_not_pri = true;
            for (int j=i+1; j<NUM_TASKS-1; j++){
              others_input_not_almost_full &= tasks[j].input_not_almost_full;
              others_not_runnable &= !tasks[j].runnable;
              others_output_not_pri &= !tasks[j].output_pri;
            }
            if (tasks[i].runnable && (others_input_not_almost_full && others_output_not_pri || others_not_runnable)) next = i;
            i++;
          }
          
          int delay; u_int64_t compute_cycles = 0;

          if (next>=0){
            if (!tasks[next].local) count_msg_latency(x,y,next-1, timer);
            delay = tasks[next].task(x,y,timer,compute_cycles);
            count_delay(frame_counters[x][y], global_cid, next, delay, compute_cycles);
          } 
          else if (need_flush_pcache(global_cid) && !oqueue123_active){
            // No task to execute, drain pcache (done in the background in hardware)
            drain_pcache(x,y, timer);
            update_timer(global_cid, 1, compute_cycles);
            // Wait for 16 totally idle cycles to start exploring the frontier
          }else if (frontier_active && epoch_begin[cid]>16){
            delay = tasks[4].task(x,y,timer,compute_cycles);
            count_delay(frame_counters[x][y], global_cid, 4, delay, compute_cycles);
            epoch_begin[cid] = 0;
          }else{
            int delay = 1;
            // Advance many cycles if the entire column is idle
            // REVISIT should make sure that the tile will not receive a task in the next 10 cycles
            if (!column_active[wtid]) delay = 10;

            frame_counters[x][y][IDLE]+=delay;
            // if the entire network is idle, see the diff between this tile and the most recent active tile
            update_timer(global_cid, delay, compute_cycles);
          }
        } // End If current_cycle
      }
    } // End of while global_router_active
    
    // Epoch is done!
    current_epoch_ongoing = false;

    // UPDATE LOCAL ACTIVITY STATUS
    bool any_cid_active = false;
    for (u_int32_t cid = 0; cid < CORES_PER_TH; cid++){
      u_int32_t x = wtid*COLUMNS_PER_TH + cid%COLUMNS_PER_TH;
      u_int32_t y = cid/COLUMNS_PER_TH;
      epoch_begin[cid] = UINT32_MAX;
      int frontier_size = calculate_frontier_occ(global(x,y));
      bool cid_active = (frontier_size > 0) || (epoch_counter < num_kernels);
      any_cid_active |= cid_active;
      // Need to set isActive for next epoch, because otherwise the router may quickly set global_router_active to 0
      isActive[cid] = cid_active;
    }
    epoch_has_work[wtid] = any_cid_active;

    { // BARRIER AMONG ALL THREADS
      std::unique_lock<std::mutex> lock2(all_threads_mutex2);
      waiting_threads2++;
      if (waiting_threads2 == MAX_THREADS) {waiting_threads2 = 0;all_threads_cv2.notify_all();}
      else {all_threads_cv2.wait(lock2, [&] { return waiting_threads2 == 0; });}
    }
    
    // ONCE WE HAVE SYNCHRONIZED, WE CAN CHECK THE FRONTIER OF EVERYONE ELSE
    for (u_int32_t t = 0; t < COLUMNS; t++){
      current_epoch_ongoing |= epoch_has_work[t];
    }
    if (wtid == 0){ // ONLY THREAD 0 WILL PRINT
        u_int64_t edgesEpoch=0,nodesEpoch=0;
        for (u_int32_t i=0; i<COLUMNS; i++){
          edgesEpoch += numEdgesProcessed[i];
          nodesEpoch += numFrontierNodesPushed[i];
        }
        print_lock.workerEpochDone(epoch_counter++, nodesEpoch, edgesEpoch, get_final_time());
    }
    {   // BARRIER AMONG ALL THREADS
        std::unique_lock<std::mutex> lock(all_threads_mutex);
        waiting_threads++;
        if (waiting_threads == (MAX_THREADS)) {waiting_threads = 0; global_router_active = 1; all_threads_cv.notify_all();}
        else {all_threads_cv.wait(lock, [&] { return waiting_threads == 0; });} 
    }

  } // End of while (current_epoch_ongoing)
  print_lock.workerProgramDone(wtid);

  for (u_int32_t cid = 0; cid < CORES_PER_TH; cid++){
    u_int32_t x = wtid*COLUMNS_PER_TH+cid%COLUMNS_PER_TH;
    u_int32_t y = cid/COLUMNS_PER_TH;
    for (int c = 0; c < GLOBAL_COUNTERS; c++) total_counters[x][y][c] += frame_counters[x][y][c];
  }
  free(last_sample); free(epoch_begin);
}
