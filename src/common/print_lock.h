
class print_lock {
private:
  std::mutex mtx;

public:

  void routerProgramDone(u_int32_t rid) {
    std::unique_lock<std::mutex> lock(mtx);
    cout << "Program Done Router Thread: "<<rid<<"\n" << flush;
  }
  void workerProgramDone(u_int32_t rid) {
    std::unique_lock<std::mutex> lock(mtx);
    cout << "Program Done Worker Thread: "<<rid<<"\n" << flush;
  }

  void routerEpochDone(u_int32_t rid) {
    std::unique_lock<std::mutex> lock(mtx);
    cout << "Epoch Done Router: "<<rid<<"\n" << flush;
  }
  void workerEpochDone(int epoch, u_int64_t nodesEpoch, u_int64_t edgesEpoch, u_int64_t timer) {
    std::unique_lock<std::mutex> lock(mtx);
    cout << "##EPOCH## "<< epoch <<" COMPLETED at Time (K cy): "<< timer/1000 <<"\n";
    cout << "Nodes processed until now: " << (nodesEpoch) << "\n";
    cout << "Edges processed until now: " << (edgesEpoch) << "\n" << flush;
  }
};
