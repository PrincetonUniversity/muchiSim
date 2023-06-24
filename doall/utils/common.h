#include <iostream>
#include <assert.h>
#include <fstream>
#include <chrono>
#include <string>

using namespace std;

csr_graph parse_bin_files(string base) {
  csr_graph ret;
  ifstream nodes_edges_file(base + "num_nodes_edges.txt");
  unsigned int nodes, edges;
  auto start = chrono::system_clock::now();
  
  nodes_edges_file >> nodes;
  nodes_edges_file >> edges;
  nodes_edges_file.close();
  cout << "found " << nodes << " " << edges << "\n";
  
  ret.nodes = nodes;
  ret.edges = edges;
  ret.node_array = (unsigned int*) malloc(sizeof(unsigned int) * (ret.nodes + 1));
  ret.edge_array = (unsigned int*) malloc(sizeof(unsigned int) * (ret.edges));
  ret.node_data = (int*) malloc(sizeof(int) * (ret.edges));

  ifstream node_array_file;
  node_array_file.open(base + "node_array.bin", ios::in | ios::binary);
  
  if (!node_array_file.is_open()) {
    cout << "no node array file" << endl;
    assert(0);
  }

  node_array_file.seekg (0, node_array_file.end);
  long int length = node_array_file.tellg();
  node_array_file.seekg (0, node_array_file.beg);
  cout << "byte length of node array: " << length << endl;
  cout << "reading byte length of:    " << (ret.nodes + 1) * sizeof(unsigned int) << endl;

  node_array_file.read((char *)ret.node_array, (ret.nodes + 1) * sizeof(unsigned int));
  node_array_file.close();
  ret.node_array[0] = 0;

  ifstream edge_array_file;
  edge_array_file.open(base + "edge_array.bin", ios::in | ios::binary);
  
  if (!edge_array_file.is_open()) {
    cout << "no edge array file" << endl;
    assert(0);
  }
  
  edge_array_file.seekg (0, edge_array_file.end);
  length = edge_array_file.tellg();
  edge_array_file.seekg (0, edge_array_file.beg);
  cout << "byte length of edge array: " << length << endl;
  cout << "reading byte length of:    " << (ret.edges) * sizeof(unsigned int) << endl;
  
  edge_array_file.read((char*)ret.edge_array, (ret.edges) * sizeof(unsigned int));
  edge_array_file.close();

  ifstream edge_values_file;
  edge_values_file.open(base + "edge_values.bin", ios::in | ios::binary);
  if (!edge_values_file.is_open()) {
    cout << "no edge values file" << endl;
    assert(0);
  }

  edge_values_file.read((char*)ret.node_data, sizeof(int) * ret.edges); 
  edge_values_file.close();        

  auto end = std::chrono::system_clock::now();
  chrono::duration<double> elapsed_seconds = end-start;
  cout << "Reading graph elapsed time: " << elapsed_seconds.count() << "s\n";

  return ret;  
}
