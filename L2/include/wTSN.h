
#ifndef _WTSN_H
#define _WTSN_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "global_def.h"
#include "log.h"
#include "prj_config.h"

#define WIAT_TIME_WTSN 1000
#define WTSN_OFFSET 0

#define bzero(s,n) (memset((s),0,(n)))

// wTSN
#define WTSN_OFFSET_BUFFER_LEN 10
#define WTSN_SYNC_NUM 10
#define WTSN_MEAN_PER 3000
#define SAMPLE_CONVER 5
#define WTSN_SAMPLE_TIME_FACTOR (1000/153.6)

//#define WTSN_T2SUBT1 3000
//#define WTSN_T3SUBT2 3000
//#define WTSN_T4SUBT3 3000

#define WTSN_SYNC_PER 3000

#define WTSN_FREQ_POINT (73.0/2)

 uint8_t* g_tx_buffer_wtsn;
 uint8_t* g_rx_buffer_wtsn_followup;
 uint8_t* g_rx_buffer_wtsn_delayresp;

//wTSN 
typedef enum {
  P2P_SIB = 0x1,
  P2P_FOLLOW_UP = 0x2,
  P2P_DELAY_RESP = 0x3,
  P2P_INVALUE = 0x200
}P2pMsgType;

//wTSN  WTSN_TIMESTAMP_STRU作为输入，WTSN_TIMEFRE_OFFSET_STRU的结果作为中间值、输出
typedef struct {
    int64_t wT1stamp;
    int64_t wT2stamp;    
    int64_t wT3stamp;
    int64_t wT4stamp;
    int64_t wT2stamp_real;
    int64_t wT4stamp_real; 
     //Alpha Flag
    uint32_t uwoffset_AlphaFlg;
    int64_t  wtsn_offsetHis;
    int64_t  wtsn_offset;    
}
WTSN_TIMESTAMP_STRU;


typedef struct {
    double* wtsn_offset;
    double* wT1stamp;
    double* wT2stamp;
    double* wT3stamp;
    double* wT4stamp;
    double   wtsn_fFreAdj;
    uint8_t  wtsn_fFreAdj_point;
    double* T_timestamp;
    double* Ts_init_matrix;  //2*WTSN_SYNC_NUM 列向量
    double* Tp_init_matrix;  //2*WTSN_SYNC_NUM行   2列  矩阵
    double* Vtp_inter_var;  //中间结果 2*2
    double* Utp_inter_var;  //中间结果 2*2
    double* Stp_inter_var;  //2*WTSN_SYNC_NUM行  2*WTSN_SYNC_NUM列
    double* Utp_mult_Tp;
    double* Tp_mult_Utp_Tp;
    double* One_mult_Stp;
    double* Ts_mult_Stp;
}
WTSN_TIMEFRE_OFFSET_STRU;

//wTSN SIB
typedef struct 
{
  uint8_t  MessageType;
  uint32_t wFrmIdx;
  uint32_t wSubFrmIdx;
  uint32_t wSlotIdx;

}WTSN_SIB_MESSAGE;   //SIB

typedef struct 
{
  uint8_t  MessageType;
  int64_t  wtimestamp;

}WTSN_P2P_MESSAGE;   //Follow_up,Delay_resp


void wTSN_init(void);
void wTSN_free(void);
int proc_wtsn_mesg(uint8_t* inputBuffer);
uint8_t* get_wtsn_mesg (void);
int wtsn_nb_deal();
uint8_t wtsn_P2PTimeoffset(WTSN_TIMESTAMP_STRU *wtsn_Timestamp);
uint8_t wtsn_P2Pfreqoffset(WTSN_TIMEFRE_OFFSET_STRU *wtsn_TimeFreoffset);
uint32_t wtsn_P2P_ClockAdjust(WTSN_TIMESTAMP_STRU *wtsn_Timestamp,WTSN_TIMEFRE_OFFSET_STRU *wtsn_TimeFreoffset);
int wtsn_ue_deal();
//static void* wtsn_P2P_func( void* param);
int init_wtsn_tran_proc(void);
int kill_wtsn_tran_proc(void);
int wTSN_SIB_recv(uint8_t* inputBuffer, int wSysFrame,int wSysSubFrame,int wSysSlot);
int wTSN_SIB_send( int targtFrame,  int targtSubFrame, int targtSubSlot, int ue_idx);

#endif
