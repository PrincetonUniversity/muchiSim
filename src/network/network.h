

#if BOARD_W < GRID_X
void board_arbitration_X(int i, int pY, int bY, u_int32_t * ports_arbitration){
  int board_id = pY * BOARD_FACTOR + (i / BOARD_W);
  #if ASSERT_MODE
  assert(board_id < BOARDS);
  #endif
  int arbiter_id = board_id*BOARD_BUSES + bY;
  int rr = ports_arbitration[arbiter_id];
  bool taken = false;

  for (int m=rr; m<rr+MUX_BUS; m++){ //mux within bus
    int tile_within_pY = bY * MUX_BUS + m%MUX_BUS; //tile within boardY
    int tile_Y = pY*BOARD_H + tile_within_pY;
    if (!taken && routers[i][tile_Y]->route_board_X){taken=true; ports_arbitration[arbiter_id] = (m+1)%MUX_BUS; }
    else {routers[i][tile_Y]->route_board_X = false;}
    assert(routers[i][tile_Y]->board_id_Y==pY);
  }
}
void board_arbitration_Y(int j, int pX, int bX, u_int32_t * ports_arbitration){
  int board_id = (j / BOARD_H) * BOARD_FACTOR + pX;
  #if ASSERT_MODE
  assert(board_id < BOARDS);
  #endif
  int arbiter_id = board_id*BOARD_BUSES + bX;
  int rr = ports_arbitration[arbiter_id];
  bool taken = false;

  for (int m=rr; m<rr+MUX_BUS; m++){ //mux within bus
    int tile_within_pX = bX * MUX_BUS + m%MUX_BUS;
    int tile_X = pX*BOARD_W + tile_within_pX;
    if (!taken && routers[tile_X][j]->route_board_Y){taken=true; ports_arbitration[arbiter_id] = (m+1)%MUX_BUS; }
    else {routers[tile_X][j]->route_board_Y = false;}
    assert(routers[tile_X][j]->board_id_X==pX);
  }
}
#endif

#include "router_thread.h"
#include "tsu_core_thread.h"

void connect_routers () {
  for (int i = 0; i < BOARDS; i++) {
    for (int j = 0; j < BOARD_BUSES; j++) {
      board_arbitration_N[i*BOARD_BUSES+j] = 0;
      board_arbitration_S[i*BOARD_BUSES+j] = 0;
      board_arbitration_E[i*BOARD_BUSES+j] = 0;
      board_arbitration_W[i*BOARD_BUSES+j] = 0;
    }
  }
  for (int i=0; i<GRID_X;i++){
    for (int j=0; j<GRID_Y;j++){
      #if R_DEBUG
      cout << "CONNECT " << routers[i][j]->id_X << "," << routers[i][j]->id_Y << ".\n"; 
      #endif
      routers[i][j]->output_q[C] = routers[i][j]->output_core;

      u_int16_t board_id_X = i / BOARD_W;
      u_int16_t board_id_Y = j / BOARD_H;
      u_int16_t board_origin_X = board_id_X * BOARD_W;
      u_int16_t board_origin_Y = board_id_Y * BOARD_H;
      routers[i][j]->output_q[S] = routers[i][(j+1)%BOARD_H + board_origin_Y]->input_q[N];
      routers[i][j]->output_q[N] = routers[i][(j+BOARD_H-1)%BOARD_H + board_origin_Y]->input_q[S];
      routers[i][j]->output_q[E] = routers[(i+1)%BOARD_W + board_origin_X][j]->input_q[W];
      routers[i][j]->output_q[W] = routers[(i+BOARD_W-1)%BOARD_W + board_origin_X][j]->input_q[E];
      #if RUCHE_X > 0
        routers[i][j]->output_q[RS] = routers[i][(j+RUCHE_Y)%BOARD_H + board_origin_Y]->input_q[RN];
        routers[i][j]->output_q[RN] = routers[i][(j+BOARD_H-RUCHE_Y)%BOARD_H + board_origin_Y]->input_q[RS];
        routers[i][j]->output_q[RE] = routers[(i+RUCHE_X)%BOARD_W + board_origin_X][j]->input_q[RW];
        routers[i][j]->output_q[RW] = routers[(i+BOARD_W-RUCHE_X)%BOARD_W + board_origin_X][j]->input_q[RE];
      #endif

      // Only designed for Torus
      #if BOARD_W < GRID_X
      if ((j%BOARD_H) == 0) {
        routers[i][j]->output_q[PY] = routers[i][(j+GRID_Y-BOARD_H)%GRID_Y]->input_q[PY];
      }else if ((j%BOARD_H) == BOARD_H_HALF) {
        routers[i][j]->output_q[PY] = routers[i][(j+BOARD_H)%GRID_Y]->input_q[PY];
      }
      
      if ((i%BOARD_W) == 0) {
        routers[i][j]->output_q[PX] = routers[(i+GRID_X-BOARD_W)%GRID_X][j]->input_q[PX];
      }else if ((i%BOARD_W) == BOARD_W_HALF) {
        routers[i][j]->output_q[PX] = routers[(i+BOARD_W)%GRID_X][j]->input_q[PX];
      }
      #endif
    }
  }
}

void connect_mesh(){
  routers = new RouterModel**[GRID_X];
  for (int i=0; i< GRID_X; i++){
    routers[i] = new RouterModel*[GRID_Y];
    for (int j=0; j< GRID_Y; j++){
      routers[i][j] = new RouterModel(i, j);
    }
  }
  connect_routers();
}