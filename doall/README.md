# Doall implementations
For reference, the original implementations are in the `doall` folder.
These commands generate, for each application, the reference output for the Kron16 dataset.

## Pagerank with Coalescing
    g++ -O3 doall/pagerank_doall.cpp -o bin/page.run -DCOALESCE=1;
    ./bin/page.run datasets/Kron16/ doall/page_K16_ref.txt

## SPMV
    g++ -O3 doall/spmv_doall.cpp -fopenmp -o bin/spmv.run; 
    bin/spmv.run datasets/Kron16/ doall/spmv_K16_ref.txt

## BFS
    g++ -O3 doall/sssp_doall.cpp -o bin/bfs.run -DIS_BFS=1
    bin/bfs.run datasets/Kron16/ doall/bfs_K16_ref.txt

## BFS with Coalescing
    g++ -O3 doall/sssp_doall.cpp -o bin/bfs.run -DIS_BFS=1 -DCOALESCE=1;
    bin/bfs.run datasets/Kron16/

## SSSP
    g++ -O3 doall/sssp_doall.cpp -o bin/sssp.run;
    bin/sssp.run datasets/Kron16/ doall/sssp_K16_ref.txt

## WCC
    Does not compile at the moment, need to be fixed after updating to the current graph loader library