#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <climits>

const u_int32_t B_numcols = 64;

class graph_loader {
public:
  string graph_name;
  uint32_t nodes;
  int64_t edges;
  uint64_t edges_traversed = 0;
  uint32_t avg_degree;
  uint64_t max_degree = 0;
  uint64_t max_vertex;
  uint32_t *node_array;
  uint32_t *edge_array;
  uint32_t *edge_values;
  uint32_t *dense_vector=NULL;
  uint32_t dense_value = 5;
  
  graph_loader(string base, int binary);
  ~graph_loader();
  void shuffle_dataset(uint32_t* node_array_pre, uint32_t* edge_array_pre, uint32_t* edge_values_pre);
  bool output_result(char* output_name);
  bool compare_out_pagerank(string output_name);
  bool compare_out_histo();
  bool output_edges_traversed(char* output_name, uint64_t edges_traversed);
  uint64_t read_edges_traversed(string name);
  uint32_t build_nodes_edges(uint32_t orig_nodes);
};

graph_loader::~graph_loader() {
  free(node_array);
  free(edge_array);
  if (dataset_has_edge_val) free(edge_values);
}

bool graph_loader::compare_out_pagerank(string output_name){
  if (graph_name != "datasets/Kron16/") return true;
  fstream reader(output_name,fstream::in);
  if(reader) {
    cout << "Open Output succesfully\n";
  } else {
    cout << "Couldn't open the Output file "<<output_name<<"\n"; 
    exit(1);
  }

  float weight;
  uint32_t adv=1;
  #if SHUFFLE
    adv=nodes/GRID_SIZE;
  #endif
  uint32_t node=0;
  for (uint32_t k=0; k<adv; k++){
    for (uint32_t i=k; i<nodes; i+=adv){
      reader >> weight;
      float diff = ret[i] - weight;
      if (diff > 0.01){
        cout <<"Error Vertex "<<node<<" : "<<ret[i]<<" > "<<weight<<"\n";
        return false;
      }
      node++;
    }
  }
  cout << "Checked Pagerank succesfully\n";
  return true;
}

bool graph_loader::compare_out_histo(){
  if (graph_name != "datasets/Kron16/") return true;
  // Calculate histogram of graph->edge_array values
  uint32_t * histo_ref = (uint32_t *) calloc(nodes, sizeof(uint32_t));
  for (uint64_t i=0; i<edges; i++){
    histo_ref[edge_array[i]]+=1;
  }
  for (uint64_t i=0; i<nodes; i++){
    if (ret[i] != histo_ref[i]){
      cout << "Error at "<<i<<" : "<<ret[i]<<" != "<<histo_ref[i]<<"\n";
      return false;
    }
  }
  free(histo_ref);
  cout << "Checked histogram succesfully\n";
  return true;
}

bool graph_loader::output_edges_traversed(char* output_name, uint64_t edges_trav){
  ofstream outfile;
  string name = output_name;
  outfile.open (name + "bfs_edges_traversed.txt");
  if (!outfile.is_open()) {
    cout << "Could not open edges file" << endl;
    return false;
  }
  outfile << edges_trav << "\n";
  outfile.close();
  return true;
}

bool graph_loader::output_result(char* output_name){
  u_int64_t ret_len = nodes;
  #if APP==SPMM
    ret_len *= B_numcols;
  #endif
  #if PROXYS>1 && (DRAIN_PROXY==0 || PCACHE==0)
    //aggregate values of proxy arrays
    for (u_int32_t j=0; j<PROXYS; j++){
      for (uint64_t i=0; i<ret_len; i++){
        ret[i] += proxys[j][i];
      }
    }
  #endif

  ofstream outfile;
  outfile.open (output_name);
  if (!outfile.is_open()) {
    cout << "Could not open output result file" << endl;
    return false;
  }

  uint32_t adv=1;
  #if SHUFFLE
    adv=ret_len/GRID_SIZE;
  #endif
  for (uint32_t k=0; k<adv; k++){
    #if APP==SPMM
      u_int32_t inc=B_numcols;
    #else
      u_int32_t inc=adv;
    #endif
    for (uint32_t i=k; i<ret_len; i+=inc){
      outfile << ret[i] << "\n";
    }
  }
  outfile.close();
  // cout << "Output file created succesfully\n";
  return true;
}


void writearray(string name, uint32_t * array, int64_t write_len){
  ofstream outfile;
  outfile.open (name, ios::out | ios::binary | ios::trunc);
  if (!outfile.is_open()) {
    cout << "Could not open dest file" << endl;
    assert(0);
  }
  outfile.write((char*)array, write_len);
  outfile.close();
}

uint64_t graph_loader::read_edges_traversed(string name){
  uint64_t edges_trav;

  fstream reader(name + "bfs_edges_traversed.txt",fstream::in);
  if(reader) {
    cout << "Open Edge Count File succesfully\n";
  } else {
    cout << "Couldn't open the Count File "<<name<<"\n"; 
    exit(1);
  }
  reader >> edges_trav;
  reader.close();

  return edges_trav;
}

uint32_t* readarray(string name, int64_t read_len, int64_t malloc_len,bool is64bit){
  ifstream array_file;
  array_file.open(name, ios::in | ios::binary);
  if (!array_file.is_open()) {
    cout << "No array found" << endl;
    assert(0);
  }
  cout << "Array "<< name << " found" << endl;
  int64_t malloc_size,read_length;
  void * array;
  if (is64bit){
    malloc_size = sizeof(uint64_t) * malloc_len;
    read_length = sizeof(uint64_t) * read_len;
    array = (uint64_t*) malloc(malloc_size);
  } else {
    malloc_size = sizeof(uint32_t) * malloc_len;
    read_length = sizeof(uint32_t) * read_len;
    array = (uint32_t*) malloc(malloc_size);
  }
  
  cout << "Malloc array of size " << malloc_size << " bytes" << endl;
  cout << "Readin array of size " << read_length << " bytes" << endl;
  array_file.read((char *)array, read_length);
  array_file.close();
  cout << "Finished Reading Array Bytes" << endl;

  if (is64bit){
    uint64_t* array_64 = (uint64_t*)array;
    malloc_size = sizeof(uint32_t) * malloc_len;
    uint32_t* array_32 = (uint32_t*) malloc(malloc_size);
    for (uint64_t i = 0; i < malloc_len; i++) {
      assert(array_64[i] < UINT32_MAX);
      array_32[i] = array_64[i];
    }
    free(array);
    cout << "Finished Reordering Array 32bit" << endl;
    return array_32;
  }
  cout << "No reordering Array 32bit" << endl;
  return (uint32_t*)array;
}

void graph_loader::shuffle_dataset(uint32_t* node_array_pre, uint32_t* edge_array_pre, uint32_t* edge_values_pre)
{
  uint32_t node_ptr = 0;
  uint64_t edge_ptr = 0;
  uint32_t round_len = nodes/GRID_SIZE;
  
  for (uint32_t k=0; k<GRID_SIZE; k++){
    // as many as total_nodes/GRID_SIZE = round_len
    for (uint32_t i=k; i<nodes; i+=GRID_SIZE){
      uint32_t round_count = i & GRID_SIZEm1;
      uint32_t idx_in_round = i >> GRID_SIZE_LOG2;
      uint32_t new_idx = round_count*round_len + idx_in_round;

      uint32_t begin = node_array_pre[i];
      uint32_t end = node_array_pre[i+1];
      uint64_t len = (end - begin);
      #if ASSERT_MODE
        assert(end>=begin);
        uint64_t sum = edge_ptr + len;
        assert(sum < UINT32_MAX);
        assert(round_count==k);
        // if (len>max_degree){max_degree=len; max_vertex=i;}
        assert(node_ptr==new_idx);
      #endif

      for (uint32_t j=0; j<len; j++){
        uint32_t old_node = edge_array_pre[begin+j];
        uint32_t new_node = (old_node & GRID_SIZEm1)*round_len + (old_node >> GRID_SIZE_LOG2);
        edge_array[edge_ptr+j] = new_node;
        if (dataset_has_edge_val) edge_values[edge_ptr+j] = edge_values_pre[begin+j];
      }
      node_array[node_ptr++] = edge_ptr;
      edge_ptr+=len;
    }
  }

  // cout << "Max Degree " << max_degree << endl;
  // cout << "Max Vertex " << max_vertex << endl;
  cout << "\nDone converting "<< edge_ptr<<" edges\n";
  cout << "Reading "<< node_ptr <<" node_ptr\n\n\n" << std::flush;

  node_array[node_ptr] = edge_ptr;
  assert(node_ptr==nodes);
  free(node_array_pre); free(edge_array_pre);
  if (dataset_has_edge_val) free(edge_values_pre);
}

uint32_t graph_loader::build_nodes_edges(uint32_t orig_nodes){
  // Making #nodes to be multiple of GRID_SIZE, filling with empty nodes
  uint32_t padded_nodes = (GRID_SIZE-(orig_nodes%GRID_SIZE)) % GRID_SIZE;
  nodes = orig_nodes + padded_nodes;
  assert(nodes%GRID_SIZE==0);

  cout << "Found Vertices: " << nodes << ", Edges: " << edges << "\n";

  // Average degree
  // Whether the dataset stores node_idx as a 64bit or 32bit integer
  avg_degree = ((edges-1) / nodes) + 1;
  assert(avg_degree>0);
  assert(edges <= UINT_MAX);
  cout << "avg degree " <<avg_degree<<"\n";

  return padded_nodes;
}

graph_loader::graph_loader(string base, int binary){
  graph_name = base;
  auto start = chrono::system_clock::now();

  uint32_t orig_nodes;
  if (binary){
    edges_traversed = read_edges_traversed(base);
    cout << "Edges Traversed BFS: " << edges_traversed << endl;
    ifstream nodes_edges_file(base + "num_nodes_edges.txt", ios::in );
    nodes_edges_file >> orig_nodes >> edges;
    nodes_edges_file.close();

    uint32_t padded_nodes = build_nodes_edges(orig_nodes);

    bool read_shuffle = false;
    string node_array_name, edge_array_name, edge_values_name;
    if (read_shuffle){
      node_array_name = base + "node_array_shuf.bin";
      edge_array_name = base + "edge_array_shuf.bin";
      edge_values_name = base + "edge_values_shuf.bin";
    } else {
      node_array_name = base + "node_array.bin";
      edge_array_name = base + "edge_array.bin";
      edge_values_name = base + "edge_values.bin";
    }

    bool is64bit = false;//(nodes >=134217728);
    uint32_t *node_array_pre; uint32_t * edge_array_pre; uint32_t *edge_values_pre;
    node_array_pre = readarray(node_array_name, orig_nodes+1, nodes+1, is64bit);

    for(int i=1; i<=padded_nodes;i++) node_array_pre[orig_nodes+i] = node_array_pre[orig_nodes];

    edge_array_pre = readarray(edge_array_name, edges,edges, is64bit);
    uint64_t malloc_edge_size = sizeof(uint32_t) * edges;
    if (dataset_has_edge_val) edge_values_pre =  (uint32_t*) readarray(edge_values_name, edges,edges,false);

    #if SHUFFLE
      if (dataset_has_edge_val) edge_values = (uint32_t*) malloc(malloc_edge_size);
      node_array = (uint32_t*) malloc(sizeof(uint32_t) * nodes+1);
      edge_array = (uint32_t*) malloc(malloc_edge_size);
      shuffle_dataset(node_array_pre, edge_array_pre, edge_values_pre);

    #else // No shuffle
      node_array = node_array_pre;
      edge_array = edge_array_pre;
      edge_values = edge_values_pre;
    #endif
    

  } else{ // NOT BINARY: Tab Separated Value (TSV)

    //remove last / if exists
    if (base[base.size()-1]=='/') base.erase(base.size()-1);
    base += ".tsv";
    fstream reader(base,fstream::in);
    if(reader) {
      cout << "Open Graph succesfully\n";
      cout << "Graph: " << base << "\n";
    }
    else {
      cout << "Couldn't open the graph file "<<base<<"\n"; 
      exit(1);
    }
    // Read header of the graph CSV file
    char comment;
    reader >> comment >> edges >> orig_nodes;

    uint32_t padded_nodes = build_nodes_edges(orig_nodes);
    cout << "Original Nodes: " << orig_nodes << endl;
    cout << "Padded Nodes: " << padded_nodes << endl;
    cout << "Edges: " << edges << endl;

    node_array = (uint32_t*) malloc(sizeof(uint32_t) * (nodes + 1));
    edge_array = (uint32_t*) malloc(sizeof(uint32_t) * (edges));
    if (dataset_has_edge_val) edge_values = (uint32_t*) malloc(sizeof(uint32_t) * (edges));

    uint32_t node_ptr = 0;
    node_array[0] = 0;
    uint32_t vertex, edge_idx, edge_value;
    double edge_value_d;

    uint64_t prev_idx = 0;
    for(uint32_t i = 0; i < edges; i++ ) {
      reader >> vertex >> edge_idx >> edge_value_d;
      // Convert the edge values to integers
      edge_value = (uint32_t) abs(edge_value_d*1000);
      //cout << "Vertex " << vertex << ", idx " << edge_idx << ", value " << edge_value << endl;
      while (node_ptr != vertex) {
        #if ASSERT_MODE
          uint64_t len = (i - prev_idx);
          prev_idx = i;
          if (len>max_degree){max_degree=len; max_vertex=node_ptr;}
        #endif
        node_array[++node_ptr] = i;
      }

      edge_array[i] = edge_idx;
      if (dataset_has_edge_val) edge_values[i] = edge_value;
    }
    node_array[++node_ptr] = edges;

    for(int i=1; i<=padded_nodes;i++) node_array[orig_nodes+i] = node_array[orig_nodes];

    cout << "Max Degree " << max_degree << endl;
    cout << "Max Vertex " << max_vertex << endl;
    reader.close();
    printf("reading %% 100.00 finished\n");
  } // END OF IF FORMAT TYPE

  assert(nodes>=(2*GRID_SIZE));

  nodePerTile = (nodes+GRID_SIZE-1)/GRID_SIZE;
  edgePerTile  = (edges+GRID_SIZE-1)/GRID_SIZE;

  auto end = std::chrono::system_clock::now();
  chrono::duration<double> elapsed_seconds = end-start;
  cout << "Reading graph elapsed time: " << elapsed_seconds.count() << "s\n\n";

}
