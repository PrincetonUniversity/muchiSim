#include <math.h>
#define TWO_PI (6.2831853071795864769252867665590057683943L)

#define EPSILON 1e-5


#if FUNC
#define SEND_DATA 1
void swap(float *a, float *b) {
    float temp = *a;
    *a = *b;
    *b = temp;
}
void bit_reversal(float *array, int N) {
    int i, j = 0;
    int N2 = N * 2;
    for (i = 0; i < N2; i += 2) {
        if (j > i) {
            swap(array[j], array[i]);
            swap(array[j+1], array[i+1]);
            assert(j<N2);
            assert(i<N2);
        }
        int m = N;
        while (m >= 2 && j >= m) {
            j -= m;
            m /= 2;
        }
        j += m;
    }
}
void fft_1d(float* array, int N) {
    int N2 = N * 2;  // Total number of float elements, considering real and imaginary parts
    bit_reversal(array, N);
    for (int m = 2; m <= N; m *= 2) {
        int mh = m / 2;
        for (int i = 0; i < N; i += m) {
            for (int j = i; j < i + mh; j++) {
                int pos = j * 2;
                assert(pos < N2);
                float wr = cos(TWO_PI * (j - i) / m);
                float wi = -sin(TWO_PI * (j - i) / m);
                int indexRe1 = pos;
                int indexIm1 = pos + 1;
                int indexRe2 = pos + m;
                int indexIm2 = pos + m + 1;
                assert(indexRe1 < N2 && indexRe2 < N2);
                assert(indexIm1 < N2 && indexIm2 < N2);
                float re = array[indexRe2] * wr - array[indexIm2] * wi;
                float im = array[indexRe2] * wi + array[indexIm2] * wr;
                float temp_re = array[indexRe1];
                float temp_im = array[indexIm1];
                array[indexRe1] = temp_re + re;
                array[indexIm1] = temp_im + im;
                array[indexRe2] = temp_re - re;
                array[indexIm2] = temp_im - im;
            }
        }
    }
}
#else // If not functional simulation
  #define SEND_DATA 0
#endif

/////////////////////////
float * fft_array;
float * fft_trans;

bool compare_out(char* output_name){
  #if FUNC
    ofstream outfile;
    outfile.open (output_name);
    if (!outfile.is_open()) {
      cout << "Could not open output result file" << endl;
      return false;
    }
    outfile << std::setprecision(1) << std::fixed;
    outfile << "\n\nFFT array result: ";
    for (uint32_t i=0; i<GRID_X*GRID_SIZE; i++){
      outfile << fft_array[i*2] << " " << fft_array[i*2+1] << "j, ";
    }
    outfile.close();
  #endif
  free(fft_array); free(fft_trans);
  return true;
}


int num_global_params = 3;
void initialize_dataset_structures(){
  task_global_params = (int64_t *) calloc(num_global_params*GRID_SIZE, sizeof(int64_t));
  u_int64_t len = (u_int64_t)(GRID_X*2) * (u_int64_t)GRID_SIZE;
  fft_array = (float *) calloc(len, sizeof(float));
  fft_trans = (float *) calloc(len, sizeof(float));
}

void config_dataset(string dataset_filename){
}

void config_app(){
  proxy_default = 0;
  u_int32_t t3b_piped_tasks = 1;
  u_int32_t t3_piped_tasks  = 8 * smt_per_tile;

  // For faster simulation we can avoid sending the the 2 data flits if that matches the system we are trying to simulate
  #if SEND_DATA
    num_task_params[2] = 4;
  #else
    num_task_params[2] = 2;
  #endif
  num_task_params[3] = num_task_params[2];

  // Configurations that are disallowed
  assert(PROXY_FACTOR==1);

  task1_dest = 2;
  iq_sizes[1] = unused_buffer;
  oq_sizes[1] = unused_buffer;
  oq_sizes[2] = t3_piped_tasks * num_task_params[2];
  iq_sizes[2] = oq_sizes[2] * io_factor_t3;
  oq_sizes[3] = unused_buffer;
  iq_sizes[3] = unused_buffer;

  dataset_words_per_tile = GRID_X*2;
}

int task_init(int tX, int tY){
  int64_t N2 = GRID_X*2;
  routers[tX][tY]->output_q[C][0].enqueue(Msg(0, MONO,0) );
  float * ptr = &fft_array[global(tX,tY)*N2];
  for (int i=0; i<GRID_X; i++) ptr[2*i] = i;

  return 1;
}

void send_data(int tX,int tY, float * ptr, int i, int dim, int dest, int addr, u_int64_t timer, int & penalty){
  union float_int send_real, send_imag;
  assert(i!=dim);
  send_real.float_val = ptr[2*i];
  send_imag.float_val = ptr[2*i+1];
  OQ(dest).enqueue(Msg(addr, HEAD, timer+penalty++) );
  #if SEND_DATA
    OQ(dest).enqueue(Msg(send_real.int_val, MID, timer+penalty++) );
    OQ(dest).enqueue(Msg(send_imag.int_val, MID, timer+penalty++) );
  #endif
  OQ(dest).enqueue(Msg(dim, TAIL, timer+penalty++) );
}

int task1_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  int tileid = global(tX,tY);
  int64_t * global_params = &task_global_params[tileid*num_global_params];
  int penalty = 0;
  Msg msg = IQ(0).dequeue();
  u_int32_t idx = msg.data;
  u_int32_t next_idx;

  int dim_to_send, dim_static;
  int round = global_params[1];
  if (idx==0){
    round = ++global_params[1];
  }

  float * gl_fft_array;
  if ((round % 2) == 1){dim_to_send = tX; dim_static = tY; gl_fft_array = fft_array;}
  else if (round == 2){dim_to_send = tY; dim_static = tX; gl_fft_array = fft_trans;}

  int n = GRID_X; int log2n = log2(n);
  float * ptr = &gl_fft_array[tileid*n*2];
  if (idx==0){
    #if FUNC
      fft_1d(ptr, n);
    #endif
    penalty += 6.5*n*log2n + 35*n + 36*log2n;
    flop(5*n*log2n);
    penalty += 29*n;

    if (dim_to_send==0) idx=1; // So that they don't start sending their own
    if (round == 3) return penalty; // After 3rd compute phase we are done.
  }

  
  int dest = get_qid();
  u_int32_t capacity = (OQ(dest).capacity() / num_task_params[dest]);

  int begin = idx;
  int end;
  if (idx < dim_to_send){ // First wave
    int remain = dim_to_send - idx;
    if (remain > capacity){
      end = begin + capacity;
      next_idx = end;
    }else{
      end = begin + remain;
      next_idx = dim_to_send+1;
    }
  } else{ // Second wave, assume X and Y dimension have equal size
    int remain = GRID_X - idx;
    if (remain > capacity){
      end = begin + capacity;
      next_idx = end;
    }else{
      end = begin + remain;
      next_idx = GRID_X;
    }
  }

  if (round == 1){ // First wave
    for (int i=begin; i<end; i++){
      send_data(tX, tY, ptr, i, dim_to_send, dest, XYHeadFlit(i, tY), timer, penalty);
    }
  } else{ // Second wave
    for (int i=begin; i<end; i++){
      send_data(tX, tY, ptr, i, dim_to_send, dest, XYHeadFlit(tX, i), timer, penalty);
    }
  }

  // If we need to keep sending
  if (next_idx < GRID_X) IQ(0).enqueue(Msg(next_idx, MONO,timer+penalty));
  else if (global_params[2] == 1){
    IQ(0).enqueue(Msg(0, MONO,timer+penalty));
    global_params[2] = 0;
  }
  
  return penalty;
}

int task2_kernel(int tX,int tY , u_int64_t timer, u_int64_t & compute_cycles){
  return 1;
}

int task3_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  int tileid = global(tX,tY);
  int64_t * global_params = &task_global_params[tileid*num_global_params];
  float * gl_fft_array; float * gl_fft_trans;

  int penalty = 5; // beq + mov
  load_mem_wait(1);
  int round = global_params[1];
  if (((round % 2) == 1)){gl_fft_array = fft_array; gl_fft_trans = fft_trans;}
  else if (round == 2){gl_fft_array = fft_trans; gl_fft_trans = fft_array;}

  int n = GRID_X;
  float * ptr_orig;
  float * ptr_array = &gl_fft_array[tileid*n*2];
  float * ptr_trans = &gl_fft_trans[tileid*n*2];
  penalty+=2;
  

  int orig_idx, dest_idx;
  union float_int data_real, data_imag;
  int addr = IQ(2).dequeue().data; // Ignore
  #if SEND_DATA
    data_real.int_val = IQ(2).dequeue().data;
    data_imag.int_val = IQ(2).dequeue().data;
  #endif
  int orig_dim = IQ(2).dequeue().data;
  penalty+=2;

  if (round==1){ // trans X
    #if ASSERT_MODE
      assert(orig_dim < GRID_X);
      assert(orig_dim != tX);
    #endif
    ptr_orig =  &gl_fft_array[global(orig_dim,tY)*n*2]; // orig tile is tY, orig
    orig_idx = tX;
  } else{
    ptr_orig =  &gl_fft_array[global(tX,orig_dim)*n*2]; // orig tile is orig_dim, tX
    orig_idx = tY;
  }


  #if FUNC
    #if SEND_DATA
      assert(fabs(data_real.float_val - ptr_orig[orig_idx*2]) < EPSILON);
      assert(fabs(data_imag.float_val - ptr_orig[orig_idx*2+1]) < EPSILON);
    #else
      data_real.float_val = ptr_orig[orig_idx*2];
      data_imag.float_val = ptr_orig[orig_idx*2+1];
    #endif
    ptr_trans[orig_dim*2] = data_real.float_val;
    ptr_trans[orig_dim*2+1] = data_imag.float_val;
  #endif
  penalty += 2;
  
  load_mem_wait(1); store(1);
  global_params[0]++;
  if (global_params[0] == GRID_Xm1){
    #if FUNC
    if (round==1){ // trans X
      ptr_trans[tX*2] = ptr_array[tX*2];
      ptr_trans[tX*2+1] = ptr_array[tX*2+1];
    } else{ // trans Y
      ptr_trans[tY*2] =  ptr_array[tY*2];
      ptr_trans[tY*2+1] = ptr_array[tY*2+1];
    }
    #endif
    penalty += 2;

    global_params[0] = 0;
    if (routers[tX][tY]->output_q[C][0].is_empty()) routers[tX][tY]->output_q[C][0].enqueue(Msg(0, MONO,0) );
    else global_params[2] = 1;
    penalty += 2;
  }

  return penalty;
}

int task3bis_kernel(int tX,int tY, u_int64_t timer, u_int64_t & compute_cycles){
  return 2;
}

