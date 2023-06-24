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
  for (int i = 0; i < graph->nodes; i++) ret[i] = proxy_default;
}

void config_dataset(string dataset_filename){
  dataset_has_edge_val = 0;
  app_has_frontier = 1;
  graph = new graph_loader(dataset_filename,1);
}

void config_app(){
  proxy_default = INT32_MAX;

  get_frontier_size(nodePerTile);
  dataset_words_per_tile = nodePerTile; //ret_array 
  dataset_words_per_tile += edgePerTile; //edge index array
  dataset_words_per_tile += nodePerTile; //node index array
  if (app_has_frontier) dataset_words_per_tile += (bitmap_len + frontier_list_len); // Bitmap Frontier is (nodePerTile/32) + Block List (nodePerTile/32)
}

int task_init(int tX, int tY){
  routers[tX][tY]->output_q[C][0].enqueue(Msg(0, MONO,0) );
  return 1;
}

int task1_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  int64_t * global_params = &task_global_params[global(tX,tY)*num_global_params];
  int64_t startInd, endEdgeIndex, label;
  R_queue<Msg> * queue = routers[tX][tY]->output_q[C];
  int64_t node_base = nodePerTile*global(tX,tY);
  int penalty = 3; // beq + LD*2
  load(2); 

  if (global_params[3]) {
      global_params[3] = 0;
      startInd = global_params[0];
      endEdgeIndex = global_params[1];
      label = global_params[2];
      load(3); store(1); penalty+=4;
  } else {
    int64_t node;
    Msg msg = queue[0].peek();
    u_int64_t time_fetched = msg.time; //META
    penalty+=3; // LD, beq, jmp

    if (global_params[4]>=0){ //Still exploring seeds
      msg = queue[0].dequeue(); //MOV

      node = msg.data + node_base; // Preloaded
      int64_t endNodeInd = min(node_base+nodePerTile, (int64_t)graph->nodes); //2 OP
      penalty+=3;

      u_int64_t time_prefetch = msg.time; //META
      u_int64_t prefetch_tag = cache_tag(ret,node)+1; //META
      // Only iterate over nodes that have not been visited yet (equal to -1)
      while (node<endNodeInd && (ret[node]!=proxy_default) ){ // 3 CMP + BEQ
        node++; 
        penalty+=5; // LD, ADD, 3 CMP, BEQ 
        penalty+=check_dcache(tX,tY,ret,node, timer+penalty, time_fetched, time_prefetch, prefetch_tag);
      }

      if (node == endNodeInd) {global_params[4] = -1; return penalty;} //End seeds, 3 OP
      queue[0].enqueue(Msg(node - node_base, MONO,timer+penalty) ); //MOV
      label = node; //MOV 
      ret[node] = node; // ST
      store(1);load(1);penalty+=6;

    } else { //Receiving from other tiles
      #if ASSERT_MODE
        assert(queue[0].occupancy()>0);
      #endif
      node = task3_dequeue(msg.data); //LD from Q
      #if ASSERT_MODE
        assert(node<graph->nodes);
      #endif
      label = ret[node];
      load(2); penalty+=5; // beq, LD*4
    }

    penalty+=check_dcache(tX,tY,graph->node_array,node, timer+penalty, msg.time);
    penalty+=check_dcache(tX,tY,graph->node_array,node+1, timer+penalty, msg.time);
    penalty+=check_dcache(tX,tY,ret,node, timer+penalty, time_fetched);

    startInd = graph->node_array[node]; //LD
    endEdgeIndex = graph->node_array[node+1]; //LD
  }
  
  penalty+=task1_helper(tX,tY,global_params, startInd, endEdgeIndex, label, timer+penalty); //community label
  // Only pop here if we were in the case of receiving from the network
  if ((global_params[4] == -1) && (global_params[3] == 0)) queue[0].dequeue();
  return penalty;
}

int task2_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  R_queue<Msg> * queue = routers[tX][tY]->output_q[C];
  int penalty = 0;

  //1. Read start, end indices to edge_array and label.
  Msg msg = queue[1].dequeue();
  u_int32_t start_index = task2_dequeue(msg.data);
  u_int32_t end_index   = queue[1].dequeue().data;
  int label = queue[1].dequeue().data;
  load(3);
  #if ASSERT_MODE && STEAL_W == 1
    check_range(tX,tY,start_index,end_index);
  #endif

  //2. Push label and neighbor to corresponding tile.
  u_int64_t time_fetched_edges = msg.time, time_prefetch_edges = msg.time; //META
  u_int64_t prefetch_edges_tag = cache_tag(graph->edge_array,start_index)+1; //META
  for (u_int32_t i=start_index; i<end_index; i++){
    u_int64_t neighbor = graph->edge_array[i];
    penalty += check_dcache(tX,tY,graph->edge_array,i,timer+penalty, time_fetched_edges, time_prefetch_edges, prefetch_edges_tag);

    u_int32_t startInd = graph->node_array[neighbor]; //META, cleanup dataset, no process empty node
    u_int32_t endEdgeIndex = graph->node_array[neighbor+1];  //META, cleanup dataset, do not count
    if (startInd<endEdgeIndex){
      //Invoke next task
      int dest = get_qid();
      OQ(dest).enqueue(Msg(getHeadFlit(dest, neighbor), HEAD,timer) );
      OQ(dest).enqueue(Msg(label, TAIL,timer) );
    }
  }

  // Instruction to configure a vector operation 
  // vector buffer_neighbor = out_vector(base=buffer_base, length=neighbor_len, stride=2)
  // vector buffer_label = out_vector(base=buffer_base+1, length=neighbor_len, stride=2)
  // vector edge_load = out_vector(base=neighbor_begin, length=neighbor_len, stride=1)
  // MOV buffer_neighbor = edge_load
  // MOV buffer_label = label
  // ASYNC MOV OQ = out_vector(base=buffer_base, length=neighbor_len, chain_length=2)
  u_int32_t vector_len = (end_index-start_index);
  numEdgesProcessed[tX/COLUMNS_PER_TH]+=vector_len; //META
  store(vector_len*2);
  return 7+penalty; // 3 queue loads + 3 vector configs, 2 vector ops of length (neighbor_len) + 1 MOV to OQ
}


int task3_kernel(int tX, int tY, u_int64_t timer, u_int64_t & compute_cycles){
  R_queue<Msg> * queue = routers[tX][tY]->output_q[C];
  Msg msg = queue[2].dequeue();
  u_int32_t neighbor = task3_dequeue(msg.data); //LD from Q
  u_int32_t label = queue[2].dequeue().data; //LD from Q
  load(2);
  int penalty;

  #if PROXY_FACTOR==1 && ASSERT_MODE
    u_int32_t node_base = nodePerTile*global(tX,tY);
    assert(neighbor>=node_base); assert(neighbor<(node_base+nodePerTile));
  #endif

  int ret_neighbor = ret[neighbor];
  penalty = check_dcache(tX,tY,ret,neighbor,timer, msg.time);

  // Take the MIN of the label and current stored
  bool min_cond = label < ret_neighbor;
  penalty+=4; //CMP, BEQ, JMP
  
  if (min_cond) {
    ret[neighbor]=label; //Hit's for sure in the cache
    store(1);

    int startInd = graph->node_array[neighbor]; //META, cleanup dataset
    int endEdgeIndex = graph->node_array[neighbor+1];  //META, cleanup dataset
    assert(startInd<endEdgeIndex);  //META, cleanup dataset

    penalty+=add_to_frontier(tX, tY,  neighbor,timer) + 1; //ADD to Frontier + ret)
    numFrontierNodesPushed[tX/COLUMNS_PER_TH]+=1; //Meta
  }

return penalty;
}

int task3bis_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  R_queue<Msg> * queue = routers[tX][tY]->output_q[C];
  Msg msg = queue[3].dequeue();
  u_int64_t neighbor = task3_dequeue(msg.data); //LD from Q
  u_int32_t label = queue[3].dequeue().data; //LD from Q
  load(2);
  int penalty;

  // Default from proxy cache (if not present) should be -1 or inf, so we visit it
  int ret_neighbor = check_pcache(tX,tY,neighbor,timer);
  bool min_cond = label < ret_neighbor;
  penalty = 6; //Load, 2 CMP, OR, BEQ, JMP
  if (min_cond) {
    update_pcache(tX,tY,neighbor,label);
    #if WRITE_THROUGH
      store(1);
      OQ(2).enqueue(Msg(getHeadFlit(2, neighbor), HEAD,timer) );
      OQ(2).enqueue(Msg(label, TAIL,timer) );
      penalty+=3; // ST, MOV, MOV
    #endif
  }
return penalty;
}