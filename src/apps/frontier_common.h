
void check_range(int tX,int tY, u_int32_t start_index, u_int32_t end_index){
  assert(end_index>start_index);
  assert((end_index-start_index) <= max_task_chunk);
  u_int32_t edge_base = edgePerTile*global(tX,tY); 
  assert(start_index>=edge_base);
  assert(end_index<=(edge_base+edgePerTile));
}

void destroy_dataset_structures(){
  delete graph;
  free(ret);
  for (int i=0;i<PROXYS;i++)  free(proxys[i]);
  if (bitmap_frontier!=NULL)    free(bitmap_frontier);
  if (frontier_list!=NULL)      free(frontier_list);
  if (task_global_params!=NULL) free(task_global_params);
  if (in_r != NULL) free(in_r);
}

int task1_helper(int tX, int tY, int64_t * global_params, int64_t startInd, int64_t endEdgeIndex, int64_t value, u_int64_t timer){
  int64_t tile = startInd/edgePerTile;
  int64_t endTile = ((endEdgeIndex-1)/edgePerTile) + 1;
  int penalty = 6; // 4 op + comp + beq

  while (tile<endTile){
      int64_t next_tile = tile+1;
      int64_t edge_end_of_tile = next_tile*edgePerTile;
      int64_t endInd = min(endEdgeIndex, edge_end_of_tile);
      int64_t num_chunks = (endInd-startInd+max_task_chunk-1)/max_task_chunk;
      penalty += 8; //7 ops + beq

      for (int j = 0; j<num_chunks; j++){
          //6. Push, start, end indices to edge_array to the corresponding tile
          penalty += 1; //beq

          bool cond = OQ(1).capacity() > 2;
          if (cond){
              int64_t chunk_end = min(startInd+max_task_chunk, endInd); // 2 op
              penalty += 5; store(3); // 2 op + 3 stores + 1 mov
              u_int64_t new_time = timer+penalty;
              u_int32_t flit = getHeadFlit(1, (u_int64_t)startInd);
              OQ(1).enqueue(Msg(flit, HEAD,new_time) );
              OQ(1).enqueue(Msg(chunk_end, MID,new_time) );
              OQ(1).enqueue(Msg(value, TAIL,new_time) );
              startInd = chunk_end;
          } else {
              penalty += 4; store(4);
              //global_params is stored in .tex (variable section) of the scratchpad
              global_params[0] = startInd;
              global_params[1] = endEdgeIndex;
              global_params[2] = value;
              global_params[3] = 1;
              return penalty;
          }
      }
      penalty += 1; //mov
      tile = next_tile;
  }
  return penalty;
}

int process_nodes(int tX,int tY, int i, u_int32_t bitmap_base, u_int64_t timer){
  int count = 0;
  u_int32_t vector = 0, node, local_index;
  #if GLOBAL_BARRIER<=2
    vector = bitmap_frontier[bitmap_base + i]; //LD
    assert(vector); //META
  #endif
  
  do{
    #if GLOBAL_BARRIER<=2 //Coalescing
    //INST1 ->  local_node = SEARCH_MSB(VECTOR)
    u_int32_t msb; asm("bsrl %1,%0" : "=r"(msb) : "r"(vector));
    //INST2
    local_index = (i << 5) + msb;
    //INTR3 VECTOR = MASK_OUT_BIT(msb)
    u_int32_t msb_oh = ~(0xFFFFFFFF << msb); // 1 op
    vector = vector & msb_oh;
    #else
    local_index = i;
    #endif

    node = nodePerTile*global(tX,tY) + local_index; // 2 ops;

    //INST4 ADD NODE
    // This message would not need to be encoded as it comes directly from T4, 
    // but since we support different encondings than upper bits, we leave it for now
    IQ(0).enqueue(Msg(getHeadFlit(0, (u_int64_t)node), MONO,timer+count*6) );
    count++; //META
    //INST5&6 2 cond
  } while(vector > 0 && (!IQ(0).is_full()) );
  #if GLOBAL_BARRIER<=2 //Do not write if not coalescing
    bitmap_frontier[bitmap_base + i] = vector;
  #endif
  return count;
}

int task4_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_time){
  u_int32_t sum=0;
  int penalty = 2; //load + beq
  load(1);
  // 3 loads, done in HW
  u_int32_t tile_id = global(tX,tY);
  u_int32_t bitmap_base = bitmap_len*tile_id;
  u_int32_t frontier_list_base = frontier_list_len*tile_id;
  int tail = frontier_list[frontier_list_base + fr_tail_offset];
  int head = frontier_list[frontier_list_base + fr_head_offset];
  int size = (head < tail) ? fr_head_offset - (tail - head) : head - tail; // size is a variable stored

  u_int64_t time_fetched_list = timer-1, time_prefetch_list = time_fetched_list; //META
  u_int64_t global_tail = frontier_list_base + tail; //META
  u_int64_t prefetch_list_tag = cache_tag(frontier_list,global_tail)+1; //META

  u_int64_t time_fetched_bitmap = timer, time_prefetch_bitmap = time_fetched_bitmap; //META
  u_int64_t prefetch_bitmap_tag = cache_tag(bitmap_frontier, bitmap_base + frontier_list[global_tail] ) + 1; //META


  bool queue_full = IQ(0).is_full(); // queue not full is a register!
  penalty += 2; //and + beq
  while( (size>0) && !queue_full){
    
    u_int64_t global_tail = frontier_list_base + tail;
    penalty += check_dcache(tX,tY,frontier_list, global_tail, timer+penalty, time_fetched_list, time_prefetch_list, prefetch_list_tag);
    int index = frontier_list[global_tail];

    penalty += check_dcache(tX,tY,bitmap_frontier, bitmap_base+index, timer+penalty, time_fetched_bitmap, time_prefetch_bitmap, prefetch_bitmap_tag);
    int processed = process_nodes(tX,tY, index, bitmap_base, timer+penalty);
    store(1); //store bitmap
    penalty += processed*8; 

    queue_full = IQ(0).is_full();
    //assert(!queue_full); //WITH BARRIER

    //Vector is already loaded inside process nodes
    penalty += 4; // beq, add, and, sub
    //only move on if the vector was fully processed
    #if GLOBAL_BARRIER<=2
    // we don't check the state vector with no coalescing because index is node_idx and not block_idx
    if (bitmap_frontier[global(tX,tY)*bitmap_len + index]==0){
    #endif
      tail = (tail + 1) % fr_head_offset; size--;
    #if GLOBAL_BARRIER<=2
    }
    #endif
  }

  penalty++; //store
  frontier_list[frontier_list_base + fr_tail_offset] = tail;
  store(1);

  return penalty;
}


int add_to_frontier_list(int tX, int tY, u_int32_t to_add, u_int64_t timer){
  u_int32_t tile_id = global(tX,tY);
  u_int32_t frontier_list_base = frontier_list_len*tile_id;
  int head = frontier_list[frontier_list_base + fr_head_offset];
  int headp1 = (head + 1) % fr_head_offset; // AND OP
  int tail = frontier_list[frontier_list_base + fr_tail_offset];

  frontier_list[frontier_list_base + head] = to_add;
  int penalty = check_dcache(tX,tY,frontier_list, frontier_list_base + head, timer);

  frontier_list[frontier_list_base + fr_head_offset] = headp1;
  
  return penalty + 2;
}

int add_to_frontier(int tX, int tY, u_int32_t neighbor_index, u_int64_t timer){
  int penalty = 4; // sub, shift, load, beq
  u_int32_t local_idx = neighbor_index % nodePerTile;

  u_int32_t bitmap_base = bitmap_len*global(tX,tY);
  #if GLOBAL_BARRIER <=2 //coalescing
    u_int32_t local_block = local_idx >> 5;
    u_int32_t index = bitmap_base + local_block;
    penalty += check_dcache(tX,tY,bitmap_frontier,index, timer+penalty);
    u_int32_t vector = bitmap_frontier[index];
    if (vector == 0){
      penalty+=3; //load, beq, add
      penalty+=add_to_frontier_list(tX, tY, local_block, timer+penalty);
    }
    //INTR1 VECTOR = MASK_IN_BIT(local_idx)
    u_int32_t block_idx = local_idx & 0x0000001F;
    u_int32_t block_idx_oh = 0x00000001 << block_idx;
    vector = vector | block_idx_oh;
    //INTR2 Store vector
    bitmap_frontier[index] = vector;
    #if ASSERT_MODE
      assert(vector > 0);
    #endif
    load(3); store(3);
  #else
    // No Coalescing
    penalty+=add_to_frontier_list(tX, tY, local_idx, timer+penalty);
  #endif

  return penalty+2;
}