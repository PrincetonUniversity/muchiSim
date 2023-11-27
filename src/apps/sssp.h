bool compare_out(char* output_name){
  bool result_correct = graph->output_result(output_name);
  return result_correct;
}

int num_global_params = 5;
void initialize_dataset_structures(){
  task_global_params = (int64_t *) calloc(num_global_params*GRID_SIZE, sizeof(int64_t));
  initialize_proxys(); cout << "Replicas initialized\n"<<flush;

  frontier_list = (int *) calloc(frontier_list_len*GRID_SIZE, sizeof(int));
  bitmap_frontier = (u_int32_t *) calloc(bitmap_len*GRID_SIZE, sizeof(u_int32_t));
  cout << "Allocated Frontier List" << endl << flush;

  ret = (int *) malloc(sizeof(int) * graph->nodes);
  ret[0] = 0;
  for (int i = 1; i < graph->nodes; i++) ret[i] = INT32_MAX>>1;
}

void config_dataset(string dataset_filename){
  dataset_has_edge_val = 1;
  app_has_frontier = 1;
  graph = new graph_loader(dataset_filename,1);
}

void config_app(){

  get_frontier_size(nodePerTile);
  dataset_words_per_tile = nodePerTile; //ret_array 
  dataset_words_per_tile += edgePerTile; //edge index array
  dataset_words_per_tile += nodePerTile; //node index array
  #if APP==SSSP
  dataset_words_per_tile += edgePerTile; // edge_values array
  #endif
  if (app_has_frontier) dataset_words_per_tile += (bitmap_len + frontier_list_len); // Bitmap Frontier is (nodePerTile/32) + Block List (nodePerTile/32)
}

int task_init(int tX, int tY){
  if (tX==0 && tY==0) routers[0][0]->output_q[C][0].enqueue(Msg(0,MONO,0));
  return 1;
}

int task1_kernel(int tX, int tY, u_int64_t timer, u_int64_t & compute_cycles){
  int64_t * global_params = &task_global_params[global(tX,tY)*num_global_params];
  int64_t startInd, endEdgeIndex, source_dist;

  int penalty = 2; // beq,jmp
  load_mem_wait(1);
  if (global_params[3]) {
    global_params[3] = 0;
    startInd = global_params[0];
    endEdgeIndex = global_params[1];
    source_dist = global_params[2];
    load_mem_wait(3);
  } else {
    Msg msg = IQ(0).peek();
    int node = task3_dequeue(msg.data);

    #if ASSERT_MODE
    // Check that node is within the range of nodes that this core is responsible for
    int node_base = nodePerTile*global(tX,tY);
    assert(node>=node_base); assert(node<(node_base+nodePerTile));
    #endif

    startInd = graph->node_array[node]; //LD
    endEdgeIndex = graph->node_array[node+1]; //LD
    source_dist = ret[node]; //LD
    if (startInd==endEdgeIndex) return 2; // META: Remove these nodes
    penalty+=check_dcache(tX,tY,graph->node_array,node, timer+penalty, msg.time);
    penalty+=check_dcache(tX,tY,graph->node_array,node+1, timer+penalty, msg.time);
    penalty+=check_dcache(tX,tY,ret,node, timer+penalty, msg.time);
  }

  penalty+=task1_helper(tX,tY,global_params, startInd, endEdgeIndex, source_dist, timer+penalty);
  if (global_params[3]==0) IQ(0).dequeue();
  return penalty;
}

int task2_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  int penalty = 0;

  Msg msg = IQ(1).dequeue();
  u_int32_t start_index = task2_dequeue(msg.data);
  u_int32_t end_index = IQ(1).dequeue().data;
  int sourceDist  = IQ(1).dequeue().data;

  #if ASSERT_MODE && STEAL_W == 1
    // ASSERT ONLY WITHOUT STEAL REGION
    check_range(tX,tY,start_index,end_index);
  #endif

  // Prefetch the edge_array
  u_int64_t time_fetched_edges = msg.time, time_prefetch_edges = msg.time; //META
  u_int64_t prefetch_edges_tag = cache_tag(graph->edge_array,start_index)+1; //META
  #if APP==SSSP
    //Do the same for edge_values
    u_int64_t time_fetched_value = msg.time, time_prefetch_value = msg.time; //META
    u_int64_t prefetch_value_tag = cache_tag(graph->edge_values,start_index)+1; //META
  #endif

  for (u_int32_t i=start_index; i<end_index; i++){
    u_int64_t neighbor = graph->edge_array[i];// REAL ACCESS

    u_int32_t startInd = graph->node_array[neighbor]; //META, cleanup dataset, no process empty node
    u_int32_t endEdgeIndex = graph->node_array[neighbor+1];  //META, cleanup dataset, do not count
    if (startInd<endEdgeIndex){  //META, IF due to cleanup dataset, do not count

      penalty+=check_dcache(tX,tY,graph->edge_array,i, timer+penalty, time_fetched_edges, time_prefetch_edges, prefetch_edges_tag);
      #if APP==SSSP
        penalty+=check_dcache(tX,tY,graph->edge_values,i,timer+penalty, time_fetched_value, time_prefetch_value, prefetch_value_tag);
        u_int32_t new_dist = sourceDist + graph->edge_values[i];
      #else
        u_int32_t new_dist = sourceDist + 1;
      #endif
      flop(1); // We also count here that BFS performs a float even though it doesn't make much sense, just so flops are not 0 in BFS
      int dest = get_qid();
      OQ(dest).enqueue(Msg(getHeadFlit(dest, neighbor), HEAD,timer) );
      OQ(dest).enqueue(Msg(new_dist, TAIL,timer) );
    }
  }
  // Instruction to configure a vector operation 
  // vector buffer_neighbor = out_vector(base=buffer_base, length=neighbor_len, stride=2)
  // vector buffer_dist = out_vector(base=buffer_base+1, length=neighbor_len, stride=2)
  // vector edge_load = out_vector(base=neighbor_begin, length=neighbor_len, stride=2)
  // vector dist_load = out_vector(base=neighbor_begin+1, length=neighbor_len, stride=2)  // IF SSSP
  // MOV buffer_neighbor = edge_load
  // MOV buffer_dist = dist_load + sourceDist // or 1 + sourceDist in BFS
  // ASYNC MOV OQ = out_vector(base=buffer_base, length=neighbor_len, chain_length=2)
  u_int32_t vector_len = (end_index-start_index);
  numEdgesProcessed[tX/COLUMNS_PER_TH]+=vector_len; //META
  store(vector_len*2);
  // TODO revisit penalty
  #if APP==SSSP
    return 8+penalty; // 3 queue loads + 4 vector configs, 2 vector ops of length (neighbor_len) + 1 MOV to OQ
  #else //BFS
    return 7+penalty; // 3 queue loads + 3 vector configs, 2 vector ops of length (neighbor_len) + 1 MOV to OQ
  #endif
}

int task3_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  Msg msg = IQ(2).dequeue();
  u_int32_t neighbor = task3_dequeue(msg.data);
  u_int32_t new_dist =  IQ(2).dequeue().data;

  int penalty = check_dcache(tX,tY,ret,neighbor,timer, msg.time);
  int ret_neighbor = ret[neighbor];
  penalty +=2; //comp + beq
  bool cond = (new_dist < ret_neighbor); flop(1);
  if (cond){
    ret[neighbor] = new_dist; //Surely it's a hit in the cache
    penalty+=1;store(1);
    penalty+=add_to_frontier(tX, tY, neighbor,timer) + 1; //ADD to Frontier + Store
    numFrontierNodesPushed[tX/COLUMNS_PER_TH]+=1; //Meta
  }
  return penalty;
}


int task3bis_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  Msg msg = IQ(3).dequeue();
  u_int64_t neighbor = task3_dequeue(msg.data);
  u_int32_t new_dist = IQ(3).dequeue().data;

  int penalty = 0;
  int ret_neighbor = check_pcache(tX,tY,neighbor,penalty,timer);
  bool cond = (new_dist < ret_neighbor); flop(1);
  penalty += 3; //Load + comp + beq
  if (cond){
    update_pcache(tX, tY, neighbor,new_dist);
    #if WRITE_THROUGH
      OQ(2).enqueue(Msg(getHeadFlit(2, neighbor), HEAD,timer) );
      OQ(2).enqueue(Msg(new_dist, TAIL,timer) );
      penalty+=3; // ST, MOV, MOV
    #endif
  }
  return penalty;
}