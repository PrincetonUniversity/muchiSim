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
#elif APP==FFT
  #include "../apps/fft.h"
#elif APP==SPMV_FLEX
  #include "../apps/spmv_flex.h"
#elif APP==SPMM
  #include "../apps/spmm.h"
#else
  #include "../apps/multi.h"
#endif