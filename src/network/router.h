#ifndef __ROUTER_MODEL_BASIC_H__
#define __ROUTER_MODEL_BASIC_H__
#include "r_queue.h"

u_int32_t task_dequeue(u_int32_t flit, u_int32_t per_tile){
  u_int64_t dst_tile = (flit % GRID_SIZE);
  u_int64_t local = (flit >> GRID_SIZE_LOG2);
  u_int64_t res = dst_tile * per_tile + local;
  return res;
}

// REVISIT make this more general for any number of tasks
u_int32_t task2_dequeue(u_int32_t data){
   return task_dequeue(data, task_chunk_size[1]);
}
u_int32_t task3_dequeue(u_int32_t data){
  return task_dequeue(data, task_chunk_size[2]);
}

u_int32_t getTile(int qid, int data){
   u_int32_t calc = data/task_chunk_size[qid];
   return calc;
}
u_int32_t getHeadFlit(int qid, int data){
   u_int32_t local_index = data%task_chunk_size[qid];
   u_int32_t dst = getTile(qid, data);
   u_int32_t flit = (local_index << GRID_SIZE_LOG2) | dst;
   return flit;
}

u_int64_t getTile(int qid, u_int64_t data){
   u_int64_t calc = data/task_chunk_size[qid];
   return calc;
}
u_int32_t getHeadFlit(int qid, u_int64_t data){
   u_int64_t local_index = data%task_chunk_size[qid];
   u_int64_t dst = getTile(qid, data);
   u_int64_t flit = (local_index << GRID_SIZE_LOG2) | dst;
   return flit;
}

u_int32_t XYHeadFlit(int tX, int tY){
   // To compensate the conversion that happens in the router for Mesh
   #if TORUS==0
      if ((tY%2)==1) tX = GRID_Xm1 - tX;
   #endif
   #if ASSERT_MODE
      assert(tX < GRID_X);
      assert(tY < GRID_Y);
   #endif
   u_int32_t flit = (tY << GRID_X_LOG2) | tX;
   return flit;
}

class RouterModel
{
public:

   RouterModel(u_int16_t id_X, u_int16_t id_Y);
   ~RouterModel();

   char print_dest_route(int d, int qid, u_int64_t router_timer);
   bool next_route(bool * drain_mode,int d, u_int16_t dir, u_int16_t qid, u_int16_t * next_inp4out, u_int16_t abs_diff);

   void get_dst(u_int32_t msg_data, int & dst_X, int & dst_Y);
   void comp_destXY(int qid, uint16_t orig, u_int32_t msg_data, int & dst_X, int & dst_Y, bool & route_to_core, bool & exact_x, bool & exact_y, bool & switch_X_board, bool & switch_Y_board);
   void find_dest(int qid, uint16_t orig, u_int32_t msg_data, int & abs_diff_X, int & abs_diff_Y, bool & Y_routing_pri, bool & dnorth, bool & dsouth, bool & dwest, bool & deast, bool & route_to_core, bool & exact_x, bool & exact_y, bool & switch_X_board, bool & switch_Y_board);
   uint64_t calc_inp4out(bool * drain_X,bool * drain_Y, int qid, u_int64_t router_timer, u_int16_t * next_inp4out, u_int32_t & collisions);
   int advance(pthread_mutex_t * mutex, bool * drain_X,bool * drain_Y, u_int64_t router_timer, u_int32_t * active);
   int check_queues_occupancy();
   int check_border_traffic(int dest, int qid, bool * already_routed);

   R_queue<Msg> * output_core;
   R_queue<Msg> * input_q[DIRECTIONS];
   R_queue<Msg> * output_q[DIRECTIONS];

   u_int16_t open[DIRECTIONS][NUM_QUEUES];
   u_int16_t next_d[NUM_QUEUES];
   u_int16_t next_q;
   bool input_empty[DIRECTIONS][NUM_QUEUES];
   bool output_routable[DIRECTIONS][NUM_QUEUES];
   bool route_board_X,route_board_Y;

   u_int16_t id_X;
   u_int16_t id_Y;

   u_int16_t pack_id_X;
   u_int16_t pack_id_Y;
   u_int16_t pack_id;

   #if BOARD_W < GRID_X
      u_int16_t board_id_X;
      u_int16_t board_id_Y;
      u_int16_t board_id;
      u_int16_t board_origin_X;
      u_int16_t board_origin_Y;
      u_int16_t board_end_X;
      u_int16_t board_end_Y;
      bool is_origin_board_X;
      bool is_origin_board_Y;
      bool is_end_board_X;
      bool is_end_board_Y;
      bool is_border;
   #endif
   u_int16_t die_id_X;
   u_int16_t die_id_Y;
   u_int32_t die_id;
   bool is_origin_die_X;
   bool is_origin_die_Y;
   bool is_end_die_X;
   bool is_end_die_Y;
   bool is_origin_pack_X;
   bool is_origin_pack_Y;
   bool is_end_pack_X;
   bool is_end_pack_Y;

   u_int32_t input_collision[NUM_QUEUES];
   u_int32_t output_collision[NUM_QUEUES];

private:
};

RouterModel::RouterModel(u_int16_t id_X, u_int16_t id_Y) :
   id_X(id_X),
   id_Y(id_Y)
{
   pack_id_X = id_X / PACK_W;
   pack_id_Y = id_Y / PACK_H;
   pack_id = pack_id_X*PACK_FACTOR + pack_id_Y;

   #if BOARD_W < GRID_X
      board_id_X = id_X / BOARD_W;
      board_id_Y = id_Y / BOARD_H;
      board_id = board_id_X*BOARD_FACTOR + board_id_Y;
      board_origin_X = board_id_X * BOARD_W;
      board_origin_Y = board_id_Y * BOARD_H;
      #if TORUS==1
         board_end_X = board_origin_X + BOARD_W_HALF;
         board_end_Y = board_origin_Y + BOARD_H_HALF;
      #else // MESH
         board_end_X = board_origin_X + BOARD_W;
         board_end_Y = board_origin_Y + BOARD_H;
      #endif
      is_origin_board_Y = (id_Y == board_origin_Y);
      is_end_board_Y    = (id_Y == board_end_Y);
      is_end_board_X    = (id_X == board_end_X);
      is_origin_board_X = (id_X == board_origin_X);
      is_border = is_origin_board_X || is_end_board_X || is_origin_board_Y || is_end_board_Y;
   #endif

   die_id_X = id_X / DIE_W;
   die_id_Y = id_Y / DIE_H;
   die_id = die_id_X*DIE_FACTOR + die_id_Y;
   
   #if TORUS==1
      // In the Torus, the tiles don't cross dies at the border
      // Don't consider die border if it's at the edge of the tile grid!
      // In a torus the wrap-around in a single link inside the same die!
      // REVISIT: Should this be at the BOARD or GRID level?
      is_origin_die_Y = (id_Y % BOARD_H_HALF != 0)                && (id_Y % DIE_H_HALF == 0);
      is_origin_die_X = (id_X % BOARD_W_HALF != 0)                && (id_X % DIE_W_HALF == 0);
      is_end_die_Y    = (id_Y % BOARD_H_HALF != (BOARD_H_HALF-1)) && (id_Y % DIE_H_HALF == (DIE_H_HALFm1));
      is_end_die_X    = (id_X % BOARD_W_HALF != (BOARD_W_HALF-1)) && (id_X % DIE_W_HALF == (DIE_W_HALFm1));
      // Package calculation
      is_origin_pack_X = is_origin_die_X && ((id_X % PACK_W_HALF) == 0);
      is_origin_pack_Y = is_origin_die_Y && ((id_Y % PACK_H_HALF) == 0);
      is_end_pack_X = is_end_die_X && ((id_X % PACK_W_HALF) == (PACK_W_HALF-1));
      is_end_pack_Y = is_end_die_Y && ((id_Y % PACK_H_HALF) == (PACK_H_HALF-1));
   #else // MESH
      // In the Mesh we should not have communication at the border of the tile grid!
      is_origin_die_Y = (id_Y % BOARD_H != 0) && (id_Y % DIE_H == 0);
      is_origin_die_X = (id_X % BOARD_W != 0) && (id_X % DIE_W == 0);
      is_end_die_Y    = (id_Y % BOARD_H != (BOARD_H-1)) && (id_Y % DIE_H == (DIE_Hm1));
      is_end_die_X    = (id_X % BOARD_W != (BOARD_W-1)) && (id_X % DIE_W == (DIE_Wm1));
      // Package calculation
      is_origin_pack_X = is_origin_die_X && ((id_X % PACK_W) == 0);
      is_origin_pack_Y = is_origin_die_Y && ((id_Y % PACK_H) == 0);
      is_end_pack_X = is_end_die_X && ((id_X % PACK_W) == (PACK_W-1));
      is_end_pack_Y = is_end_die_Y && ((id_Y % PACK_H) == (PACK_H-1));
   #endif

   output_core = (R_queue<Msg> *) malloc(NUM_QUEUES*sizeof(R_queue<Msg>) );
   // Queues that go from the router to the core (the router "outputs" to the core)
   for (int j=0; j<NUM_QUEUES; j++) output_core[j].init(iq_sizes[j]);

   
   for (int i=0; i<DIRECTIONS; i++){
      input_q[i] = (R_queue<Msg> *) malloc(NUM_QUEUES*sizeof(R_queue<Msg>) );
      output_q[i] = NULL;
      for (int j=0; j<NUM_QUEUES; j++){
         // Queues that go from the core to the router (the router gets "inputs" from the core)
         if (i==C) input_q[i][j].init(oq_sizes[j]);
         #if BOARD_W < GRID_X
            else if (i==PX || i==PY){if (is_border) input_q[i][j].init(rq_sizes[j]*GRID_X);}
         #endif
         // Network buffers
         else input_q[i][j].init(rq_sizes[j]);
         open[i][j] = DIR_NONE;
      }
   }
}

RouterModel::~RouterModel()
{
   for (int j=0; j< NUM_QUEUES; j++){
      output_core[j].destroy();
      if (output_core[j].occupancy() > 0) cout << "Delete C OQ OCC:"<< output_core[j].occupancy() <<" Q(" << j << "), R(" << id_X << "," << id_Y << ")\n";
   }
   free(output_core);
   for (int i=0; i<DIRECTIONS; i++){
      for (int j=0; j<NUM_QUEUES; j++){
         input_q[i][j].destroy();
         if (input_q[i][j].occupancy() > 0) cout << "Delete IQ OCC:"<< input_q[i][j].occupancy() <<" Q(" << j << "," << i << ", R(" << id_X << "," << id_Y << ")\n";
      }
      free(input_q[i]); 
   }
}

int RouterModel::check_queues_occupancy()
{
   int acc = 0;
   for (int q=0; q<NUM_QUEUES; q++) acc += output_core[q].occupancy();

   for (int dir=0; dir<DIRECTIONS; dir++){
      for (int q=0; q<NUM_QUEUES; q++){
         int iq_occ = input_q[dir][q].occupancy();
         if (input_q[dir]!=NULL) acc += iq_occ;
         if (output_q[dir]!=NULL) acc += output_q[dir][q].occupancy();
         // We consider the input queue as empty if it has less than 2 elements (for 'wide' two-flit NoC) or zero elements for single-fit NoC
         // REVISIT to make this code more general to any width, right now it only supports 32 and 64 bit flits
         input_empty[dir][q] = (iq_occ == 0);
      }
   }
   #if MUX_BUS > 1
      route_board_X = false;
      route_board_Y = false;
      for (int q=0; q<NUM_QUEUES; q++){
         route_board_X |= !input_empty[PX][q];
         route_board_Y |= !input_empty[PY][q];
      }
   #endif

   return acc;
}

// ONLY PRINTOUT FOR DEBUGGING, NOT USED FOR ROUTING
char RouterModel::print_dest_route(int origin, int qid, u_int64_t router_timer){
   char dest;
   bool valid;

   dest = '_';
   bool is_empty = input_q[origin][qid].is_empty();
   if (!is_empty){
      dest = '=';
      Msg msg = input_q[origin][qid].peek();
      u_int64_t msg_time = pu_to_noc_cy(msg.time);
      if ((msg.type <= HEAD) && (router_timer >= msg_time)){
            bool switch_Y_board, switch_X_board, route_to_core, exact_Y, exact_X;
            bool Y_routing_pri, dnorth, dsouth, dwest, deast;
            int abs_diff_Y, abs_diff_X;
            find_dest(qid, origin, msg.data, abs_diff_X, abs_diff_Y, Y_routing_pri, dnorth, dsouth, dwest, deast, route_to_core, exact_X, exact_Y, switch_X_board, switch_Y_board);
         
         if (switch_Y_board) {dest = getD(PY);}
         else if (switch_X_board){dest = getD(PX);}
         else if (route_to_core){   dest = getD(C);}
         else if (Y_routing_pri && abs_diff_Y!=0 || abs_diff_X==0){
            if (dnorth) {dest = getD(N);}
            else if (dsouth) {dest = getD(S);}
            else assert(0);
         }else{
            if (deast){dest = getD(E);}
            else if (dwest){dest = getD(W);}
            else assert(0);
         }

      } else if (router_timer >= msg_time){
         // assert(msg.type != HEAD);
         if (origin == open[C][qid]) return 'C';
         if (origin == open[PX][qid]) return 'X';
         if (origin == open[PY][qid]) return 'Y';
         for (u_int16_t out_port = 0; out_port <=7; out_port++){
            if (origin == open[out_port][qid]) return getD(out_port%4);
         }
      } else dest = '&'; //it's no time yet to move the message
   }// end of if empty
   return dest;
}

//Return true means routing to 'dir' failed and we need to explore another route
//Return false means we don't need to explore another route since this one can route the message
bool RouterModel::next_route(bool * drain_mode, int orig, u_int16_t dir, u_int16_t qid, u_int16_t * next_inp4out, u_int16_t abs_diff){
   // orig is input (direction where the message is coming from), dir is the direction to reach the destination
   u_int16_t flip = flipD(dir);

   bool output_contested = false;

   // Consider the output contested if the core tries to inject and the network buffer has over half occupancy
   // output_contested = (orig==C) ? (output_q[dir][qid].occupancy() > (rq_sizes[qid]/2)) : false;

   #if BOARD_W < GRID_X
      // Consider the output contested if we are in drain mode
      output_contested |= drain_mode[qid];
   #endif
   
   #if RUCHE_X
      // Ruche port to a given destination is that dest + 4
      u_int16_t dirR = dir+4;
      u_int16_t flipR = flip+4;
      bool can_jump_ruche = (abs_diff>=RUCHE_X);
      bool at_a_ruche_port;
      #if RUCHE_X == DIE_W_HALF
         // Connecting ruche only at the edges of a chiplet die (Torus mode)
         at_a_ruche_port = (id_X % DIE_W_HALF) == DIE_W_HALFm1 || (id_Y % DIE_H_HALF) == DIE_H_HALFm1;
      #elif RUCHE_X == DIE_W
         // Connecting ruche only at the edges of a chiplet die (Mesh mode)
         at_a_ruche_port = (id_X % DIE_W) == DIE_Wm1 || (id_Y % DIE_H) == DIE_Hm1;
      #else
         // Between chiplet dies!
         at_a_ruche_port = ((dir == W) || (dir == E)) && ((id_X % RUCHE_X)==0) ||
                           ((dir == N) || (dir == S)) && ((id_Y % RUCHE_Y)==0);
      #endif
      if (can_jump_ruche && at_a_ruche_port){
         output_contested |= !output_routable[dirR][qid]; // Check if dirRuche is available
         if (next_inp4out[dirR]==DIR_NONE && (orig==flipR || !output_contested)) {
            next_inp4out[dirR]=orig; return false;
         }
      }
   #endif

   // If Ruche cannot route, or there is no ruche, explore the regular NoC
   output_contested |= !output_routable[dir][qid];

   // PX (inter-board port) and PY have priority when injecting into the Torus
   // Any direction can output to PX and PY with high priority
   bool input_priority = orig==flip;
   #if BOARD_W < GRID_X
      input_priority |= ( (dir==N || dir==S  || dir==PY) && (orig==PY) ) || 
                        ( (dir==W || dir==E  || dir==PX) && (orig==PX) ) ||
                        ((orig==N || orig==S || orig==PY) && (dir==PY) ) ||
                        ((orig==W || orig==E || orig==PX) && (dir==PX) );
   #endif

   // If the direction is not taken, and either the output is routable for turns, or the message was coming straight from that direction
   if (next_inp4out[dir]==DIR_NONE && (input_priority || !output_contested)) {
      next_inp4out[dir] = orig; return false;
   }

return true;
}

void RouterModel::get_dst(u_int32_t msg_data, int & dst_X, int & dst_Y){
   u_int32_t dst = msg_data % GRID_SIZE;
   dst_X = dst & GRID_Xm1;
   dst_Y = dst >> GRID_X_LOG2;
   #if TORUS==0
      if ((dst_Y%2)==1) dst_X = GRID_Xm1 - dst_X;
   #endif  

   #if ASSERT_MODE
      assert(global(dst_X,dst_Y)==dst);
   #endif
}

void RouterModel::comp_destXY(int qid, uint16_t orig, u_int32_t msg_data, int & dst_X, int & dst_Y, bool & route_to_core, bool & exact_x, bool & exact_y, bool & switch_X_board, bool & switch_Y_board){
   get_dst(msg_data, dst_X, dst_Y);
   
   #if PROXY_FACTOR>1 && PROXY_ROUTING>=3 // Route Within proxy region
      if (qid == 3){
         int dst_rep_X = (id_X / PROXY_W);
         int dst_rep_Y = (id_Y / PROXY_H);
         int offset_in_rep_X = (dst_X % PROXY_W);
         int offset_in_rep_Y = (dst_Y % PROXY_H);
         // Truncate the destination to the nearest Proxy
         int x_within = dst_rep_X * PROXY_W + offset_in_rep_X;
         int y_within = dst_rep_Y * PROXY_H + offset_in_rep_Y;
         dst_X = x_within;
         dst_Y = y_within;
      }
   #endif

   // The flit should go to the Core
   exact_y = (dst_Y == id_Y);
   exact_x = (dst_X == id_X);

   // With Cascade Writeback mode, find proxy tiles on the way to the true owner
   #if PROXY_FACTOR>1
   #if PROXY_ROUTING==1
      if (qid == 3){
         #if BOARD_W < GRID_X
            // The following code is used to calculate the destination coordinates of a certain element
            // based on its current ID and the dimensions of the board it is placed on.
            // First, we calculate the row (dst_board_Y) and column (dst_board_X) that the element's ID corresponds to on the board.
            int dst_board_X = (id_X / BOARD_W);
            int dst_board_Y = (id_Y / BOARD_H);

            // Next, we calculate the offset between the element's current position and the nearest board in both dimensions.
            int offset_in_board_X = (dst_X % BOARD_W);
            int offset_in_board_Y = (dst_Y % BOARD_H);

            // Finally, we adjust the element's destination coordinates so that they are aligned with the nearest board.
            dst_X = dst_board_X * BOARD_W + offset_in_board_X;
            dst_Y = dst_board_Y * BOARD_H + offset_in_board_Y;
         #endif

         // Route to Proxy that is on the way to the real owner
         bool is_proxy_y = ( (dst_Y % PROXY_H) == (id_Y % PROXY_H) );
         bool is_proxy_x = ( (dst_X % PROXY_W) == (id_X % PROXY_W) );
         // route to the proxy tile unless it's a proxy tile and it's coming from the core (avoid endless loop)
         if (!(is_proxy_y && is_proxy_x && (orig==C)) ){
            #if SELECTIVE_SINKING==1
               bool came_from_vertical = orig==N || orig==S || orig==RN || orig==RS;
               bool cant_drop = !came_from_vertical && !exact_y;
               if (cant_drop && orig!=C) assert(is_proxy_y);
               uint16_t dest = (dst_Y < id_Y ? N : (dst_Y > id_Y ? S : (dst_X < id_X ? W : E)));
               bool port_is_not_empty = !output_q[dest][qid].is_empty();
               if (cant_drop || port_is_not_empty)
            #endif
               {exact_y |= is_proxy_y; exact_x |= is_proxy_x;}
         }
      }
   #elif PROXY_ROUTING>=4 // Proxy_routing=3 never cascades
      if (qid == 2){
         // Route to Proxy that is on the way to the real owner
         bool is_proxy_y = ( (dst_Y % PROXY_H) == (id_Y % PROXY_H) );
         bool is_proxy_x = ( (dst_X % PROXY_W) == (id_X % PROXY_W) );
         // route to the proxy tile unless it's a proxy tile and it's coming from the core (avoid endless loop)
         if (!(is_proxy_y && is_proxy_x && (orig==C)) ){
            uint16_t dest = (dst_Y < id_Y ? N : (dst_Y > id_Y ? S : (dst_X < id_X ? W : E)));
            // Check if it's a proxy
            if (is_proxy_y && is_proxy_x){
               // We want that: (a) coalesce a#if PROXY_ROUTING==4 lot while not in drain mode (b) coalesce a little while in drain mode
               // Go into the proxy if more than half the queue is free

               bool sink_into_proxy = true;
               #if PROXY_ROUTING==4 // extra selection
                  bool into_core_not_full = !output_q[C][3].cannot_fit_two();
                  //bool in_drain = (input_q[C][3].occupancy() < 4) && (input_q[C][2].occupancy() < 4) && (input_q[C][1].occupancy() < 4);
                  bool trajectory_has_something = !output_q[dest][2].is_empty();
                  bool trajectory_nearly_full = output_q[dest][2].cannot_fit_two();
                  bool into_core_not_too_busy = output_q[C][3].more_than_half_free();// && input_q[C][3].more_than_half_free();
                  // I sink if the trajectory is full, or
                  // The input to the core is not too busy in drain mode if my current path is busy, or I sink in normal mode if the destination queue is not busy
                  bool select_to_sink = into_core_not_full && trajectory_nearly_full || into_core_not_too_busy;// && (in_drain && trajectory_has_something || !in_drain);
                  sink_into_proxy = select_to_sink;
               #endif
               exact_y |= sink_into_proxy;
               exact_x |= sink_into_proxy;
            }
         }
      }
   #endif
   #endif

   // Steal region means that a in-transit task message can be stolen by a core that is in the same region
   #if STEAL_W > 1
      if (qid == 1){
         bool is_steal_region_y = ( (dst_Y / STEAL_H) == (id_Y / STEAL_H) );
         bool is_steal_region_x = ( (dst_X / STEAL_W) == (id_X / STEAL_W) );
         if (is_steal_region_x && is_steal_region_y){// && (orig!=C || qid!=3) ){
            bool this_core_less_busy_than_owner = frame_counters[id_X][id_Y][IDLE] > frame_counters[dst_X][dst_Y][IDLE];
            if (this_core_less_busy_than_owner || output_q[C][qid].occupancy()==0){exact_y = true; exact_x = true;}
            else{
               uint16_t dest = (dst_Y < id_Y ? N : (dst_Y > id_Y ? S : (dst_X < id_X ? W : E)));
               //if (!output_q[dest][qid].more_than_half_free()){exact_y = true; exact_x = true;}
               if (output_q[dest][qid].is_full()){exact_y = true; exact_x = true;}
            }
         }
      }
   #endif
   
   route_to_core = (exact_y && exact_x);
   
   #if BOARD_W < GRID_X
      if (!exact_y){
         // Or switching Chip Boards
         int dst_board_Y = (dst_Y / BOARD_H);
         if (dst_board_Y < board_id_Y){
            dst_Y = board_origin_Y;
            switch_Y_board = is_origin_board_Y;
         } else if (dst_board_Y > board_id_Y){
            dst_Y = board_end_Y;
            switch_Y_board = is_end_board_Y;
         }
      } else { // Exact Y
         if (!exact_x){
            // It's at the correct Board Y, but not at the correct Board X
            int dst_board_X = (dst_X / BOARD_W);
            if (dst_board_X != board_id_X){
               // // Make sure it's not at the corner!
               // if (dst_Y == board_origin_Y)  {if (id_Y < board_end_Y) {dst_Y = board_origin_Y+1;} else {dst_Y = board_origin_Y+BOARD_H-1;} }
               // else if (dst_Y == board_end_Y){if (id_Y < board_end_Y) {dst_Y = board_end_Y-1;}    else {dst_Y = board_end_Y+1;} }
               if (dst_board_X < board_id_X){
                  dst_X = board_origin_X;
                  switch_X_board = is_origin_board_X;
               } else if (dst_board_X > board_id_X){
                  dst_X = board_end_X;
                  switch_X_board = is_end_board_X;
               }
            }
         }
      }
      exact_y |= (dst_Y == id_Y);
      exact_x |= (dst_X == id_X);
   #endif
}

void RouterModel::find_dest(int qid, uint16_t orig, u_int32_t msg_data, int & abs_diff_X, int & abs_diff_Y, bool & Y_routing_pri, bool & dnorth, bool & dsouth, bool & dwest, bool & deast, bool & route_to_core, bool & exact_X, bool & exact_Y, bool & switch_X_board, bool & switch_Y_board){
   int dst_X, dst_Y;
   // Assign all these boolean variables inside comp_destXY
   comp_destXY(qid, orig, msg_data, dst_X, dst_Y, route_to_core, exact_X, exact_Y, switch_X_board, switch_Y_board);

   dnorth = (dst_Y < id_Y) && !exact_Y;
   dwest  = (dst_X < id_X) && !exact_X;

   abs_diff_Y = dnorth ? id_Y-dst_Y : dst_Y-id_Y;
   abs_diff_X = dwest  ? id_X-dst_X : dst_X-id_X;

   //TORUS: Basic Y then X routing (Dimension-Ordered with bubble alg), with or without Ruche
   //MESH: Random Init with dynamic turns, with 3/4 of the options.
   Y_routing_pri = true; //True for torus networks
   #if TORUS==1
      dnorth = dnorth && (abs_diff_Y <= BOARD_H_HALF) || (dst_Y > id_Y) && (abs_diff_Y > BOARD_H_HALF);
      dwest  = dwest  && (abs_diff_X <= BOARD_W_HALF) || (dst_X > id_X) && (abs_diff_X > BOARD_W_HALF);
      dsouth = !dnorth && !exact_Y;
      deast  = !dwest && !exact_X;
   #elif TORUS==0 //no torus
      dsouth = !dnorth && !exact_Y;
      deast  = !dwest && !exact_X;
      // Priority Y axis movement IF it can go North or West, but not SouthEast turn, WestNorth turn not allowed either
      // bool dest_X_odd = (dst_X%2);
      // bool dest_X_even = !dest_X_odd;
      // bool sure_Yfirst = dnorth && dwest;
      // bool sure_Xfirst = dsouth && deast;
      // bool maybe_Yfirst =  dest_X_even && (dnorth || dwest) || dest_X_odd && (dsouth || deast);
      //Y_routing_pri = sure_Yfirst || maybe_Yfirst && !sure_Xfirst;
      Y_routing_pri = dwest;
   #endif
}

uint64_t RouterModel::calc_inp4out(bool * drain_X, bool * drain_Y, int qid, u_int64_t router_timer, u_int16_t * next_inp4out, u_int32_t & collisions){
   int want_to_move_accum = 0;
   //reset next_inp4out
   for (int d = 0; d < DIRECTIONS; d++){
      next_inp4out[d]=DIR_NONE;

      #if TORUS==1 //only if pure torus, not with torus-extra
         if (output_q[d]!=NULL) output_routable[d][qid] = (output_q[d][qid].occupancy() < (rq_sizes[qid]-4) );
      #else
         output_routable[d][qid] = true;
      #endif
   }

   uint16_t start_d = next_d[qid];
   uint16_t end_d = start_d + DIRECTIONS;
   for (uint16_t i = start_d; i < end_d; i++){
      uint16_t orig = i%DIRECTIONS;
      
      #if ASSERT_MODE
         assert(input_q[orig] != NULL);
      #endif

      if (!input_empty[orig][qid]){
         want_to_move_accum++;
         Msg msg = input_q[orig][qid].peek();

         #if MUX_BUS > 1
            // There is board congestion if msg wantes to cross board boundary (input not empty) but output is not granted by MUX
            bool board_congestion = (orig==PY && (!route_board_Y && !input_empty[PY][qid]) ) || (orig==PX && (!route_board_X && !input_empty[PX][qid]) );
         #else
            bool board_congestion = false;
         #endif

         // If it's not a head we don't check the dst. We also check the time!
         // We will not have the case where cores run ahead of routers and put newer data in front of older data,
         // because the Core-output router wouldn't let the new messages progress until the router is caught up!
         u_int64_t msg_time = pu_to_noc_cy(msg.time);
         if ((msg.type <= HEAD) && (router_timer >= msg_time) && !board_congestion){
            // Therse variables are set inside find_dest
            bool switch_Y_board = false, switch_X_board = false, route_to_core = false;
            bool exact_Y = false, exact_X = false;
            bool Y_routing_pri, dnorth, dsouth, dwest, deast;
            int abs_diff_Y, abs_diff_X;
            find_dest(qid, orig, msg.data, abs_diff_X, abs_diff_Y, Y_routing_pri, dnorth, dsouth, dwest, deast, route_to_core, exact_X, exact_Y, switch_X_board, switch_Y_board);

            #if BOARD_W < GRID_X
            if (switch_Y_board){
               if (next_inp4out[PY]==DIR_NONE && (orig<=E || output_routable[PY][qid])) next_inp4out[PY] = orig; else collisions++;
            }
            else if (switch_X_board){
               if (next_inp4out[PX]==DIR_NONE && (orig<=E || output_routable[PX][qid])) next_inp4out[PX] = orig; else collisions++;
            }
            else
            #endif
            if (route_to_core){
               if (next_inp4out[C]==DIR_NONE) next_inp4out[C] = orig; else collisions++;
            }
            // Route to axis Y if Y_routing got priority and we are not at the correct Y or if we are already in exact_x
            else if (Y_routing_pri && !exact_Y || exact_X){
               #if TORUS==1
                  // Assert that we don't take a turn from X-dim to Y-dim
                  assert (orig!=PX);assert(orig!=E); assert(orig!=W);
               #endif
               // west could be going first south

               if (dnorth) {
                  if (next_route(drain_Y,orig,N,qid,next_inp4out,abs_diff_Y) ){ //couldn't route to N
                     #if TORUS==0
                        //it can try go East, as East/North is okay
                        if (deast){
                           if (next_route(drain_X,orig,E,qid,next_inp4out,abs_diff_X) ) collisions++;
                        } else 
                     #endif
                     collisions++;
                  }
               }else if (dsouth) {
                  if (next_route(drain_Y,orig,S,qid,next_inp4out,abs_diff_Y) ){  //couldn't route to S
                     #if TORUS==0
                        //it can try go West, as West/South is okay
                        if (dwest){
                           if (next_route(drain_X,orig,W,qid,next_inp4out,abs_diff_X) ) collisions++;
                        } else 
                     #endif
                     collisions++;
                  }
               } else assert(0);
            // Route to axis X if X_routing got priority and we are not at the correct X
            }else{
               if (deast){
                  if (next_route(drain_X,orig,E,qid,next_inp4out,abs_diff_X) ){  //couldn't route to E
                     #if TORUS==0
                        //it can try go North, as East/North is okay
                        if (dnorth){
                           if (next_route(drain_Y,orig,N,qid,next_inp4out,abs_diff_Y) ) collisions++;
                        } else 
                     #endif
                     collisions++;
                  }
               }else if (dwest){
                  if (next_route(drain_X,orig,W,qid,next_inp4out,abs_diff_X) ){  //couldn't route to W
                     #if TORUS==0
                        //it can try go South, as West/South is okay
                        if (dsouth){
                           if (next_route(drain_Y,orig,S,qid,next_inp4out,abs_diff_Y) ) collisions++;
                        } else 
                     #endif
                     collisions++;
                  } 
               }
               
            }
         } //if msg..
      }
   }
   return want_to_move_accum;
}

bool is_crossing_die(int dest, int dir_begin, int dir_end, bool is_origin, bool is_end){
   return is_origin && (dest == dir_begin) || is_end && (dest == dir_end);
}

int RouterModel::check_border_traffic(int dest, int qid, bool * already_routed){
   //Marked as routed so that we cannot route on this same cycle on the same physical noc
   already_routed[phy_noc(dest,qid)] = true;

   // Inter-board links
   #if BOARD_W < GRID_X
      if (dest==PX || dest==PY){
         inter_board_traffic[board_id]++;
         // Mark all the other links as already routed
         for (int p = 0; p < PHY_NOCS; p++) already_routed[dest*PHY_NOCS+p] = true;
         return inter_board_latency_ns*noc_freq;
      }
   #endif

   int latency = on_die_hop_latency;
   // Inter-Die links
   #if DIE_W < BOARD_W
      if (is_crossing_die(dest,W,E,is_origin_die_X, is_end_die_X) || is_crossing_die(dest,N,S,is_origin_die_Y, is_end_die_Y)){
         latency = die_hop_latency;
         inter_die_traffic[die_id]++;

         // Check if it's also crossing packages
         #if PACK_W < BOARD_W
            if (is_crossing_die(dest,W,E,is_origin_pack_X, is_end_pack_X) || is_crossing_die(dest,N,S,is_origin_pack_Y, is_end_pack_Y)){
               inter_pack_traffic[pack_id]++;
               latency = inter_pack_latency_ns*noc_freq;
            }
         #endif

         // Only one physical 32-bit NOC between dies unless config 3
         #if NOC_CONF<2
            for (int p = 0; p < PHY_NOCS; p++) already_routed[dest*PHY_NOCS+p] = true;
         #endif

         //Wide Noc but serialized into 32bit link
         #if NOC_CONF==1 || NOC_CONF==2
            // It assumes the link is pipelined, so it adds one cycle
            // To be precise, the next cycle should still use the port (add contention)
            if (wide_noc(qid)) latency++;
         #endif
         return latency;
      }
   #endif

   // If RUCHE enabled
   #if RUCHE_X>0
      // If it has taken a rouche port
      if ((dest>=RN) && (dest<=RE)){
         latency = ruche_hop_latency;
         ruche_traffic[die_id]++;
         // Ruche is shared across nocs and serialized 64bit into 32bit, always!
         // Ruche is a single Noc of 32bits
         for (int p = 0; p < PHY_NOCS; p++) already_routed[dest*PHY_NOCS+p] = true;
         if (wide_noc(qid)) latency++;
      }
   #endif

   return latency;
}

// Advance the router model by one cycle
// Drain is only used when we have more than 1 board, so don't care about it now.
// Router timer is the current cycle of the router
int RouterModel::advance(pthread_mutex_t * mutex, bool * drain_X,bool * drain_Y, u_int64_t router_timer, u_int32_t * active)
{
   u_int32_t want_to_move_accum = 0;
   u_int32_t end_collisions = 0;

   int moved[NUM_QUEUES];
   int origin;
   bool output_full[NUM_QUEUES][DIRECTIONS];
   bool already_routed[DIRECTIONS*PHY_NOCS];
   for (int d = 0; d < DIRECTIONS; d++)
      for (int p = 0; p < PHY_NOCS; p++)
         already_routed[d*PHY_NOCS+p] = false;

   // Route in a round-robin fashion between channels at the granularity of a flit
   u_int16_t end_q = next_q+NUM_QUEUES;
   // Iterate over the channels to see where the ports want to go
   for (u_int16_t q = next_q; q < end_q; q++){
      int qid = q%NUM_QUEUES;
      // next_inp4out are reset on calc_inp4out
      u_int16_t next_inp4out[DIRECTIONS];
      moved[qid] = 0;
      u_int32_t collisions = 0;
      // Process all directions inside calc_inp4out
      want_to_move_accum += calc_inp4out(drain_X,drain_Y, qid, router_timer, &next_inp4out[0], collisions);
      input_collision[qid] = collisions;
      output_collision[qid] = 0;

      // Loop to iterate over the outputs ports
      for (int d = 0; d < DIRECTIONS; d++){
         // Cannot fit two is there for the case where we have a wide noc and we are trying to route two.
         output_full[qid][d] = (output_q[d] != NULL) && output_q[d][qid].cannot_fit_two();

         // Determine the input that an output (d) route should pop from
         if ( (output_q[d] != NULL) && !output_full[qid][d] && !already_routed[phy_noc(d,qid)] ){

            origin = open[d][qid];
            // A certain input (origin) is currently open to flow to an output (d)
            if (origin < DIR_NONE){
               R_queue<Msg> * iq = input_q[origin];
               //There is an extra collision if some input head was granted this (d)irection
               if (next_inp4out[d] < DIR_NONE) input_collision[qid]++;

               //If the route is open, messages come back to back, so no need to check time
               //If we use a single NOC, it might be that a route is open but messages don't come back to back
               // But flits of different messages within the same channel shouldn't interleave 
               if (!input_empty[origin][qid]){
                  Msg msg = iq[qid].dequeue();
                  ASSERT_MSG(msg.type != HEAD, "Head on open route qid: " << qid);
                  output_q[d][qid].enqueue(msg); moved[qid]++;
                  // Ignore latency result since time is carried by the HEAD flit
                  check_border_traffic(d,qid,&already_routed[0]);
                  if (wide_noc(qid) && (msg.type != TAIL) && (!iq[qid].is_empty()) ){
                     msg = iq[qid].dequeue();
                     output_q[d][qid].enqueue(msg); moved[qid]++;
                     check_border_traffic(d,qid,&already_routed[0]);
                  }
                  // Close the route
                  if (msg.type == TAIL) open[d][qid]=DIR_NONE;
                  // Bump the priority channel id
                  next_q = (qid+1)%NUM_QUEUES;
               } 
            } else { //Not Open Route
               origin = next_inp4out[d];
               // If someone (origin) requested to be routed to this output (d)
               if (origin < DIR_NONE){
                  //If the origin input wants to be routed to a specific d(estination)
                  ASSERT_MSG(!input_empty[origin][qid], "Must not be empty");
                  
                  bool t3_oq_not_full = true;
                  #if PROXY_FACTOR == 1
                     bool t3_to_core = false;
                  #else
                     bool t3_to_core = (qid==2 || qid==3) && (d==C); // T3' to Core or T3 to Core, need to check due to proxy
                     #if PROXY_ROUTING==5
                     if (t3_to_core){
                        Msg pre_msg = input_q[origin][qid].peek();
                        int dst_X, dst_Y; get_dst(pre_msg.data, dst_X, dst_Y);
                        bool owner_core = (dst_X == id_X && dst_Y == id_Y) && !output_q[C][2].cannot_fit_two();
                        if (!owner_core) t3_oq_not_full = !output_q[C][3].cannot_fit_two();
                     }
                     #endif
                  #endif

                  R_queue<Msg> * iq = input_q[origin];
                  // t3_to_core move should move all the flits at once
                  if (!t3_to_core || (iq[qid].occupancy() > 1) && t3_oq_not_full){
                     u_int16_t hop_latency = check_border_traffic(d,qid,&already_routed[0]);
                     Msg msg = iq[qid].dequeue();
                     msg.time = noc_to_pu_cy(router_timer+hop_latency);

                     u_int16_t qid_dest = qid;
                     if (t3_to_core){ // input occupancy must have been more than 1
                        int dst_X, dst_Y;
                        get_dst(msg.data, dst_X, dst_Y);
                        // If it was coming from T3' but T3 IQ is full, then it goes to T3' even though it's the owner. That's okay because the owner could also be a proxy of itself!
                        bool owner_core = (dst_X == id_X && dst_Y == id_Y) && !output_q[C][2].cannot_fit_two();
                        // if it's the real dest then qid=2, else qid=3
                        if (owner_core) qid_dest = 2;
                        else qid_dest = 3; // If coming from ch-2 then check that ch-3 is not busy!
                        ASSERT_MSG(!output_q[C][qid_dest].cannot_fit_two(), "Must be able to fit in output queue");
                        ASSERT_MSG(msg.type != TAIL, "Msg cannot be TAIL");
                        ASSERT_MSG(!iq[qid].is_empty(), "IQ cannot be empty");
                     }

                     output_q[d][qid_dest].enqueue(msg); moved[qid]++;

                     // Update Round Robin for both Nocs and Input Ports (origin qid)
                     next_d[qid] = (d+1)%DIRECTIONS; next_q = (qid+1)%NUM_QUEUES;

                     if (t3_to_core || (wide_noc(qid) && (msg.type != TAIL) && !iq[qid].is_empty()) ){
                        msg = iq[qid].dequeue();
                        output_q[d][qid_dest].enqueue(msg); moved[qid]++;
                        if (msg.type == MID) open[d][qid]=origin;
                     }
                     else if (msg.type == HEAD) open[d][qid]=origin; //Open route if HEAD, but not MONO

                  } // end of If enough messages
               }
            } //end else (not open router)
         } else if (output_full[qid][d]){ 
            output_collision[qid]++; 
            if (d==C){
               // Endpoint collisions
               end_collisions++;
            }
         } 

      } //end for-direction
   } //end for-qid
   
   
   uint64_t moved_accum = moved[0]+moved[1];
   uint64_t total_in_collisions = 0;
   uint64_t total_out_collisions = 0;
   for (int qid = 0; qid < NUM_QUEUES; qid++){
      total_in_collisions += input_collision[qid];
      total_out_collisions += output_collision[qid];
   }

   msg(0,moved_accum);
   msg(1,moved[2]);
   msg(2,moved[3]);
   msgcol(0,total_in_collisions);
   msgcol(1,total_out_collisions);
   msgcol(2,end_collisions);

   moved_accum += moved[2] + moved[3];
   if (moved_accum > 0) active[0] += 1;
   
   return want_to_move_accum + total_out_collisions;
}

RouterModel *** routers;

#endif /* __ROUTER_MODEL_BASIC_H__ */
