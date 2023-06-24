#if APP==PAGE
  #include "../apps/pagerank.h"
#elif (APP==SSSP || APP==BFS)
  #include "../apps/sssp.h"
#elif APP==WCC
  #include "../apps/wcc.h"
#elif APP==SPMV
  #include "../apps/spmv.h"
#elif APP==HISTO
  #include "../apps/histo.h"
#else
  #include "../apps/multi.h"
#endif