bool compare_out(char* output_name){
  bool result_correct = graph->output_result(output_name);
  result_correct &= graph->compare_out_histo();
  return result_correct;
}

void initialize_dataset_structures(){
  initialize_proxys(); cout << "proxys initialized\n"<<flush;

  ret = (int *) malloc(sizeof(int) * graph->nodes);
  for (int i = 0; i < graph->nodes; i++) ret[i] = 0;
}

void config_dataset(string dataset_filename){
  dataset_has_edge_val = 0;
  graph = new graph_loader(dataset_filename,1);
}

void config_app(){
  proxy_default = 0;
  u_int32_t t3b_piped_tasks = 1;
  u_int32_t t3_piped_tasks  = 8 * smt_per_tile;
  #if TEST_QUEUES>0
    t3_piped_tasks *= (1 << (TEST_QUEUES-1));
  #else
    t3_piped_tasks *= 4;
  #endif

  #if PROXY_FACTOR>1 //proxys
    t3b_piped_tasks = t3_piped_tasks;
    t3_piped_tasks  = PROXY_ROUTING==1 ? unused_buffer : t3_piped_tasks / proxy_shrink;
  #endif

  task1_dest = dest_qid;
  max_task_chunk = LOOP_CHUNK;

  iq_sizes[1] = unused_buffer;
  oq_sizes[1] = unused_buffer;
  oq_sizes[2] = t3_piped_tasks  * num_task_params[2];
  iq_sizes[2] = oq_sizes[2] * io_factor_t3;
  oq_sizes[3] = t3b_piped_tasks * num_task_params[3];
  iq_sizes[3] = oq_sizes[3] * io_factor_t3b;

  dataset_words_per_tile = nodePerTile; //ret_array 
  dataset_words_per_tile += edgePerTile; //edge index array
}

int task_init(int tX, int tY){
  routers[tX][tY]->output_q[C][0].enqueue(Msg(0, MONO,0) );
  return 1;
}

int task1_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  int penalty = 0;
  bool stop = false;
  Msg msg = IQ(0).dequeue();

  // In Task SW we don't need to add the base as index would be local to the tile
  int64_t base = (int64_t)edgePerTile * global(tX,tY);
  int64_t end = min( (base + edgePerTile - 1), (graph->edges - 1) );
  int64_t elem = msg.data + base; //LD, ADD

  u_int64_t time_fetched = msg.time, time_prefetch = msg.time; //META
  u_int64_t prefetch_tag = cache_tag(graph->edge_array,elem)+1; //META

  load_mem_wait(2); //LD index, LD end

  penalty+=2; //BEQ, JMP
  int iter = 1;
  while(!stop){
    penalty+=1; //EQ

    u_int64_t bucket = graph->edge_array[elem]; flop(1);
    penalty+=check_dcache(tX,tY,graph->edge_array,elem, timer+penalty, time_fetched, time_prefetch, prefetch_tag);
    
    // stop if it's the last element
    stop = (elem++ == end) || (iter++ == max_task_chunk);
    
    int dest = dest_qid;
    #if TASK3_DIRECTLY_IF_NETWORK_EMPTY
    if (OQ(2).occupancy()==0){dest = 2;}
    #endif

    OQ(dest).enqueue(Msg(getHeadFlit(dest, bucket), HEAD, timer+penalty) );
    OQ(dest).enqueue(Msg(1, TAIL, timer+penalty) );
    penalty+=2; //ADD, MOD, MOV

    stop |= (OQ(dest_qid).capacity() < 2);
    load_mem_wait(1); // Load capacity of OQ
    penalty+=1; // OR
  }

  if (elem <= end){IQ(0).enqueue(Msg(elem - base, MONO,timer+penalty));penalty+=1;}
  penalty+=4; store(1);//BEQ,JMP, SUB, ST
  
  return penalty;
}

int task2_kernel(int tX,int tY , u_int64_t timer, u_int64_t & compute_cycles){
  return 1;
}

int task3_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  Msg msg = IQ(2).dequeue();
  u_int32_t bucket = task3_dequeue(msg.data);
  u_int32_t update = IQ(2).dequeue().data;

  int penalty = check_dcache(tX,tY,ret,bucket,timer, msg.time);
  // ADDI [bucket], 1
  ret[bucket]+=update;
  store(1);

  numFrontierNodesPushed[tX/COLUMNS_PER_TH]+=1; //Meta
  return penalty+1; //Task + RMW
}

int task3bis_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  Msg msg = IQ(3).dequeue();
  u_int32_t bucket = task3_dequeue(msg.data);
  u_int32_t update = IQ(3).dequeue().data;

  int penalty = 0;
  int ret_bucket = check_pcache(tX,tY,bucket,penalty,timer);
  // ADDI [bucket], 1
  ret_bucket+=update;
  update_pcache(tX,tY,bucket,ret_bucket);

  numFrontierNodesPushed[tX/COLUMNS_PER_TH]+=1; //Meta
  return penalty+1; //Task + RMW
}

