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

int countRedundancy(int *in_wl, int listLength, int numNodes){
  bool isAdded[numNodes];
  int redundancyCount = 0;
  for (int i=0; i< numNodes; i++){
    isAdded[i] = false;
  }
  for (int i=0; i< listLength; i++){
    if (isAdded[in_wl[i]]) {
      redundancyCount+=1;
    }
    else {
      isAdded[in_wl[i]] = true;
    }
  }
  double ratio = (double) redundancyCount / listLength ;
  cout << "\tFrontier Count: " << listLength << "\t Redundant Count: " << redundancyCount << "\t \% Redundancy: " << ratio << "\n";
  return redundancyCount;
}


void _kernel_(int* ret, int* in_wl, int* in_index, int* out_wl, int* out_index, int tid, int num_threads) {
  int node, neighbor;
  int label;
  int old_label=-1;
  for (int i = tid; i < *in_index; i+=num_threads) {
    node = in_wl[i];
    cout << "Got node " << node << " from frontier\n";
    int start = G.node_array[node];
    int end = G.node_array[node+1];
    if (ret[node]!=-1) label = ret[node];
    else {
      ret[node]=node;
      label = node;
    }
    cout << "NUMBER OF NEIGHBORS = " << end-start << "\n";
    cout << "label = " << label << "\n";
    
    for (int e = start; e < end; e++) {
      neighbor = G.edge_array[e];
      int success = dec_atomic_compare_exchange((uint32_t*) (ret + neighbor), (uint32_t) -1, (uint32_t) label);
      if (!success) old_label= dec_atomic_fetch_min((uint32_t*) (ret + neighbor), (uint32_t) label);
      if (success || old_label<label) {
        int index = dec_atomic_fetch_add((uint32_t*) out_index, 1);
        out_wl[index] = neighbor;
      }
    } 
  }

}

void wcc(int* ret, int* in_wl, int* in_index, int* out_wl, int* out_index, int tid, int num_threads) {
  int num_epochs = 1;
  int index = 0;

  while (*in_index > 0) {
    printf("-- epoch %d %d\n", num_epochs, *in_index);
    auto start = chrono::system_clock::now();
    _kernel_(ret, in_wl, in_index, out_wl, out_index, tid, num_threads);
    auto end = std::chrono::system_clock::now();
    chrono::duration<float> elapsed_seconds = end-start;
    printf("----finished kernel! doing %d nodes in x86\n", *in_index);
    cout << "\ndecades kernel computation time: " << elapsed_seconds.count() << "s\n";
    
    if (*out_index==0) {
      for (int i=index; i<G.nodes; i++){
	if (ret[i]==-1){
	  out_wl[*out_index] = i;
	  *out_index +=1;
	  index +=1;
	  break;
	}
      }
    }

    int *tmp_wl = out_wl;
    out_wl = in_wl;
    in_wl = tmp_wl;
    
    *in_index = *out_index;
    *out_index = 0;
    countRedundancy(in_wl, in_index[0], G.nodes);
    num_epochs++;
  }  
}

int main(int argc, char** argv) {

  char *graph_fname;

  assert(argc >= 2);
  graph_fname = argv[1];

  assert(argc >= 3);
  int num_seeds = atoi(argv[2]);
  
  // Create data structures
  G = graph_loader(graph_fname);

  int N = G.nodes;
  
  int * ret = (int *) malloc(sizeof(float) * N);
  for (int i=0; i< G.nodes; i++) ret[i] = -1;
  
  int * in_index = (int *) malloc(sizeof(int) * 1);
  *in_index = 0;
  int * out_index = (int *) malloc(sizeof(int) * 1);
  *out_index = 0;
  
  int * in_wl = (int *) malloc(sizeof(int) * N);
  int * out_wl = (int *) malloc(sizeof(int) * N);


  // Kernel initialization
  int chunk_size = G.nodes/num_seeds;
  for (int i = 0; i < num_seeds; i++) {
    int index = *in_index;
    *in_index = index + 1;
    in_wl[index] = i*chunk_size;
    cout << "Added node " << in_wl[index] << " to frontier.\n";
  }

  // Kernel execution
  printf("\n\nstarting kernel\n");
  auto start = chrono::system_clock::now();

  wcc(ret, in_wl, in_index, out_wl, out_index, 0 , 1);

  printf("\nending kernel");
  auto end = std::chrono::system_clock::now();
  chrono::duration<float> elapsed_seconds = end-start;
  cout << "\nkernel computation time: " << elapsed_seconds.count() << "s\n";
  
#if defined(OUTPUT_RET)
  ofstream outfile;
  outfile.open ("WCC_out.txt");
  for (int i = 0; i < N; i++) {
    //outfile << i << "\t" << ret[i] << "\n";
    outfile << ret[i] << "\n";
  }
  outfile.close();
#endif
  
  free(ret);
  free(in_index);
  free(out_index);
  free(in_wl);
  free(out_wl);

  return 0;
}
