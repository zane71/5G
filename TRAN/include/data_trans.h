#ifndef _DATA_TRANS_H
#define _DATA_TRANS_H

#include "global_def.h"
#include "log.h"
#include "udp.h"
#include "tcp.h"

#define MAX_BUFF_NUM 3000
#define UDP_BUFF_NUM 300
#define UDP_BUF_SIZE (UDP_BUFF_NUM*MAC_DATA_LEN)
#define PF_BUF_SIZE (UDP_BUFF_NUM*(MAC_DATA_LEN+14))
#define BLACKLIST_NUM 3 
#define RETRANS_NUM 2 
#define SEND_STORE_NUM 5
#define UDP_CLEAR_SEND_BUFF_TIMES 5000
#define TCP_CLEAR_SEND_BUFF_TIMES 5000

#define MAX_TCP_SERVER 4 
#define MAX_TCP_CLIENT 4 
#define MAX_LISTEN 5
#define MAX_TCP_SEND_TRY_TIMES 3 
#define TCP_BUFFER_TY 0

#define SYNC_DETAY_TIME (153600*2) //5ms

uint32_t GET_TRAN_DIFF_STATE(uint32_t StateItem);
uint32_t GET_TRAN_STATE(uint32_t StateItem);
void STATE_TRAN_CNT(uint32_t StateItem);
typedef enum {
  BUFF_EMPTY,
  BUFF_HAVE_DATA
}Buff_Type_t;

typedef struct {
    uint8_t isHaveData;
    uint16_t now_packet_sn;
    uint16_t send_colBuffer_len;
    uint8_t* socket_send_buffer;
    uint8_t* pSendStore[UDP_BUFF_NUM];
    uint8_t SubIndexRecvFlag[UDP_BUFF_NUM];
}SOCKET_SEND_BUFF_STRU;

typedef struct {
    uint8_t usChannelPriority;
    uint16_t usLocalPort;
    char auLocalIP[20];
    uint16_t usPeerPort;
    char auPeerIP[20];
    char srcNetcard[20];
    char dstNetcard[20];
    uint8_t TransType;
    int SendSockfd;
    int RecvSockfd;
    uint16_t PocketSN;
    int phRxDealIndex;
    int phSendIndex;
    int phTxDealIndex;
    int phRecvIndex;
    uint8_t* g_tx_buffer_queue[MAX_BUFF_NUM];
    uint8_t* g_rx_buffer_queue[MAX_BUFF_NUM];

    pthread_mutex_t udp_recv_mutex;
    pthread_mutex_t udp_send_mutex;
    
    uint8_t Socket_send_buffer_finsh;
    uint8_t*  Socket_send_buffer;
    uint16_t Socket_send_len;
    uint16_t blacklist[BLACKLIST_NUM];
    uint8_t blacklist_index;

    uint16_t clear_buff_times;
    uint32_t buffclearIdx;

    SOCKET_SEND_BUFF_STRU auSendStore[SEND_STORE_NUM];
} MAC_CHANNEL_STRU;


typedef enum {
  NOT_USE,
  UDP_NORMAL,
  UDP_MULTICAST,
  UDP_BROADCAST,
  TCP_CLIENT,
  TCP_SERVER,
  TIME_SYNC_MESG,
  PF_NORMAL,
  PF_NORMAL_L3
}TransType_t;

typedef enum {
  UDP_T,
  TCP_T
}IPType_t;

typedef enum {
  STOP_CMD = 6,
  START_CMD,
  NONE_CMD
}Socket_Cmd_t;

typedef struct {
     uint32_t stampIndex;
     long long int stamptime1;
     long long int stamptime2;
     long long int stamptime3;
     long long int stamptime4;
     // TODO:zach
     long long int send_slot;
}TIME_STAMP_STRU;

typedef struct {
    int socketfd;
    int prion;
}Recv_Input_t;

int set_send_buffer(uint8_t* inputBuffer);
uint8_t* get_recv_buffer(int ue_idx);

int init_data_tran_proc(int stamp_swtich);
int kill_data_tran_proc(void);
void tcp_accept_deal(int channelId);



#endif

