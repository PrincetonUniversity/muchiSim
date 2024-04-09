using namespace std;

#include "../src/common/macros.h"
// System Configuration Parameters
#include "../src/configs/config_system.h"
#include "../src/configs/config_queue.h"
// Global structures and defined parameters
#include "../src/common/global.h"
#include "../src/mem/memory_util.h"
#include "../src/dataset_loaders/graph_loader.h"
graph_loader * graph;

#define COALESCE 1

u_int64_t _kernel_(u_int32_t id, u_int32_t core_num){
  u_int64_t nodes_explored = 0;
  u_int64_t edges_explored = 0;
  u_int64_t fr_len = 0;
  u_int64_t new_fr_len = 0;
  
  bool * bitmap = (bool *) calloc(graph->nodes, sizeof(bool) );
  u_int64_t * in_wl = (u_int64_t *) malloc(sizeof(u_int64_t) * graph->nodes * 2);
  u_int64_t * out_wl = (u_int64_t *) malloc(sizeof(u_int64_t) * graph->nodes * 2);
  fr_len = 1;
  in_wl[0] = 0;
  int count = 1;
  while (fr_len > 0) {
    cout << "Epoch #"<<count++<<"\n";
    for (u_int64_t i = id; i < fr_len; i+=core_num){
      u_int64_t node = in_wl[i];
      nodes_explored++;
      u_int32_t begin = graph->node_array[node];
      u_int32_t end = graph->node_array[node+1];
      edges_explored+=(end-begin);
      for (u_int64_t k = begin; k < end; k++){
        u_int32_t neighbor = graph->edge_array[k];
        #if IS_BFS
        int new_dist = ret[node] + 1;
        #else
        int new_dist = ret[node] + graph->edge_values[k];
        #endif
        int old_dist = ret[neighbor];
        if (old_dist>new_dist){
          ret[neighbor] = new_dist;
          #if COALESCE
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
    u_int64_t *tmp = out_wl;
    out_wl = in_wl;
    in_wl = tmp;
    fr_len = new_fr_len;
    new_fr_len = 0;
    for (u_int64_t v = 0; v < graph->nodes; v++) bitmap[v]=false;
  }
  free(in_wl); free(out_wl); free(bitmap);
  printf("\nK Nodes Processed: %lu\n",nodes_explored/1000);
  printf("K Edges Processed: %lu\n",edges_explored/1000);
  printf("\nNode Re-explore factor: %.2f\n",(double)nodes_explored/graph->nodes);
  printf("Edge Re-explore factor: %.2f\n",(double)edges_explored/graph->edges);
  return edges_explored;
}

int main(int argc, char** argv) {

  char* output_name = NULL;
  string graph_fname;
  if (argc >= 2) graph_fname = argv[1];
  else graph_fname = "datasets/Kron16/";

  if (argc >= 3) output_name = argv[2];

  dataset_has_edge_val = 1;
  graph = new graph_loader(graph_fname,1);
  ret = (int *) calloc(graph->nodes, sizeof(int));
  for (int i = 1; i < graph->nodes; i++) {
    ret[i] = (INT32_MAX>>1);
  }


  printf("\n\nstarting kernel\n");
  auto start = chrono::system_clock::now();
  std::cout << std::setprecision(2) << std::fixed;

  u_int64_t edges_explored = _kernel_(0, 1);
  printf("\nending kernel");
  auto end = std::chrono::system_clock::now();

  chrono::duration<double> elapsed_seconds = end-start;
  cout << "\nkernel computation time: " << elapsed_seconds.count() << "s\n";

  if (output_name!=NULL){
    string name = output_name;
    if (edges_explored < (1<<24) ) graph->output_result(output_name);
  }
  bool res = graph->output_edges_traversed(argv[1],edges_explored);
  
  free(ret);
  delete graph;
  return res;
}
