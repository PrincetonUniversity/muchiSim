
void router_thread(u_int32_t rid){
  u_int32_t rid_next = rid+1;
  // Given the width of the MESH and the number of threads, figure out how many columns a thread is responsible for
  u_int32_t columns = GRID_X/COLUMNS;
  // A thread is assigned to advance a number of columns, and each column has 'GRID_Y' cores
  u_int32_t routers_per_thread = columns*GRID_Y;

  u_int64_t last_sample = 0;

  // Local variable to this single thread
  bool current_epoch_ongoing = true;

  while (current_epoch_ongoing){
    while (global_router_active){ //GLOBAL SYNC

      bool local_router_active = true;
      while (local_router_active && waiting_routers==0) {
        u_int64_t current_noc_cycle = router_timer[rid];

        #if PRINT>=3
          u_int64_t current_sample = (current_noc_cycle / sample_time);
          // SAMPLING AND PRINTING INSIDE HERE AT EACH SAMPLE
          if (current_sample > last_sample){
            last_sample++;
            LK;
            for (u_int32_t qid = 0; qid <NUM_QUEUES; qid++){
              #if PRINT>=4
                //Print Neighbor links
                for (u_int32_t c = 0; c <= 3; c++){
                  cout << "\nChannel "<< qid << getD(c) << "\n0:\t";
                  for (u_int32_t j=0; j<GRID_Y; j++){
                    for (u_int32_t i=rid*columns; i<rid_next*columns; i++){
                      RouterModel * r = routers[i][j];
                      uint16_t input_occ = ((float)r->input_q[c][qid].occupancy()*10.0/(rq_sizes[qid]) );
                      printf("%X,%c  ",input_occ, r->print_dest_route(c, qid, current_noc_cycle) );
                    }
                    printf("\n%d:\t",j+1);
                  }
                }
              #endif

              #if PRINT>=3
                //Print Core queues
                cout << "\n\nQueue OUT from the Core to the Router Ch["<< qid << "]\n0:\t";
                for (u_int32_t j=0; j<GRID_Y; j++){
                  for (u_int32_t i=rid*columns; i<rid_next*columns; i++){
                    RouterModel * r = routers[i][j];
                    printf("%X,%c  ",(uint16_t)((float)r->input_q[C][qid].occupancy()*10.0/(oq_sizes[qid])),r->print_dest_route(C, qid,current_noc_cycle) );
                  }
                  printf("\n%d:\t",j+1);
                }
                
                //All Direction inputs towards the Router Output:
                #if BOARD_W < GRID_X
                  cout << "\n\nInputs (N/S/W/E/(PY/PX)/C) of Router at Ch["<< qid <<  "] wants to go: \n0:\t";
                #else
                  cout << "\n\nInputs (N/S/W/E/C) of Router at Ch["<< qid <<  "] wants to go: \n0:\t";
                #endif
                for (u_int32_t j=0; j<GRID_Y; j++){
                  for (u_int32_t i=rid*columns; i<rid_next*columns; i++){
                    #if BOARD_W < GRID_X
                      if (i%BOARD_W==0) cout << " || ";
                      char dest[DIR_TO_PRINT+2+1]; dest[DIR_TO_PRINT+2] = '\0';
                    #else
                      char dest[DIR_TO_PRINT-2+1]; dest[DIR_TO_PRINT-2] = '\0';
                    #endif

                    RouterModel * r = routers[i][j];
                    int d;
                    for (d = 0; d <= E; d++) dest[d] = r->print_dest_route(d, qid, current_noc_cycle);
                    #if BOARD_W < GRID_X
                      dest[d++] = '(';
                      dest[d++] = r->print_dest_route(PY, qid, current_noc_cycle);
                      dest[d++] = r->print_dest_route(PX, qid, current_noc_cycle);
                      dest[d++] = ')';
                    #endif
                    dest[d++] = r->print_dest_route(C, qid, current_noc_cycle);
                    printf("%s  ",dest);
                  }
                  printf("\n");
                  if ((j+1)%BOARD_H==0) cout << " ==========================================================================================\n";
                  printf("%d:\t",j+1);
                }

                // Print % of occupancy of the Queue into the Core from Router
                cout << "\n\nQueue INTO the core, from the Router Channel["<< qid << "]\n0:\t";
                for (u_int32_t j=0; j<GRID_Y; j++){
                  for (u_int32_t i=rid*columns; i<rid_next*columns; i++){
                    //IQT3 is the input queue for T3, other tasks should use INPQ_LEN
                    printf("%X ",(int)((float)routers[i][j]->output_q[C][qid].occupancy()*10.0/(iq_sizes[qid]) ) );
                    }
                  printf("\n%d:\t",j+1);
                }
              #endif
            } //end for qid
            
            ULK;
          } //end 'if sample'
        #endif // END OF PRINT>=3

        // OBTAIN THE SLOWEST CORE ID
        u_int64_t current_pu_cy = noc_to_pu_cy(current_noc_cycle);
        u_int64_t min_pu_cy = current_pu_cy;
        u_int32_t tileid_min = rid;
        for (u_int32_t i=rid*columns; i<rid_next*columns; i++){
          for (u_int32_t j=0; j<GRID_Y; j++){
             u_int32_t tileid = global(i,j);
            if (core_timer[tileid*smt_per_tile] < min_pu_cy){
              min_pu_cy = core_timer[tileid*smt_per_tile]; tileid_min = tileid;
            }
          }
        }

        // The core (pu) should be ahead of the router (Active wait)
        // Wait while the router is ahead or equal to the slowest core
        while (current_pu_cy >= min_pu_cy){
          if (waiting_routers>0) break;
          volatile u_int64_t * timer_ptr = &core_timer[tileid_min*smt_per_tile];
          min_pu_cy = timer_ptr[0];
        }

        // Sync with other routers, make sure this router is not ahead
        u_int32_t mate_id = 0;
        u_int64_t my_time = router_timer[rid];
        while (mate_id < COLUMNS){
          volatile u_int64_t * timer_ptr = &router_timer[mate_id];
          if ( (timer_ptr[0] >= my_time) || !column_active[mate_id] || waiting_routers>0) mate_id++;
          //cout << "Router " << rid << " waiting for " << mate_id << " , "<< column_active[mate_id] << endl << flush;
        }

        // ADVANCE ROUTERS
        // We start as false, and if something is active, it will OR it with true
        local_router_active = false;
        u_int32_t base_i = rid*columns;
        u_int32_t end_i = rid_next*columns;
        for (u_int32_t i=base_i; i<end_i; i++){
          for (u_int32_t j=0; j<GRID_Y; j++){
            local_router_active |= isActive[global(i,j)];
            local_router_active |= (routers[i][j]->check_queues_occupancy() > 0);
          }
        }

        #if (MUX_BUS > 1)
          // Arbitrate between the board links X Dimension
          for (u_int32_t i=base_i; i<end_i; i++){
            if (i%BOARD_W_HALF==0){
              for (u_int32_t pY=0; pY<GRID_Y/BOARD_H; pY++){ //boards in the Y dim
                for (u_int32_t bY=0; bY<BOARD_H/MUX_BUS; bY++){ //buses Y dim
                  if (i%BOARD_W==0) board_arbitration_X(i,pY,bY,board_arbitration_W);
                  else board_arbitration_X(i,pY,bY,board_arbitration_E);
                }
              }
            }
          }
          // Arbitrate between the board links Y Dimension
          for (u_int32_t j=0; j<GRID_Y; j++){
            if (j%BOARD_H_HALF==0){
              for (u_int32_t pX=base_i/BOARD_W; pX<end_i/BOARD_W; pX++){ //boards in the X dim
                for (u_int32_t bX=0; bX<BOARD_W/MUX_BUS; bX++){ //buses X dim
                  if (j%BOARD_H==0) board_arbitration_Y(j,pX,bX,board_arbitration_N);
                  else board_arbitration_Y(j,pX,bX,board_arbitration_S);
                }
              }
            }
          }
        #endif

        // Check whether we enter in drain mode, and advance the routers
        for (u_int32_t i=base_i; i<end_i; i++){
          #if (BOARD_W == GRID_X)
            for (u_int32_t j=0; j<GRID_Y; j++) local_router_active |= (routers[i][j]->advance(&printf_mutex, NULL, NULL, current_noc_cycle, &frame_counters[i][j][ROUTER_ACTIVE]) > 0);
          #else
            // NOT ACCESSES IF BOARD_W == GRID_X
            for (u_int32_t pY=0; pY<GRID_Y/BOARD_H; pY++){ //board
              u_int32_t board_base = pY*BOARD_H;
              u_int32_t j = board_base;
              bool drain_Y_board[NUM_QUEUES];
              for (u_int32_t qid=0; qid<NUM_QUEUES; qid++){drain_Y_board[qid] = (routers[i][j]->input_q[PY][qid].is_drain()) || (routers[i][j+BOARD_H_HALF]->input_q[PY][qid].is_drain());}
              
              for (u_int32_t b=0; b<BOARD_H; b++){ //bus
                RouterModel * r = routers[i][j];
                u_int16_t board_origin_X = r->board_origin_X;
                u_int16_t board_end_X = r->board_end_X;
                bool drain_X[NUM_QUEUES];
                bool drain_Y[NUM_QUEUES];
                for (u_int32_t qid=0; qid<NUM_QUEUES; qid++){
                  drain_X[qid] = (routers[board_origin_X][j]->input_q[PX][qid].is_drain()) || (routers[board_end_X][j]->input_q[PX][qid].is_drain());
                  drain_Y[qid] = drain_X[qid] || drain_Y_board[qid];
                }
                local_router_active |= (routers[i][j]->advance(&printf_mutex, &drain_X[0], &drain_Y[0], current_noc_cycle, &frame_counters[i][j][ROUTER_ACTIVE]) > 0);
                j++;
              }
            }
          #endif
        }
        ++router_timer[rid];
        column_active[rid] = local_router_active;
      } // End of while(local_router_active) 

      // OUTSIDE OF THE LOCAL ROUTER LOOP
      { // BARRIER AMONG ROUTERS
          std::unique_lock<std::mutex> lock(router_mutex);
          waiting_routers++;
          if (waiting_routers == COLUMNS) {waiting_routers = 0;router_cv.notify_all();}
          else {router_cv.wait(lock, [&] { return waiting_routers == 0; });}
      }
      if (rid == 0) { //CHECK WHETHER THIS EPOCH IS DONE
        bool check_active = false;
        for (int th = 0; th < COLUMNS && !check_active; ++th) {
          check_active |= column_active[th];
          for (u_int32_t i=th*columns; i<(th+1)*columns; i++){
            for (u_int32_t j=0; j<GRID_Y; j++){
              RouterModel * r = routers[i][j];
              uint32_t global_cid = global(i,j);
              check_active |= isActive[global_cid];// || need_flush_pcache(global_cid);
              check_active |= (r->check_queues_occupancy() > 0);
            }
          }
        }
        global_router_active = check_active;
      }
      { // BARRIER AMONG ROUTERS 2
          std::unique_lock<std::mutex> lock(router_mutex2);
          waiting_routers2++;
          if (waiting_routers2 == COLUMNS) {waiting_routers2 = 0;router_cv2.notify_all();}
          else {router_cv2.wait(lock, [&] { return waiting_routers2 == 0; });}
      }
    } // End of while (global_router_active) loop, Exit if epoch is really done
    
    // OUTSIDE OF THE GLOBAL_ROUTER_ACTIVE LOOP, EPOCH IS DONE
    { // BARRIER AMONG ALL THREADS
      std::unique_lock<std::mutex> lock2(all_threads_mutex2);
      waiting_threads2++;
      if (waiting_threads2 == MAX_THREADS) {waiting_threads2 = 0;all_threads_cv2.notify_all();}
      else {all_threads_cv2.wait(lock2, [&] { return waiting_threads2 == 0; });}
    }
    
    print_lock.routerEpochDone(rid);
    router_timer[rid] += barrier_penalty;
    // Epochs ongoing is a local var
    current_epoch_ongoing = false;
    // Once we have synchronized the routers, we can check whether there are still nodes in the frontiers
    column_active[rid] = true;
    for (u_int32_t t = 0; t < COLUMNS; t++){
      current_epoch_ongoing |= epoch_has_work[t];
    }

    { // BARRIER AMONG ALL THREADS
        std::unique_lock<std::mutex> lock(all_threads_mutex);
        waiting_threads++;
        if (waiting_threads == MAX_THREADS) {waiting_threads = 0; global_router_active = 1; all_threads_cv.notify_all();}
        else {all_threads_cv.wait(lock, [&] { return waiting_threads == 0; });} 
    }
  } // End of while (current_epoch_ongoing) loop
  print_lock.routerProgramDone(rid);
}