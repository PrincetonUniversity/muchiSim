#include <iostream>
#include <assert.h>
#include <fstream>
#include <chrono>
#include <string>
#include "../common.h"

using namespace std;

csc_graph to_check;

void csc_print_binary_file(csc_graph to_print, string base) {

  ofstream num_nodes_edges;
  num_nodes_edges.open(base + "csc_num_nodes_edges.txt");
  
  if (!num_nodes_edges.is_open()) {
    assert(0);
  }

  num_nodes_edges << to_print.nodes << "\n";
  num_nodes_edges << to_print.edges << "\n";
  num_nodes_edges.close();

  ofstream node_array_file;
  node_array_file.open(base + "csc_node_array.bin", ios::out | ios::binary);
  
  if (!node_array_file.is_open()) {
    assert(0);
  }

  cout << "writing byte length of:    " << (to_print.nodes + 1) * sizeof(unsigned long) << endl;
  node_array_file.write((char *)to_print.node_array, (to_print.nodes + 1) * sizeof(unsigned long));
  
  node_array_file.close();

  ofstream edge_array_file;
  edge_array_file.open(base + "csc_edge_array.bin", ios::out | ios::binary);

  if (!edge_array_file.is_open()) {
    assert(0);
  }

  cout << "writing byte length of:    " << (to_print.edges) * sizeof(unsigned long) << endl;
  edge_array_file.write((char*)to_print.edge_array, to_print.edges * sizeof(unsigned long));

  edge_array_file.close();

  ofstream edge_values_file;
  edge_values_file.open(base + "csc_edge_values.bin", ios::out | ios::binary);
  if (!edge_values_file.is_open()) {
    assert(0);
  }

  edge_values_file.write((char*)to_print.edge_values, sizeof(int) * to_print.edges);
    
  edge_values_file.close();        
}

csc_graph csc_parse_bin_files(string base) {
  csc_graph ret;
  ifstream nodes_edges_file(base + "csc_num_nodes_edges.txt");
  unsigned long nodes, edges;

  auto start = chrono::system_clock::now();
  
  nodes_edges_file >> nodes;
  nodes_edges_file >> edges;
  nodes_edges_file.close();
  cout << "found " << nodes << " " << edges << "\n";
  
  ret.nodes = nodes;
  ret.edges = edges;
  ret.node_array = (unsigned long*) malloc(sizeof(unsigned long) * (ret.nodes + 1));
  ret.edge_array = (unsigned long*) malloc(sizeof(unsigned long) * (ret.edges));
  ret.edge_values = (int*) malloc(sizeof(int) * (ret.edges));
  
  cout << "done allocating" << endl;

  ifstream node_array_file;
  node_array_file.open(base + "csc_node_array.bin", ios::in | ios::binary);
  
  if (!node_array_file.is_open()) {
    assert(0);
  }

  cout << "reading byte length of:    " << (ret.nodes + 1) * sizeof(unsigned long) << endl;
  node_array_file.read((char *)ret.node_array, (ret.nodes + 1) * sizeof(unsigned long));

  node_array_file.close();

  ifstream edge_array_file;
  edge_array_file.open(base + "csc_edge_array.bin", ios::in | ios::binary);
  
  if (!edge_array_file.is_open()) {
    assert(0);
  }
  
  cout << "reading byte length of:    " << (ret.edges) * sizeof(unsigned long) << endl;
  edge_array_file.read((char*)ret.edge_array, (ret.edges) * sizeof(unsigned long));
  
  edge_array_file.close();

  ifstream edge_values_file;
  edge_values_file.open(base + "csc_edge_values.bin", ios::in | ios::binary);
  if (!edge_values_file.is_open()) {
    assert(0);
  }

  edge_values_file.read((char*)ret.edge_values, sizeof(int) * ret.edges);
    
  edge_values_file.close();        

  auto end = std::chrono::system_clock::now();
  chrono::duration<double> elapsed_seconds = end-start;
  cout << "Reading graph elapsed time: " << elapsed_seconds.count() << "s\n";


#define CHECK
#ifdef CHECK
  assert(to_check.nodes == ret.nodes);
  assert(to_check.edges == ret.edges);
  for (unsigned long i = 0; i < ret.nodes + 1; i++) {
    assert(ret.node_array[i] == to_check.node_array[i]);
  }

  for (unsigned long i = 0; i < ret.edges + 1; i++) {
    assert(ret.edge_array[i] == to_check.edge_array[i]);
    assert(ret.edge_values[i] == to_check.edge_values[i]);
  }

#endif

  return ret;  
}


int main(int argc, char** argv) {
  char *graph_fname, *base_name;
  csr_graph G_csr;
  csc_graph G;

  graph_fname = argv[1];

  G_csr = parse_bin_files(graph_fname);
  G = convert_csr_to_csc(G_csr);
  to_check = G;
  csc_print_binary_file(G, graph_fname);
  csc_parse_bin_files(graph_fname);

  return 0;
}
