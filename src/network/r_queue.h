
// Circular buffer for queues
template <typename T>
struct R_queue {
  T * arr; 
  uint32_t virtual_size = 1;
  int head = 0;
  int tail = 0;

  R_queue(uint32_t size){init(size);}

  void init(uint32_t size) {
    size++;
    arr = (T *) malloc(size*sizeof(T));
    virtual_size = size;
    head = 0;
    tail = 0;
  }
  void destroy() {
    free(arr);
  }

  T get(int index){
    return arr[index];
  }

  int capacity(){
    int rest;
    if (head < tail) rest = tail - head - 1;
    else rest = (virtual_size - 1 - (head - tail) );
    return rest;
  }

  bool more_than_half_free(){
    int used;
    if (head < tail) used = virtual_size - (tail - head);
    else used = (head - tail);
    int max = virtual_size / 2;
    return (used < max);
}

  int occupancy(){
    int used;
    if (head < tail) used = virtual_size - (tail - head);
    else used = (head - tail);
    return used;
  }

  bool is_drain(){
    int used;
    if (head < tail) used = virtual_size - (tail - head);
    else used = (head - tail);
    int max = router_buffer;// * drain_trigger_ratio);
    return (used > max);
  }

  bool cannot_fit_two(){
    int used;
    if (head < tail) used = virtual_size - (tail - head);
    else used = (head - tail);
    int max = virtual_size - 2;
    // e.g. size is 32, and max is 31
    // so we return true if used is 31 or more
    return (used >= max);
  }

  bool is_full() {    
    return tail == ((head + 1) % virtual_size);
  }
  
  bool is_empty() {
    return (head == tail);
  }
  
  void enqueue(T v) {
    int index = head;
    #if ASSERT_MODE
      assert(tail != ((head + 1) % virtual_size));
    #endif
    arr[index] = v;
    head = ((index + 1) % virtual_size);
  }
  
  T dequeue() {
    #if ASSERT_MODE
      assert(head != tail);
    #endif
    int index = tail;
    T ret = arr[index];
    tail = ((index + 1) % virtual_size);
    return ret;
  }

  T peek() {
    T ret = arr[tail];
    return ret;
  }

  T peek_next() {
    T ret = arr[((tail + 1) % virtual_size)];
    return ret;
  }
  
  void do_enqueue(T v) {
    while (is_full()) { };
    enqueue(v);
  }

  T do_dequeue() {
    while (is_empty()) {};
    T ret = dequeue();
    return ret;
  }  
};