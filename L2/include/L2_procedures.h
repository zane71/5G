
#ifndef _L2_PROCEDURES_H
#define _L2_PROCEDURES_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>

#include "global_def.h"
#include "log.h"
#include "wTSN.h"
#include "time_manage.h"

#define bzero(s,n) (memset((s),0,(n)))

typedef struct {
    uint8_t usMacChannelType;
    uint8_t usSubIndex;
    uint16_t usMacTotalLenBytes;
    
    uint16_t usPacketSN;
    uint8_t usThisLen;
    uint8_t ucFirstData;
} MAC_HEADER_STRU;


typedef struct {
    MAC_HEADER_STRU MacHeader;
    uint8_t aucMacOffload[MAC_DATA_LEN];
} MAC_STRU;

typedef enum {
    SAFETY_CHUNNEL,
    DATA_CHUNNEL,
    MAX_CHUNNEL
}
Mac_Chunnel_t;

int Rx_Mac(int wSysFrame,int wSysSubFrame,int wSysSlot, uint8_t* rx_mac_date , int mac_date_len, int* sib_flag);
int Tx_Mac(int wSysFrame,int wSysSubFrame,int wSysSlot, int *targtFrame, int *targtSubFrame, int *targtSubSlot, int* channelMask);
int init_tx_mac_buffer(void);
int free_tx_mac_buffer(void);
uint8_t* get_tx_mac_pnt(uint8_t idx);
bool can_rxtx(int wSysFrame, int wSysSubFrame, int wSysSlot);
void get_rxtx_ueids(int wSysFrame, int wSysSubFrame, int wSysSlot, int ueids[MAX_SUB_CARRIER_UE_NUM], int* channelMask);
bool is_sib_msg_slot_nb(int wSysFrame, int wSysSubFrame, int wSysSlot);
bool is_sib_msg_slot_ue(int wSysFrame, int wSysSubFrame, int wSysSlot);
// 在当前帧上进行检测
bool is_sib_dect_frame(int wSysFrame);
bool is_multi_freq_mode();
int8_t get_sib_channel();
#endif
