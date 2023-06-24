#include "stdio.h"
#include "stdlib.h"
#include "assert.h"
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <climits>
#include <limits>
#include <sys/stat.h>
#include <vector>
#include <utility>
#include <algorithm>
#include <random>

struct Edge {
    uint32_t src;
    uint32_t dst;
    double value;

    // Constructor
    Edge(uint32_t v, uint32_t n, double a) : src(v), dst(n), value(a) {}
    // Empty constructor
    Edge() : src(UINT32_MAX) {}

    // Operator < for sorting
    bool operator<(const Edge& other) const {
        return std::tie(src, dst) < std::tie(other.src, other.dst);
    }
};

struct Vertex {
    uint32_t id;
    Edge* edges;
    uint32_t num_edges;
};


class graph_loader {
public:
  string graph_name;
  uint32_t nodes;
  int64_t edges;
  uint64_t edges_traversed;
  Vertex* vertex_vector;
  omp_lock_t * locks;
  
  graph_loader(string base);
  ~graph_loader();
  bool print_out(string output_name);
};

graph_loader::~graph_loader() {
  delete[] vertex_vector;
}

bool graph_loader::print_out(string output_name){
  int check = 1;
  if (check==1){
    ofstream outfile;
    outfile.open (output_name);
    if (!outfile.is_open()) {
      cout << "Could not open result file" << endl;
      return false;
    }

    for (u_int32_t i=0; i<nodes; i++) {
      if (vertex_vector[i].id != UINT32_MAX) {
        for (u_int32_t j=0; j<vertex_vector[i].num_edges; j++) {
          outfile << vertex_vector[i].edges[j].src <<","<< vertex_vector[i].edges[j].dst <<","<<vertex_vector[i].edges[j].value << "\n";
        }
      }
    }
    outfile.close();
  }
  return true;
}

graph_loader::graph_loader(string base){
  graph_name = base+"Kron16.tsv";
  nodes=(65536/GRID_SIZE)*GRID_SIZE*1;
  edges=955864;
  vertex_vector = (Vertex*) malloc(nodes * sizeof(Vertex));
  locks = new omp_lock_t[nodes];

  nodePerTile = (nodes+GRID_SIZE-1)/GRID_SIZE;
  edgePerTile  = (edges+GRID_SIZE-1)/GRID_SIZE;
  cout << "nodePerTile: " << nodePerTile << ", edgePerTile: " << edgePerTile << "\n";
}
