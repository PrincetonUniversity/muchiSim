bool compare_out(char* output_name){
  bool result_correct = graph->output_result(output_name);
  return result_correct;
}

int num_global_params = 5;
void initialize_dataset_structures(){
  task_global_params = (int64_t *) calloc(num_global_params*GRID_SIZE, sizeof(int64_t));
  initialize_proxys(); cout << "proxys initialized\n"<<flush;

  ret = (int *) malloc(sizeof(int) * graph->nodes * B_numcols);
  for (int i = 0; i < graph->nodes * B_numcols; i++) ret[i] = 0;
}

void config_dataset(string dataset_filename){
  dataset_has_edge_val = 1;
  graph = new graph_loader(dataset_filename,1);
}

void config_app(){
  proxy_default = 0;
  num_task_params[2] = 3;
  num_task_params[3] = num_task_params[2];
  u_int32_t t3_piped_tasks  = t2_piped_tasks * t2_t3_ratio;
  oq_sizes[2] = t3_piped_tasks * num_task_params[2];
  iq_sizes[2] = oq_sizes[2] * io_factor_t3;

  dataset_words_per_tile = nodePerTile*B_numcols; //ret_array 
  dataset_words_per_tile += edgePerTile; //edge index array
  dataset_words_per_tile += nodePerTile; //node index array
  dataset_words_per_tile += edgePerTile; // edge_values and dense vector
  dataset_words_per_tile += nodePerTile*B_numcols; // dense vector
}

int task_init(int tX, int tY){
  // Task3 is fetching data from DRAM, so we need to be in data cache mode.
  assert(dataset_cached);
  routers[tX][tY]->output_q[C][0].enqueue(Msg(0, MONO,0) );
  return 1;
}

int task1_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  int64_t * global_params = &task_global_params[global(tX,tY)*num_global_params];
  u_int64_t startInd, endEdgeIndex, value;

  int penalty = 0;
  load(1); 
  if (global_params[3]) {//LD. Transaction is 'parked'
    // Global variables
    global_params[3]=0;
    startInd = global_params[0];
    endEdgeIndex = global_params[1];
    value = global_params[2];
    load(3); store(1); penalty = 6; // beq + 5 mem
    penalty+=task1_helper(tX,tY,global_params, startInd, endEdgeIndex, value, timer+penalty);

  } else{

    Msg msg = IQ(0).dequeue(); //LD 

    load(2);
    int colID_base = nodePerTile*global(tX,tY);
    //last colID that is being processed, compile time, not calculated, LD
    int colID_end = min(colID_base+nodePerTile-1, (int)(graph->nodes-1));
    int colID = msg.data+colID_base; //LD, no ADD (HW)

    u_int64_t time_fetched = msg.time, time_prefetch = msg.time; //META
    u_int64_t prefetch_tag = cache_tag(graph->node_array,colID)+1; //META

    bool done = true;
    bool stop = (colID > colID_end);
    // BEQ,JMP
    penalty+=2;
    while(!stop){
      penalty+=check_dcache(tX,tY,graph->node_array, colID,   timer+penalty, time_fetched, time_prefetch, prefetch_tag);
      penalty+=check_dcache(tX,tY,graph->node_array, colID+1, timer+penalty, time_fetched, time_prefetch, prefetch_tag);
      startInd = graph->node_array[colID]; //LD
      endEdgeIndex = graph->node_array[colID+1]; //LD

      if (startInd<endEdgeIndex){ //EQ, JMP
        penalty+=task1_helper(tX,tY,global_params, startInd, endEdgeIndex, colID, timer+penalty);
      }

      colID++; // ADD
      done = (colID > colID_end); //EQ
      stop = done || global_params[3]; //OR. Transaction is 'parked'
      penalty+=3;
    }
    penalty+=2;  //AND, BEQ
    if (!done || global_params[3]){
      IQ(0).enqueue(Msg(colID - colID_base, MONO,timer+penalty) );
      penalty+=1;store(1);
    }

  }
  return penalty;
}


int task2_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  Msg msg = IQ(1).dequeue();
  u_int32_t start_index = task2_dequeue(msg.data);
  u_int32_t end_index = IQ(1).dequeue().data;
  int A_colID  = IQ(1).dequeue().data;
  load(3);

  #if ASSERT_MODE && STEAL_W == 1
    check_range(tX,tY,start_index,end_index);
    assert(msg.type == HEAD);
  #endif

  u_int32_t vector_len = (end_index-start_index);
  numEdgesProcessed[tX/COLUMNS_PER_TH] += vector_len * B_numcols; //META
  u_int32_t random_start = start_index % vector_len; //random_number between 0 and vector_len-1
  u_int32_t pivot = start_index + random_start;

  u_int64_t time_fetched_edges = msg.time, time_prefetch_edges = msg.time; //META
  u_int64_t prefetch_edges_tag = cache_tag(graph->edge_array,pivot)+1; //META
  u_int64_t time_fetched_value = msg.time, time_prefetch_value = msg.time; //META
  u_int64_t prefetch_value_tag = cache_tag(graph->edge_values,pivot)+1; //META

  int penalty = 0;
  while (start_index < end_index){
    u_int32_t i = pivot;
    while(i<end_index){
      penalty += check_dcache(tX,tY,graph->edge_array,i,timer+penalty, time_fetched_edges, time_prefetch_edges, prefetch_edges_tag);
      penalty += check_dcache(tX,tY,graph->edge_values,i,timer+penalty, time_fetched_value, time_prefetch_value, prefetch_value_tag);
      int A_rowID = graph->edge_array[i];
      int sparse_val = graph->edge_values[i];
      // Invokation of the next task
      int dest = get_qid();
      OQ(dest).enqueue(Msg(getHeadFlit(dest, A_rowID), HEAD,timer) );
      OQ(dest).enqueue(Msg(A_colID, MID,timer) );
      OQ(dest).enqueue(Msg(sparse_val, TAIL,timer) );
      i+=1;
    }
    end_index = pivot;
    pivot = start_index;
  }

  store(vector_len*2);
  return 8+penalty; // 3 queue loads + 4 vector configs, 2 vector ops of length (columnID_len) + 1 MOV to OQ
}

int task3_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  Msg msg = IQ(2).dequeue();
  u_int32_t A_rowID = task3_dequeue(msg.data);
  u_int32_t A_colID = IQ(2).dequeue().data;
  u_int32_t sparse_value = IQ(2).dequeue().data;
  load(2); int penalty = 2;

  for (int B_colID = 0; B_colID < B_numcols; B_colID++){
    //C[A_rowID][B_colID] += sparse_val * B[A_colID][B_colID];
    penalty += check_dcache(tX,tY,graph->dense_vector, A_colID * B_numcols + B_colID, timer+penalty);
    u_int32_t ret_index = A_rowID * B_numcols + B_colID;
    penalty += check_dcache(tX,tY,ret, ret_index, timer+penalty);
    ret[ret_index] += sparse_value * graph->dense_value;
    flop(2); store(1);
  }
  // ret[A_rowID] += sparse_value * graph->dense_value;
  numFrontierNodesPushed[tX/COLUMNS_PER_TH] += B_numcols; //Meta
  return penalty+1;
}

int task3bis_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  Msg msg = IQ(3).dequeue();
  u_int32_t A_rowID = task3_dequeue(msg.data);
  u_int32_t A_colID = IQ(3).dequeue().data;
  u_int32_t sparse_value = IQ(3).dequeue().data;
  load(2); int penalty = 2;

  for (int B_colID = 0; B_colID < B_numcols; B_colID++){
    penalty += check_dcache(tX,tY,graph->dense_vector, A_colID * B_numcols + B_colID, timer+penalty);
    u_int32_t ret_index = A_rowID * B_numcols + B_colID;
    int value = check_pcache(tX,tY, ret_index, penalty, timer);
    value += sparse_value * graph->dense_value;
    flop(2);
    update_pcache(tX,tY, ret_index, value);
  }
  return penalty+1;
}
