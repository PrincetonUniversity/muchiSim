using namespace std;
#define APP 1
#define SHUFFLE 0
#include "../src/common/macros.h"
// System Configuration Parameters
#include "../src/configs/config_system.h"
#include "../src/configs/config_queue.h"
// Global structures and defined parameters
#include "../src/common/global.h"
#include "../src/mem/memory_util.h"
#include "../src/dataset_loaders/graph_loader.h"
graph_loader * graph;

void _kernel_(float alpha, float epsilon, int id, int core_num) {
  u_int64_t nodes_explored = 0;
  u_int64_t edges_explored = 0;
  uint32_t fr_len = 0;
  uint32_t new_fr_len = 0;
  
  uint32_t * in_wl = (uint32_t *) malloc(sizeof(uint32_t) * graph->nodes * 5);
  uint32_t * out_wl = (uint32_t *) malloc(sizeof(uint32_t) * graph->nodes * 5);
  uint32_t N = graph->nodes;  
  float * in_r = (float *) calloc(N, sizeof(float));
  float * out_r = (float *) calloc(N, sizeof(float));
  bool * bitmap = (bool *) calloc(graph->nodes, sizeof(bool) );
  uint32_t count = 1;

  // Initialize in_r state
  cout << "Epoch #"<<count++<<"\n";
  for (uint32_t v = 0; v < N; v++) {
    uint32_t num_neighbors = graph->node_array[v+1]-graph->node_array[v];
    float my_take = 1.0/num_neighbors;
    for (uint32_t i = graph->node_array[v]; i < graph->node_array[v+1]; i++) {
        uint32_t neighbor = graph->edge_array[i];
        in_r[neighbor] += my_take;
    }
    in_wl[fr_len++] = v;
  }

  for (uint32_t v = 0; v < N; v++) {
    in_r[v] = (1 - alpha) * alpha * in_r[v];
    out_r[v] = 0.0;
  }
  
  while (fr_len > 0) {
    cout << "Epoch #"<<count++<<"\n";
    for (uint32_t i = id; i < fr_len; i+=core_num){
      uint32_t node = in_wl[i];
      ret[node] += in_r[node];
      nodes_explored++;
      uint32_t end = graph->node_array[node+1];
      uint32_t begin = graph->node_array[node];
      uint32_t num_neighbors = end-begin;
      float new_r = in_r[node]*alpha/num_neighbors;
      edges_explored+=num_neighbors; //META
      in_r[node]= 0.0;

      for (uint32_t k = begin; k < end; k++) {
        u_int32_t neighbor = graph->edge_array[k];
        float r_old = out_r[neighbor];
        out_r[neighbor] += new_r;
        if ((out_r[neighbor] >= epsilon) && (r_old < epsilon)) {
          #if COALESCE==1
          if (!bitmap[neighbor]){
            out_wl[new_fr_len++] = neighbor;
            bitmap[neighbor]=true;
          }
          #else
          out_wl[new_fr_len++] = neighbor;
          #endif
        }
      } 
      
    }

    // Swap frontier lists
    uint32_t *tmp = out_wl;
    out_wl = in_wl;
    in_wl = tmp;
    fr_len = new_fr_len;
    new_fr_len = 0;
    for (uint32_t i=0; i<graph->nodes; i++){
      bitmap[i]=false;
      in_r[i]=out_r[i];
      // If it was added to the frontier
      if (out_r[i]>=epsilon){
        out_r[i] = 0;
      }
    }    
  }
  free(in_wl); free(out_wl); free(bitmap);
  cout << "\nK Nodes Processed: " << nodes_explored/1000 << "\n";
  cout << "K Edges Processed: " << edges_explored/1000 << "\n";
  cout << "\nNode Re-explore factor: " << (double)nodes_explored/graph->nodes << "\n";
  cout << "Edge Re-explore factor: " << (double)edges_explored/graph->edges << "\n";
}

int main(int argc, char** argv) {

  char* output_name = NULL;
  string graph_fname;
  if (argc >= 2) graph_fname = argv[1];
  else graph_fname = "datasets/Kron16/";

  if (argc >= 3) output_name = argv[2];
  
  graph = new graph_loader(graph_fname, 1);
  float alpha = 0.85;
  float epsilon = 0.01;
  ret = (float *) calloc(graph->nodes, sizeof(float));

  // Kernel initialization
  for (uint32_t v = 0; v < graph->nodes; v++) {
    ret[v] = 1 - alpha;
  }
  
  // Kernel execution
  printf("\n\nstarting kernel\n");
  auto start = chrono::system_clock::now();

  _kernel_(alpha, epsilon, 0 , 1);

  printf("\nending kernel");
  auto end = std::chrono::system_clock::now();
  chrono::duration<float> elapsed_seconds = end-start;
  cout << "\nkernel computation time: " << elapsed_seconds.count() << "s\n";
  
  if (output_name!=NULL){
    graph->output_result(output_name);
  }
  
  free(ret);
  delete graph;
  return 0;
}
