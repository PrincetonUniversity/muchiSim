#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <climits>

class graph_loader {
public:
  string graph_name;
  uint32_t nodes;
  int64_t edges;
  uint64_t edges_traversed = 0;
  uint32_t *node_array;
  uint32_t *edge_array;
  uint32_t *edge_values;
  uint32_t *dense_vector;
  
  graph_loader(string base, int binary);
  ~graph_loader();
  bool compare_out(string output_name,void* ret);
  uint32_t build_nodes_edges(uint32_t orig_nodes);
};

graph_loader::~graph_loader() {
  free(node_array);
  free(edge_array);
  if (dataset_has_edge_val) free(edge_values);
}

bool graph_loader::compare_out(string output_name, void * ret){
  return true;
}

uint32_t graph_loader::build_nodes_edges(uint32_t orig_nodes){
  // Making #nodes to be multiple of GRID_SIZE, filling with empty nodes
  uint32_t padded_nodes = (GRID_SIZE-(orig_nodes%GRID_SIZE)) % GRID_SIZE;
  nodes = orig_nodes + padded_nodes;
  assert(nodes%GRID_SIZE==0);
  cout << "Found Vertices: " << nodes << ", Edges: " << edges << "\n";
  return padded_nodes;
}

graph_loader::graph_loader(string base){
  graph_name = base;
  base = base+"Kron16.tsv";
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
  uint32_t orig_nodes;
  reader >> comment >> edges >> orig_nodes;

  uint32_t padded_nodes = build_nodes_edges(orig_nodes);
  node_array = (uint32_t*) malloc(sizeof(uint32_t) * (nodes + 1));
  edge_array = (uint32_t*) malloc(sizeof(uint32_t) * (edges));
  if (dataset_has_edge_val) edge_values = (uint32_t*) malloc(sizeof(uint32_t) * (edges));

  uint32_t node_ptr = 0;
  node_array[0] = 0;
  uint32_t vertex, edge_idx, edge_value;
  
  for(uint32_t i = 0; i < edges; i++ ) {
    reader >> vertex >> edge_idx >> edge_value;
    while (node_ptr != vertex) node_array[++node_ptr] = i;

    edge_array[i] = edge_idx;
    if (dataset_has_edge_val) edge_values[i] = edge_value;
  }
  node_array[++node_ptr] = edges;

  for(int i=1; i<=padded_nodes;i++) node_array[orig_nodes+i] = node_array[orig_nodes];

  reader.close();
  printf("reading %% 100.00 finished\n");

  nodePerTile = (nodes+GRID_SIZE-1)/GRID_SIZE;
  edgePerTile  = (edges+GRID_SIZE-1)/GRID_SIZE;

}