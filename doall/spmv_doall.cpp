
using namespace std;
#include<omp.h>
#include "../src/common/macros.h"
// System Configuration Parameters
#include "../src/configs/config_system.h"
#include "../src/configs/config_queue.h"
// Global structures and defined parameters
#include "../src/common/global.h"
#include "../src/mem/memory_util.h"
#include "../src/dataset_loaders/graph_loader.h"
graph_loader * graph;

void _kernel_(u_int32_t id, u_int32_t core_num, u_int64_t * numFrontierNodesPushed, u_int64_t * numEdgesProcessed){
    for (int i = id; i < graph->nodes; i+=core_num)
    {
        int yi0 = 0;
        u_int32_t begin = graph->node_array[i];
        u_int32_t end = graph->node_array[i+1];
        //value should be the dense_array, since we don't have it, we use a constant vector
        int dense_value = 5; //graph->dense_vector[i];

        for (int k=begin; k < end; k++){
          u_int32_t idx = graph->edge_array[k];
          int res = graph->edge_values[k] * dense_value;
          yi0 += res;
        } 
        ret[i] = yi0;
    }
}

int main(int argc, char** argv) {

  char* output_name = NULL;
  string graph_fname;
  if (argc >= 2) graph_fname = argv[1];
  else graph_fname = "datasets/Kron16/";

  int num_workers = GRID_X;
  
  if (argc >= 3) output_name = argv[2];
  if (argc >= 4) num_workers = atoi(argv[3]);

  cout << "Running with "<<num_workers<<"\n";

  u_int64_t totalEdgesProcessed=0;
  u_int64_t totalFrontierNodesProcessed = 0;
  u_int64_t * numFrontierNodesPushed  = (u_int64_t *) calloc(num_workers, sizeof(u_int64_t));
  u_int64_t * numEdgesProcessed  = (u_int64_t *) calloc(num_workers, sizeof(u_int64_t));

  dataset_has_edge_val = 1;
  graph = new graph_loader(graph_fname,1);
  ret = (int *) calloc(graph->nodes, sizeof(int));


  printf("\n\nstarting kernel\n");
  auto start = chrono::system_clock::now();
  std::cout << std::setprecision(2) << std::fixed;

  omp_set_num_threads(num_workers);
  #pragma omp parallel default(shared)
  {
    int tid = omp_get_thread_num();
    int max_threads = omp_get_num_threads();
    assert(max_threads == num_workers);
    _kernel_(tid, max_threads, &numFrontierNodesPushed[tid], &numEdgesProcessed[tid]);
  }
  printf("\nending kernel");
  auto end = std::chrono::system_clock::now();

  chrono::duration<double> elapsed_seconds = end-start;
  cout << "\nkernel computation time: " << elapsed_seconds.count() << "s\n";

  if (output_name!=NULL){
    graph->output_result(output_name);
  }
  
  free(ret);
  delete graph;
  free(numEdgesProcessed); free(numFrontierNodesPushed);
  return 0;
}
