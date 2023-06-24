// ==== PAGERANK CONSTANTS ====
const float alpha = 0.85;
const float minusalpha = 1 - alpha;
const float alphamult = minusalpha * alpha;
const float epsilon = 1.0;

bool compare_out(char* output_name){
  bool result_correct = graph->output_result(output_name);
  result_correct &= graph->compare_out_pagerank("doall/page_K16_ref.txt");
  return result_correct;
}

void config_dataset(string dataset_filename){
  dataset_has_edge_val = 0;
  app_has_frontier = 1;
  graph = new graph_loader(dataset_filename,1);
}

void config_app(){ //After dataset has been configured
  proxy_default = 0;

  get_frontier_size(nodePerTile);
  dataset_words_per_tile = nodePerTile; //ret_array 
  dataset_words_per_tile += edgePerTile; //edge index array
  dataset_words_per_tile += nodePerTile; //node index array
  dataset_words_per_tile += nodePerTile; // residuals_array
  if (app_has_frontier) dataset_words_per_tile += (bitmap_len + frontier_list_len); // Bitmap Frontier is (node_count/32) + Block List (node_count/32)
}

int num_global_params = 5;
void initialize_dataset_structures(){
  task_global_params = (int64_t *) calloc(num_global_params*GRID_SIZE, sizeof(int64_t));
   
  frontier_list = (int *) calloc(frontier_list_len*GRID_SIZE, sizeof(int));
  bitmap_frontier = (u_int32_t *) calloc(bitmap_len*GRID_SIZE, sizeof(u_int32_t));
  cout << "Allocated Frontier List" << endl << flush;

  // PAGE
  ret = (float *) malloc(sizeof(float) * graph->nodes);
  in_r = (float *) malloc(sizeof(float) * graph->nodes);
  for (int i=0; i<PROXYS; i++){proxys[i] = (float *) malloc(sizeof(float) * graph->nodes);}
  cout << "Allocated Proxys" << endl << flush;
  for (int v = 0; v < graph->nodes; v++) {
    ret[v] = minusalpha;
    in_r[v] = 0.0;
    for (int i=0; i<PROXYS; i++){proxys[i][v] = 0.0;}
  }
  cout << "Zeroed Proxys" << endl << flush;
  for (int v = 0; v < graph->nodes; v++) {
    uint32_t begin = graph->node_array[v];
    uint32_t end = graph->node_array[v+1];
    uint32_t degree = end-begin;
    float my_love = 1.0/degree;
    for (uint32_t i = begin; i < end; i++) {
        uint32_t w = graph->edge_array[i];
        in_r[w] += my_love;
        for (int i=0; i<PROXYS; i++){proxys[i][w] += my_love;}
    }
  }
  for (int v = 0; v < graph->nodes; v++) {
    in_r[v] = alphamult * in_r[v];
    for (int i=0; i<PROXYS; i++){proxys[i][v] = alphamult * proxys[i][v];}
  }
  cout << "Initialized Proxys" << endl << flush;
  
}

int task_init(int tX, int tY){
  routers[tX][tY]->output_q[C][0].enqueue(Msg(0, MONO,0) );
  return 1;
}

int task1_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  int64_t * global_params = &task_global_params[global(tX,tY)*num_global_params];
  int64_t startInd, endEdgeIndex;
  union float_int newR;

  int penalty = 2; // beq + ld
  load(1); 
  if (global_params[3]) {
      global_params[3] = 0;
      startInd = global_params[0];
      endEdgeIndex = global_params[1];
      newR.int_val = global_params[2];
      load(3); store(1); penalty+=4;
  } else {//not parked

      int64_t iterative_phase = global_params[4]; //LD
      load(1); penalty+=2; // ld, beq
      int node;
      Msg msg = IQ(0).peek(); //META
      if (iterative_phase>=0){
          msg = IQ(0).dequeue(); //MOV
          int local_node = msg.data; // Preloaded
          int node_base = nodePerTile*global(tX,tY);
          node = local_node + node_base; //ADD
          local_node++; // ADD
          store(1); penalty+=3;
          IQ(0).enqueue(Msg(local_node, MONO, timer+penalty)); //MOV
          load(1); penalty+=4; // LD, CMP, ST
          if (local_node > (nodePerTile-1) || node == (graph->nodes-1) ){ global_params[4] = -1; store(1); penalty+1;}
      } else {
          node = task3_dequeue(msg.data); //LD from Q
          load(1);penalty+=1; 
      }

      startInd = graph->node_array[node]; //LD
      endEdgeIndex = graph->node_array[node+1]; //LD
      // META: Remove these nodes
      if (startInd==endEdgeIndex){
        if (global_params[4] == -1){IQ(0).dequeue();}
        return 2; 
      }

      penalty+=check_dcache(tX,tY,graph->node_array,node,   timer+penalty, msg.time);
      penalty+=check_dcache(tX,tY,graph->node_array,node+1, timer+penalty, msg.time);
      penalty+=check_dcache(tX,tY,ret,node, timer+penalty, msg.time);
      penalty+=check_dcache(tX,tY,in_r,node,timer+penalty, msg.time);
      float val = in_r[node]; //LD
      ret[node] += val; // LD, OP, ST

      int64_t nodeDegree = (endEdgeIndex - startInd); // OP
      #if ASSERT_MODE
        assert(nodeDegree>0);
      #endif
      newR.float_val = val*alpha / (float)nodeDegree; // 2 op
      in_r[node]= 0.0; // ST

      store(2); penalty+=3;
  }
  penalty+=task1_helper(tX,tY,global_params, startInd, endEdgeIndex, newR.int_val, timer+penalty); //source distance
  if ((global_params[4] == -1) && (global_params[3]==0) ) IQ(0).dequeue();
  return penalty;
}

int task2_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  int penalty = 0;
  
  //1. Read start, end indices to edge_array and new_r.
  Msg msg = IQ(1).dequeue();
  u_int32_t start_index = task2_dequeue(msg.data);
  u_int32_t end_index   = IQ(1).dequeue().data;
  int new_R  = IQ(1).dequeue().data;
  load(3);
  #if ASSERT_MODE && STEAL_W == 1
    check_range(tX,tY,start_index,end_index);
  #endif

  //2. Push new_r and neoghbor to corresponding tile.
  u_int64_t time_fetched_edges = msg.time, time_prefetch_edges = msg.time; //META
  u_int64_t prefetch_edges_tag = cache_tag(graph->edge_array,start_index)+1; //META
  for (u_int32_t i=start_index; i<end_index; i++){
    u_int64_t neighbor = graph->edge_array[i];
    penalty += check_dcache(tX,tY,graph->edge_array,i,timer+penalty, time_fetched_edges, time_prefetch_edges, prefetch_edges_tag);

    u_int32_t startInd = graph->node_array[neighbor]; //META, cleanup dataset, no process empty node
    u_int32_t endEdgeIndex = graph->node_array[neighbor+1];  //META, cleanup dataset, do not count
    if (startInd<endEdgeIndex){
      // Invoke next task
      int dest = get_qid();
      OQ(dest).enqueue(Msg(getHeadFlit(dest, neighbor), HEAD,timer) );
      OQ(dest).enqueue(Msg(new_R, TAIL,timer) );
    }
  }
  // Instruction to configure a vector operation 
  // vector buffer_neighbor = out_vector(base=buffer_base, length=neighbor_len, stride=2)
  // vector buffer_new_R = out_vector(base=buffer_base+1, length=neighbor_len, stride=2)
  // vector edge_load = out_vector(base=neighbor_begin, length=neighbor_len, stride=1)
  // MOV buffer_neighbor = edge_load
  // MOV buffer_new_R = new_R
  // ASYNC MOV OQ = out_vector(base=buffer_base, length=neighbor_len, chain_length=2)
  u_int32_t vector_len = (end_index-start_index);
  numEdgesProcessed[tX/COLUMNS_PER_TH]+=vector_len; //META
  store(vector_len*2);
  return 7+penalty; // 3 queue loads + 3 vector configs, 2 vector ops of length (neighbor_len) + 1 MOV to OQ
}

int task3_kernel(int tX, int tY, u_int64_t timer, u_int64_t & compute_cycles){
  union float_int newR;
  Msg msg = IQ(2).dequeue();
  u_int32_t neighbor = task3_dequeue(msg.data);
  newR.int_val = IQ(2).dequeue().data;
  load(2);

  #if PROXY_FACTOR==1 && ASSERT_MODE
    u_int32_t node_base = nodePerTile*global(tX,tY);
    assert(neighbor>=node_base);
    u_int32_t nodeEnd = node_base+nodePerTile;
    assert(neighbor<nodeEnd);
  #endif

  int penalty = check_dcache(tX,tY,in_r, neighbor,timer, msg.time);
  float val_neighbor = in_r[neighbor];
  val_neighbor += newR.float_val; //ADDI [neighbor], newR
  in_r[neighbor] = val_neighbor; //STORE
  
  penalty+=3; //COMP, BEQ, JMP
  bool cond = (val_neighbor >= epsilon); 
  if (cond) {
    penalty+=add_to_frontier(tX, tY,  neighbor,timer);
    numFrontierNodesPushed[tX/COLUMNS_PER_TH]+=1; //Meta
  }
  return penalty;
}


int task3bis_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  union float_int newR;
  Msg msg = IQ(3).dequeue();
  u_int32_t neighbor = task3_dequeue(msg.data);
  newR.int_val = IQ(3).dequeue().data;
  load(2);

  // ADDI [neighbor], newR
  union float_int val_neighbor;
  val_neighbor.int_val = check_pcache(tX,tY,neighbor,timer);
  val_neighbor.float_val += newR.float_val; //ADD
  update_pcache(tX,tY, neighbor, val_neighbor.int_val); //STORE
  return 2;
}