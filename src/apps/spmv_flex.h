bool compare_out(char* output_name){
  bool result_correct = graph->output_result(output_name);
  return result_correct;
}

int num_global_params = 5;
void initialize_dataset_structures(){
  task_global_params = (int64_t *) calloc(num_global_params*GRID_SIZE, sizeof(int64_t));
  initialize_proxys(); cout << "proxys initialized\n"<<flush;

  ret = (int *) malloc(sizeof(int) * graph->nodes);
  for (int i = 0; i < graph->nodes; i++) ret[i] = 0;
}

void config_dataset(string dataset_filename){
  dataset_has_edge_val = 1;
  graph = new graph_loader(dataset_filename,1);
}

void config_app(){
  proxy_default = 0;
  u_int32_t t3b_piped_tasks = 1;
  u_int32_t t3_piped_tasks  = 64 * smt_per_tile;
  #if PROXY_FACTOR>1 //proxys
    t3b_piped_tasks = t3_piped_tasks;
    t3_piped_tasks  = PROXY_ROUTING==1 ? unused_buffer : t3_piped_tasks / proxy_shrink;
  #endif

  task1_dest = dest_qid;

  iq_sizes[1] = unused_buffer;
  oq_sizes[1] = unused_buffer;
  oq_sizes[2] = t3_piped_tasks  * num_task_params[2];
  iq_sizes[2] = oq_sizes[2] * io_factor_t3;
  oq_sizes[3] = t3b_piped_tasks * num_task_params[3];
  iq_sizes[3] = oq_sizes[3] * io_factor_t3b;
  
  dataset_words_per_tile = nodePerTile; //ret_array 
  dataset_words_per_tile += edgePerTile; //edge index array
  dataset_words_per_tile += nodePerTile; //node index array
  dataset_words_per_tile += edgePerTile + nodePerTile; // edge_values and dense vector
}

int task_init(int tX, int tY){
  routers[tX][tY]->output_q[C][0].enqueue(Msg(0, MONO,0) );
  return 1;
}


int iterate(int tX, int tY, int64_t * global_params, u_int64_t startInd, u_int64_t endEdgeIndex, u_int64_t source_dist, u_int64_t timer){
  int penalty = 0;
  u_int64_t endInd = endEdgeIndex;
  int dest = get_qid();
  u_int32_t capacity = OQ(dest).capacity() / 2;
  u_int32_t vector_len = (endEdgeIndex-startInd);
  if (vector_len > capacity){
    vector_len = capacity;
    endInd = startInd + vector_len;
    penalty += 4; store(4);
    //global_params is stored in .tex (variable section) of the scratchpad
    global_params[0] = endInd;
    global_params[1] = endEdgeIndex;
    global_params[2] = source_dist;
    global_params[3] = 1;
  }  
  numEdgesProcessed[tX/COLUMNS_PER_TH]+=vector_len; //META

  int i = startInd;
  for (int i = startInd; i<endInd; i++){
    penalty += check_dcache(tX,tY,graph->edge_array,i,timer+penalty);
    penalty += check_dcache(tX,tY,graph->edge_values,i,timer+penalty);
    int columnID = graph->edge_array[i];
    int new_dist = source_dist * graph->edge_values[i];
    flop(1);
    OQ(dest).enqueue(Msg(getHeadFlit(dest, columnID), HEAD,timer) );
    OQ(dest).enqueue(Msg(new_dist, TAIL,timer) );
  }
  store(vector_len*2);
  penalty+=8; // 3 queue loads + 4 vector configs, 2 vector ops of length (columnID_len) + 1 MOV to OQ
  return penalty;
}



int task1_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  u_int64_t startInd, endEdgeIndex, source_dist;

  int64_t * global_params = &task_global_params[global(tX,tY)*num_global_params];
  int penalty = 0;
  load_mem_wait(1); // global_params[3]

  if (global_params[3]) {//LD. Transaction is 'parked'
    // Global variables
    global_params[3]=0;
    startInd = global_params[0];
    endEdgeIndex = global_params[1];
    source_dist = global_params[2];
    load_mem_wait(3);
    store(1); penalty = 2; // beq + store
    penalty += iterate(tX,tY,global_params, startInd, endEdgeIndex, source_dist, timer+penalty);

  } else {

    Msg msg = IQ(0).dequeue(); //LD
    int node_base = nodePerTile*global(tX,tY);
    //last node that is being processed, compile time, not calculated, LD
    int nodeEnd = min(node_base+nodePerTile-1, (int)(graph->nodes-1));
    int node = msg.data+node_base; //LD, no ADD (HW)

    u_int64_t time_fetched = msg.time, time_prefetch = msg.time; //META
    u_int64_t prefetch_tag = cache_tag(graph->node_array,node)+1; //META
    u_int64_t time_fetched_vec = msg.time, time_prefetch_vec = msg.time; //META
    u_int64_t prefetch_tag_vec = cache_tag(graph->dense_vector,node)+1; //META

    bool done = true;
    bool stop = (node > nodeEnd);
    // BEQ,JMP
    penalty+=2;
    while(!stop){
      penalty+=check_dcache(tX,tY,graph->node_array,node,   timer+penalty, time_fetched, time_prefetch, prefetch_tag);
      penalty+=check_dcache(tX,tY,graph->node_array,node+1, timer+penalty, time_fetched, time_prefetch, prefetch_tag);
      startInd = graph->node_array[node]; //LD
      endEdgeIndex = graph->node_array[node+1]; //LD
      bool len_not_zero = (endEdgeIndex-startInd) > 0;
      if (len_not_zero){
        penalty+=check_dcache(tX,tY,graph->dense_vector, node, timer+penalty, time_fetched_vec, time_prefetch_vec, prefetch_tag_vec);
        source_dist = 5;//graph->dense_vector[node]; //LD, Access to dense_vec array     
        penalty += iterate(tX,tY,global_params, startInd, endEdgeIndex, source_dist, timer+penalty);
      }
      node++; // ADD
      done = (node > nodeEnd); //EQ
      stop = done || global_params[3]; //OR. Transaction is 'parked'
      penalty+=3;
    }
    penalty+=2;  //AND, BEQ
    if (!done || global_params[3]){
      IQ(0).enqueue(Msg(node - node_base, MONO,timer+penalty) );
      penalty+=1;store(1);
    }
  }

  return penalty;
}


int task2_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  return 2;
}

int task3_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  Msg msg = IQ(2).dequeue();
  u_int32_t columnID = task3_dequeue(msg.data);
  u_int32_t new_dist = IQ(2).dequeue().data;

  int penalty = check_dcache(tX,tY,ret,columnID,timer, msg.time);
  ret[columnID] += new_dist;
  flop(1); store(1);
  numFrontierNodesPushed[tX/COLUMNS_PER_TH]++; //Meta
  return penalty+1;
}

int task3bis_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  Msg msg = IQ(3).dequeue();
  u_int32_t columnID = task3_dequeue(msg.data);
  u_int32_t new_dist = IQ(3).dequeue().data;

  //ADDI [columnID], new_dist
  int penalty = 0;
  int ret_columnID = check_pcache(tX,tY,columnID, penalty, timer);
  ret_columnID += new_dist;
  flop(1);
  update_pcache(tX,tY,columnID,ret_columnID);

  numFrontierNodesPushed[tX/COLUMNS_PER_TH]++; //Meta
  return penalty+1;
}
