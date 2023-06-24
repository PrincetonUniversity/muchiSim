
bool compare_out(string output_name){
  return graph->print_out(output_name);
}

void initialize_dataset_structures(){
}

void config_dataset(string dataset_filename){
  num_kernels = 4;
  graph = new graph_loader(dataset_filename);
}


void config_app(){
  proxy_default = 0;
  dataset_words_per_tile = nodePerTile; //ret_array 
  dataset_words_per_tile += edgePerTile; //edge index array
  dataset_words_per_tile += nodePerTile; //vertex index array
  dataset_words_per_tile += edgePerTile + nodePerTile; // edge_values and dense vector
}

std::vector <std::string> sep(std::string & line) {
  uint64_t idx = 0, start = 0, i;
  std::vector <std::string> tokens(3);
  for (i = 0; i < line.length(); i++){
    if (line[i] == ' '){
       tokens[idx++] = line.substr(start, i - start);
       start=i+1;
    }
  }
  tokens[2] = line.substr(start, i - start);
  return tokens;
}

std::vector<Edge> getVector(string filename, uint64_t tid, u_int64_t & penalty){
  std::ifstream file(filename);
  if (!file.is_open()) {
    printf("Tile %lu cannot open %s\n", tid, filename.c_str());
    exit(1);
  }
  struct stat stats;
  std::string line;
  stat(filename.c_str(), & stats);

  uint64_t num_bytes = stats.st_size / GRID_SIZE;
  uint64_t start = tid * num_bytes;
  uint64_t end = start + num_bytes;
  // cout << "start: " << start << " end: " << end << endl << flush;

  if (tid != 0) {
     file.seekg(start - 1);
     getline(file, line);
     // get to the next line
     if (line[0] != '\n') start += line.size();
  } else if (tid == GRID_SIZE-1){
    end = stats.st_size; // last tile gets the rest of the file
  } else {
      getline(file, line); // skip first line
  }
  penalty += 10;

  uint32_t src, dst, topic; double value;
  std::vector<Edge> subvector;
  
  while (start < end) {
      getline(file, line);
      uint64_t len = line.size() + 1;
      penalty += len;
      start += len;
      std::vector<std::string> tokens = sep(line);
      // cout << "tokens: " << tokens[0] << " " << tokens[1] << " " << tokens[2] << endl << flush;
      src = static_cast<uint32_t>(std::stoi(tokens[0]));
      dst = static_cast<uint32_t>(std::stoi(tokens[1]));
      value = std::stod(tokens[2]);
      subvector.push_back(Edge(src, dst, value));
      penalty += 10;
  }
  file.close();
  // cout << "TID " << tid << " has " << subvector.size() << " edges" << endl << flush;
  return subvector;
}

std::vector<std::vector<Edge>> edge_vectors(GRID_SIZE);

int read_file(int tX, int tY){
  u_int64_t penalty = 3;
  IQ(0).dequeue(); int tid = global(tX,tY);

  edge_vectors[tid] = getVector(graph->graph_name, tid, penalty);
  uint32_t start = tid * nodePerTile;
  uint32_t end = tid==(GRID_SIZE-1) ? graph->nodes : start + nodePerTile;

  // Initialize Vertex vectorID
  for (size_t i = start; i < end; i++) {
    graph->vertex_vector[i].id = UINT32_MAX;
    graph->vertex_vector[i].num_edges = 0;
    penalty += 4;
    omp_init_lock(&graph->locks[i]);
  }
  return penalty;
}

u_int32_t lookup(u_int32_t search_key, uint64_t & penalty){
    // Lookup Vertex based on hashing of src, or find empty slot
  u_int32_t index = search_key % graph->nodes;
  u_int32_t curr_key = graph->vertex_vector[index].id;
  penalty += 4;
  while (curr_key!= UINT32_MAX && curr_key != search_key) {
    index = (index + 1) % graph->nodes;
    curr_key = graph->vertex_vector[index].id;
    penalty += 4;
  }
  return index;
}
u_int32_t vertexs_cnt = 0;
int min_edges_per_vertex = 64;
int create_hashmap(int tX,int tY){
  uint64_t penalty = 3;
  IQ(0).dequeue(); int tid = global(tX,tY);
  Vertex * vertex_vector = graph->vertex_vector;
  
  uint64_t start = 0;
  uint64_t end = start + edge_vectors[tid].size();
  // LK; cout << "Thread " << global(tX,tY) << " start " << start << " end " << end << endl; ULK;
  for (size_t i = start; i < end; i++) {

    ////// SELLER //////
    u_int32_t search_key = edge_vectors[tid][i].src;
    u_int32_t index = lookup(search_key, penalty);
    // Lock the index
    omp_set_lock(&graph->locks[index]);
    // If empty slot, initialize Vertex
    if (vertex_vector[index].id == UINT32_MAX) {
      vertex_vector[index].id = search_key;
      vertex_vector[index].edges = (Edge*) malloc(sizeof(Edge) * 2);
      vertex_vector[index].edges[0] = edge_vectors[tid][i];
      vertex_vector[index].num_edges=1;
      #pragma omp atomic
      vertexs_cnt++;
      assert(vertexs_cnt < graph->nodes);

    } else if (vertex_vector[index].id == search_key) {
      vertex_vector[index].edges[vertex_vector[index].num_edges++] = edge_vectors[tid][i];
      int num = vertex_vector[index].num_edges;
      bool is_pow2 = (num & (num - 1)) == 0;
      if (is_pow2) {
        vertex_vector[index].edges = (Edge*) realloc(vertex_vector[index].edges, sizeof(Edge) * num * 2);
      }
    } else{assert(0);}
    penalty += 8;
    omp_unset_lock(&graph->locks[index]);

  } // end for edges
  return penalty;
}

int task_size=256;
int iterate(int tX, int tY, u_int64_t timer, u_int64_t & compute_cycles){
  int tile_base = nodePerTile*global(tX,tY);
  int vertex_base = tile_base + IQ(0).dequeue().data;

  int vertex_end = min(vertex_base+task_size, tile_base+nodePerTile);
  vertex_end = min(vertex_end, (int)(graph->nodes));
  int v = vertex_base;
  u_int64_t penalty = 2; // Loading the v and vertex_end
  u_int64_t edges_processed = 0;

  penalty+=2; // BEQ,JMP
  while(v < vertex_end){
    u_int32_t num_edges = get_dcache<u_int32_t>(tX,tY, &graph->vertex_vector[v].num_edges, penalty, timer);
    edges_processed+=num_edges; // METADATA
    cout << "Vertex " << v << " has " << num_edges << " edges" << endl;
    compute_cycles++;
    v++; penalty+=3; // ADD, BEQ, JMP
    numFrontierNodesPushed[tX/COLUMNS_PER_TH]++;
  }
  
  numEdgesProcessed[tX/COLUMNS_PER_TH] += edges_processed;
  return penalty;
}


int iter_per_tile = 1;
u_int64_t iterate2(int tX, int tY){
  IQ(0).dequeue(); u_int32_t tid = global(tX,tY);
  u_int64_t penalty = 2;
  u_int32_t start = tid * iter_per_tile;
  u_int32_t end = start + iter_per_tile;

  for (uint32_t i = start; i < end; i++) {
    cout << "Thread " << tid << " iter " << i << endl;
    penalty += 1;
  }

  return penalty;
}

void add_many_tasks(int tX, int tY){
  int vertex_per_th = nodePerTile/(smt_per_tile);
  if (vertex_per_th < 16) task_size = vertex_per_th;
  int tasks = nodePerTile/task_size;

  for (int i = 0; i < tasks; i++){
    routers[tX][tY]->output_q[C][0].enqueue(Msg(task_size*i, MONO,0) );
  }
  if (task_size*tasks < nodePerTile){
    routers[tX][tY]->output_q[C][0].enqueue(Msg(task_size*tasks, MONO,0) );
  }
}

void task_init(int tX, int tY){
  if (epoch_counter<3){
    routers[tX][tY]->output_q[C][0].enqueue(Msg(0, MONO,0) );
  } else if (epoch_counter==3){
    add_many_tasks(tX,tY);
  } else if (epoch_counter==4){
    routers[tX][tY]->output_q[C][0].enqueue(Msg(0, MONO,0) );
  }
}

int task1_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  // LK; cout << "barrier_index: " << epoch_counter << endl << flush; ULK;
  if (epoch_counter==1) return read_file(tX,tY);
  if (epoch_counter==2) return create_hashmap(tX,tY);
  if (epoch_counter==3) return iterate(tX,tY, timer, compute_cycles);
  if (epoch_counter==4) return iterate2(tX,tY);
  return 1;
}

int task2_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  return 2;
}

// routing based on return array (vertex array)
int task3_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  return 2;
}

int task3bis_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  return 2;
}

