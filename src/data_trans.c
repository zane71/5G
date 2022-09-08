
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sched.h>
#include <math.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <time.h>
#include <limits.h>
#include <sched.h>
#include <sys/socket.h>

#include "data_trans.h"
#include "prj_config.h"
#include "L2_procedures.h"
#include "rt_wrapper.h"
#include "global_def.h"
#include "common_lib.h"

#include "threadpackage.h"

extern int oai_exit;
extern EIOT_properties_t eiot_properties;
extern Logical_ue_map_t logical_ue_map;
extern uint32_t Get_TBSize_Byte();

#define INVALID_SN (-1)
#define RELEASE_SN1 0
#define RELEASE_SN2 1
extern openair0_device openair0;
int gstamp_swtich = 1;

pthread_t udp_nom_recv_data_pthread;
pthread_t udp_mb_recv_data_pthread;
pthread_t udp_nom_send_data_pthread;
pthread_t udp_mb_send_data_pthread;
pthread_t tcp_server_pthread[MAX_TCP_SERVER];
pthread_t tcp_client_pthread[MAX_TCP_CLIENT];
pthread_t pf_nom_recv_data_pthread;
pthread_t pf_nom_send_data_pthread;


thread_together* udp_sync_pthread;
thread_together* udp_sync_recv_pthread;

MAC_CHANNEL_STRU gChannel[MAX_SOCKET_CHANNEL_NUM];

/*
const MAC_CHANNEL_STRU g_temp_info[MAX_CHANNEL_NUM] = 
{
    {0,54231,"172.16.1.1",54230,"172.16.1.101",0,0,0,0,0,0,0,0,NULL,NULL},
    {1,320,"172.16.1.10",321,"224.0.1.129",1,0,0,0,0,0,0,0,NULL,NULL},
    {2,319,"172.16.1.8",322,"224.0.1.129",0,0,0,0,0,0,0,0,NULL,NULL}
};*/

//UDP包统计
uint32_t g_TranStatistics[TRAN_STATISTICS_BUTT] = {0};
uint32_t g_TranStatistics_His[TRAN_STATISTICS_BUTT] = {0};

uint8_t* gReTransBuff[UDP_BUFF_NUM]; 

int tcp_server_channel[MAX_SOCKET_CHANNEL_NUM];
int tcp_client_channel[MAX_SOCKET_CHANNEL_NUM];
int time_sync_channel[MAX_SOCKET_CHANNEL_NUM];
int tcp_server_num = 0;
int tcp_client_num = 0;
int time_sync_num = 0;
extern int UE_ID;

uint8_t peer_socket_cmd = NONE_CMD;
uint8_t peer_channel = INVALD_CHANNEL_NO;
uint8_t local_socket_cmd = NONE_CMD;
uint8_t local_channel = INVALD_CHANNEL_NO;

void STATE_TRAN_CNT(uint32_t StateItem)
{
    if(TRAN_STATISTICS_BUTT > StateItem)
    {
        g_TranStatistics[StateItem]++;
    }
    return ;
}

uint32_t GET_TRAN_STATE(uint32_t StateItem)
{
    if(TRAN_STATISTICS_BUTT > StateItem)
    {
        return g_TranStatistics[StateItem];
    }
    return 0;
}

uint32_t GET_TRAN_DIFF_STATE(uint32_t StateItem)
{
    uint32_t uwDiff = 0;
    if(TRAN_STATISTICS_BUTT > StateItem)
    {
        uwDiff= g_TranStatistics[StateItem] - g_TranStatistics_His[StateItem];
        g_TranStatistics_His[StateItem] = g_TranStatistics[StateItem];
        return uwDiff;
    }
    return 0;
}
//打印统计信息
uint32_t Show_Tran_State()
{
    uint32_t uwStateItemLoop;

    printf("---------------------TRAN Statistic-----------------------------------------\n");
    
    printf("TRAN_ETH_REV_CNT  :[%d].\n",GET_TRAN_STATE(TRAN_ETH_REV_CNT));
    printf("TRAN_LOGIC_CH0_CNT:[%d].\n",GET_TRAN_STATE(TRAN_REV_LOGIC_CH0_CNT));
    printf("TRAN_LOGIC_CH1_CNT:[%d].\n",GET_TRAN_STATE(TRAN_REV_LOGIC_CH1_CNT));
    printf("TRAN_LOGIC_CH2_CNT:[%d].\n",GET_TRAN_STATE(TRAN_REV_LOGIC_CH2_CNT));
    printf("TRAN_LOGIC_CH0_CNT:[%d].\n",GET_TRAN_STATE(TRAN_REV_LOGIC_CH3_CNT));
    printf("TRAN_LOGIC_CH1_CNT:[%d].\n",GET_TRAN_STATE(TRAN_REV_LOGIC_CH4_CNT));
    printf("TRAN_LOGIC_CH2_CNT:[%d].\n",GET_TRAN_STATE(TRAN_REV_LOGIC_CH5_CNT));
    printf("TRAN_LOGIC_CH1_CNT:[%d].\n",GET_TRAN_STATE(TRAN_REV_LOGIC_CH6_CNT));
    
    printf("TRAN_ETH_SND_CNT:[%d].\n",GET_TRAN_STATE(TRAN_ETH_SND_CNT));
    
    printf("---------------------PHY Statistic end------------------------------------\n");

}

//注意ChunnelPriority和usMacChunnelType是一一对应的
int Channel_init(void)
{
    int result;  
    uint32_t uwTbSizeBytes = MAC_LEN;
    uint32_t uwMAcBodyBytes = MAC_DATA_LEN;
    int fd =0;

    setup_sig_handlers();
        
    for(int i = 0;i < MAX_SOCKET_CHANNEL_NUM ;i++)
    {
        gChannel[i].usChannelPriority = i;
        gChannel[i].TransType = eiot_properties.astruChInit[i].TransType;
        gChannel[i].usLocalPort = eiot_properties.astruChInit[i].usLocalPort;
        gChannel[i].usPeerPort = eiot_properties.astruChInit[i].usPeerPort;
        memcpy(gChannel[i].auPeerIP, eiot_properties.astruChInit[i].auPeerIP, 20);
        memcpy(gChannel[i].auLocalIP, eiot_properties.astruChInit[i].auLocalIP, 20);
        // row socket 使用网卡表示通道
        if (i == MAX_SOCKET_CHANNEL_NUM - 1) {
            memcpy(gChannel[i].srcNetcard, eiot_properties.astruChInit[i].srcNetcard, 20);
            memcpy(gChannel[i].dstNetcard, eiot_properties.astruChInit[i].dstNetcard, 20);
        }

        //如果port == 0则这个上行或者下行的通道不使用

        gChannel[i].SendSockfd = -1;
        gChannel[i].RecvSockfd = -1;
        switch (gChannel[i].TransType) {
            case TIME_SYNC_MESG:
                printf("create channel:TIME_SYNC_MESG\n");
                fd = create_udp_socket();
                if(fd == -1){
                    perror("create_udp_socket failed");
                    return -1;
                }
                
                if(gChannel[i].usPeerPort > 0)
                {
                    gChannel[i].SendSockfd = fd;
                 }

                if(gChannel[i].usLocalPort > 0)
                {
                    gChannel[i].RecvSockfd =  fd;
                    
                    if(udp_Multicast(gChannel[i].RecvSockfd, gChannel[i].auPeerIP, gChannel[i].usLocalPort) == -1){
                        perror("Bind error");
                        close_udp_socket(gChannel[i].RecvSockfd);
                        return -1;
                    }  
                }
                gChannel[i].clear_buff_times = UDP_CLEAR_SEND_BUFF_TIMES;

                time_sync_channel[time_sync_num] = i;
                time_sync_num++;
                break;

            
            case UDP_NORMAL:
                printf("create channel:UDP_NORMAL\n");
                fd = create_udp_socket();
                if(fd == -1){
                    perror("create_udp_socket failed");
                    return -1;
                }
                    
                if(gChannel[i].usPeerPort > 0)
                {
                    gChannel[i].SendSockfd = fd;
                }

                if(gChannel[i].usLocalPort > 0)
                {
                    gChannel[i].RecvSockfd = fd;
    
                    //bind to port, ip may many
                    //if(udp_bind_ip(gChannel[i].RecvSockfd, gChannel[i].usLocalPort) == -1){
                    // bind to ip and port
                    if(udp_bind_ip(gChannel[i].RecvSockfd, gChannel[i].usLocalPort, gChannel[i].auLocalIP) == -1){
                        perror("Bind error");
                        close_udp_socket(gChannel[i].RecvSockfd);
                        return -1;
                } 
	            //for test	printf("i:%d,auPeerIP:%s,usPeerPort:%d,auLocalIp:%s,usLocalPort:%d---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------, \n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n",i,gChannel[i].auPeerIP,gChannel[i].usPeerPort,gChannel[i].auLocalIP,gChannel[i].usLocalPort);//by wxy 
                }
                gChannel[i].clear_buff_times = UDP_CLEAR_SEND_BUFF_TIMES;
                break;
                
            case UDP_MULTICAST:
                printf("create channel:UDP_MULTICAST\n");  
                fd = create_udp_socket();
                if(fd == -1){
                    perror("create_udp_socket failed");
                    return -1;
                }
                
                if(gChannel[i].usPeerPort > 0)
                {
                    gChannel[i].SendSockfd = fd;
                 }

                if(gChannel[i].usLocalPort > 0)
                {
                    gChannel[i].RecvSockfd =  fd;
                    
                    if(udp_Multicast(gChannel[i].RecvSockfd, gChannel[i].auPeerIP, gChannel[i].usLocalPort) == -1){
                        perror("Bind error");
                        close_udp_socket(gChannel[i].RecvSockfd);
                        return -1;
                    }  
                }
                gChannel[i].clear_buff_times = UDP_CLEAR_SEND_BUFF_TIMES;
                break;

            case UDP_BROADCAST:
                printf("create channel:UDP_BROADCAST\n");  
                if(gChannel[i].usPeerPort > 0)
                {
                    gChannel[i].SendSockfd = create_udp_socket();
                    if(gChannel[i].SendSockfd == -1){
                        perror("Create SendSockfd failed");
                        return -1;
                    }
                    int optval = 1;
                    if (setsockopt(gChannel[i].SendSockfd, SOL_SOCKET,SO_BROADCAST,(char *)&optval,sizeof(optval))< 0)
                    {
                        perror("setsockopt():IP_ADD_MEMBERSHIP");
                        return -1;
                    }
                }
                
                if(gChannel[i].usLocalPort > 0)
                {
                    gChannel[i].RecvSockfd = create_udp_socket();
                    if(gChannel[i].RecvSockfd == -1){
                        perror("Create RecvSockfd failed");
                        return -1;
                    }
                    if(udp_Broadcast(gChannel[i].RecvSockfd,  gChannel[i].usLocalPort) == -1){
                        perror("Bind error");
                        close_udp_socket(gChannel[i].RecvSockfd);
                        return -1;
                    }  
                }
                gChannel[i].clear_buff_times = UDP_CLEAR_SEND_BUFF_TIMES;
                break;

            case TCP_SERVER:
                //TCP not init here
                printf("create channel:TCP_SERVER\n");  
                tcp_server_channel[tcp_server_num] = i;
                tcp_server_num++;
                gChannel[i].clear_buff_times = TCP_CLEAR_SEND_BUFF_TIMES;
                break;
                
            case TCP_CLIENT:
                printf("create channel:TCP_CLIENT\n");  
                tcp_client_channel[tcp_client_num] = i;
                tcp_client_num++;
                gChannel[i].clear_buff_times = TCP_CLEAR_SEND_BUFF_TIMES;
                //TCP not init here
                break;

            case PF_NORMAL:
                printf("create channel:PF_NORMAL\n");
                fd = create_pf_socket();
                if (fd == -1) {
                    perror("create_pf_socket failed");
                    return -1;
                }
                gChannel[i].SendSockfd = fd;
                gChannel[i].RecvSockfd = fd;
                // "eno1" stand for a network card
                if (pf_bind(gChannel[i].RecvSockfd, gChannel[i].srcNetcard) == -1) { 
                    perror("Bind error");
                    close_pf_socket(gChannel[i].RecvSockfd);
                    return -1;
                }
                gChannel[i].clear_buff_times = UDP_CLEAR_SEND_BUFF_TIMES;
                break;
            
            /*
            case PF_NORMAL_L3:
                fd = create_pf_l3_socket();
                if (fd == -1) {
                    perror("create_pf_l3_socket failed");
                    return -1;
                }
                gChannel[i].SendSockfd = fd;
                gChannel[i].RecvSockfd = fd; 
                if (pf_bind(gChannel[i].RecvSockfd) == -1) { 
                    perror("Bind error");
                    close_pf_socket(gChannel[i].RecvSockfd);
                    return -1;
                }
                gChannel[i].clear_buff_times = UDP_CLEAR_SEND_BUFF_TIMES;
                break;
            */
            case NOT_USE:
                continue;
                break;
                
            default:
                break;
        }

        gChannel[i].buffclearIdx = 0;
        
        for(int qn = 0;qn < MAX_BUFF_NUM ;qn++)
        {
            gChannel[i].g_tx_buffer_queue[qn] = (uint8_t*)malloc(uwTbSizeBytes*sizeof(uint8_t));
        }

        for(int qn = 0;qn < MAX_BUFF_NUM ;qn++)
        {
            gChannel[i].g_rx_buffer_queue[qn] = (uint8_t*)malloc(uwTbSizeBytes*sizeof(uint8_t));
        }
        
        gChannel[i].Socket_send_buffer_finsh = 0;
        gChannel[i].Socket_send_buffer = NULL;
        gChannel[i].Socket_send_len = 0;

        for(int bn = 0;bn < BLACKLIST_NUM ;bn++)
        {
            gChannel[i].blacklist[bn] = 44444;
        }

        gChannel[i].blacklist_index = 0;

        pthread_mutex_init(&gChannel[i].udp_recv_mutex, NULL);
        pthread_mutex_init(&gChannel[i].udp_send_mutex, NULL);


        for(int bn = 0;bn < SEND_STORE_NUM ;bn++)
        {
             gChannel[i].auSendStore[bn].isHaveData = BUFF_EMPTY;
             gChannel[i].auSendStore[bn].now_packet_sn = 0;
             gChannel[i].auSendStore[bn].send_colBuffer_len = 0;
             memset(gChannel[i].auSendStore[bn].SubIndexRecvFlag , 0, UDP_BUFF_NUM);
             gChannel[i].auSendStore[bn].socket_send_buffer = (uint8_t*)malloc(UDP_BUF_SIZE*sizeof(uint8_t));

             for(int kn = 0;kn < UDP_BUFF_NUM;kn++)
            {
                gChannel[i].auSendStore[bn].pSendStore[kn] = gChannel[i].auSendStore[bn].socket_send_buffer + kn*MAC_DATA_LEN;
            }
             
        }
        
        printf("[Channel_init] idx:%d,ChPri:%d,L_ip:%s,L_port:%d,Peer_ip:%s,Peer_port:%d\n",i,gChannel[i].usChannelPriority,
            gChannel[i].auLocalIP,gChannel[i].usLocalPort,gChannel[i].auPeerIP,gChannel[i].usPeerPort);
    }

    for(int ri = 0; ri < UDP_BUFF_NUM; ri++)
    {
        gReTransBuff[ri] = (uint8_t*)malloc(uwTbSizeBytes*sizeof(uint8_t));
    }
    
    return 0;
}


int Channel_free(void)
{
    for(int i = 0;i < MAX_SOCKET_CHANNEL_NUM ;i++)
    {
        if(gChannel[i].TransType  == NOT_USE)
        {
            continue;
        }
        
        for(int qn = 0;qn < MAX_BUFF_NUM ;qn++)
        {
            free(gChannel[i].g_tx_buffer_queue[qn]);
        }
        
        for(int qn = 0;qn < MAX_BUFF_NUM ;qn++)
        {
            free(gChannel[i].g_rx_buffer_queue[qn]);
        }

        for(int bn = 0;bn < SEND_STORE_NUM ;bn++)
        {
            free(gChannel[i].auSendStore[bn].socket_send_buffer);
        }

        if((gChannel[i].TransType == TCP_CLIENT) ||(gChannel[i].TransType == TCP_SERVER))
        {
            close_tcp_socket(gChannel[i].RecvSockfd);
        }
        else
        {
            close_udp_socket(gChannel[i].SendSockfd);
            close_udp_socket(gChannel[i].RecvSockfd);
        }

        pthread_mutex_destroy(&gChannel[i].udp_recv_mutex);
        pthread_mutex_destroy(&gChannel[i].udp_send_mutex);
      
    }
    
    return 0;
}

int set_socket_send_buffer(uint8_t* inputBuffer) {
    MAC_STRU* pMacBuff = (MAC_STRU*)inputBuffer;
    uint32_t uwTbSizeBytes = MAC_LEN;
    uint8_t tChannelType = pMacBuff->MacHeader.usMacChannelType;

    if(tChannelType >= MAX_SOCKET_CHANNEL_NUM)
    {
        printf("The send tChunnelType is over MAX_SOCKET_CHANNEL_NUM: %d \n", tChannelType);
        return -1;
    }

    if(INVALID_TYPE == tChannelType)
    {
        printf("The send tChunnelType is invalid\n");
        return 0;
    }
    
    if(TCP_SYNC_TYPE == tChannelType)
    {
        local_channel = pMacBuff->MacHeader.usSubIndex;
        local_socket_cmd = pMacBuff->MacHeader.usMacTotalLenBytes;
        if((local_channel != INVALD_CHANNEL_NO) &&(local_socket_cmd != NONE_CMD))
        {
            printf("CMD %d local_channel %d\n", local_socket_cmd, local_channel);
        }

        return 0;
    }
    
    STATE_TRAN_CNT(L2_REV_TRAFFIC_CNT);

    if(gChannel[tChannelType].SendSockfd == -1)
    {
        printf("error input tChannelType %d no send chunnel !!\n", tChannelType);
        return -1;
    }
    
    pthread_mutex_lock(&gChannel[tChannelType].udp_send_mutex);
    memcpy(gChannel[tChannelType].g_rx_buffer_queue[gChannel[tChannelType].phRxDealIndex],
            inputBuffer, uwTbSizeBytes);
    
    gChannel[tChannelType].phRxDealIndex++;
    if(gChannel[tChannelType].phRxDealIndex >= MAX_BUFF_NUM) 
    {
        gChannel[tChannelType].phRxDealIndex=0;
    }
    pthread_mutex_unlock(&gChannel[tChannelType].udp_send_mutex);
    return 0;
    
}

uint8_t* get_socket_recv_buffer(int ue_idx) {

    //从高优先级往低找, ue_idx == -1 表示从所有的channel查找
    uint8_t* pRet = NULL;
    //重传次数
    for(int prion = 0; prion< MAX_SOCKET_CHANNEL_NUM; prion++)
    {
        if ((ue_idx != -1) && (logical_ue_map.channel_ue_map[prion] != ue_idx)) {
            // 当前用户不收发数据
            continue;
        }
         if((gChannel[prion].TransType  == NOT_USE)||(gChannel[prion].RecvSockfd== -1))
        {
            continue;
        }
        
        pthread_mutex_lock(&gChannel[prion].udp_recv_mutex);
        if(gChannel[prion].phRecvIndex == gChannel[prion].phTxDealIndex)
        {
            pthread_mutex_unlock(&gChannel[prion].udp_recv_mutex);
            continue;
        }
        
        pRet = gChannel[prion].g_tx_buffer_queue[gChannel[prion].phTxDealIndex];    
        /*
        printf("[get_recv_buffer] prion[%d]Rev[%d]Deal[%d]P[%x] \n",prion,
                gChannel[prion].phRecvIndex, gChannel[prion].phTxDealIndex,pRet);
        */
        gChannel[prion].phTxDealIndex++;
        if(gChannel[prion].phTxDealIndex >= MAX_BUFF_NUM) 
        {
            gChannel[prion].phTxDealIndex=0;
        }
        pthread_mutex_unlock(&gChannel[prion].udp_recv_mutex);
        break;
    }
    return pRet;
    
}

void UDP_recv_stamp(uint8_t* Buf)
{
    TIME_STAMP_STRU* pstamp = (TIME_STAMP_STRU*)Buf;
    struct timespec lnow;
    clockid_t lclock_id = CLOCK_MONOTONIC;
    clock_gettime(lclock_id, &lnow);
    pstamp->stamptime2 = (lnow.tv_sec*(long long int)1000000000+lnow.tv_nsec);
}

void UDP_send_stamp(uint8_t* Buf)
{
    TIME_STAMP_STRU* pstamp = (TIME_STAMP_STRU*)Buf;
    struct timespec lnow;
    clockid_t lclock_id = CLOCK_MONOTONIC;
    clock_gettime(lclock_id, &lnow);
    pstamp->stamptime3 = (lnow.tv_sec*(long long int)1000000000+lnow.tv_nsec);
}

// 网卡net的mac
void get_test_ue_mac(uint8_t* tempmac) {
    tempmac[0] = 0x04;
    tempmac[1] = 0xd9;
    tempmac[2] = 0xf5;
    tempmac[3] = 0x02;
    tempmac[4] = 0xe5;
    tempmac[5] = 0xfc;
}


RTIME t85 = 0;
RTIME tmpT = 0;

int trans_recv_deal(MAC_CHANNEL_STRU* pChannel)
{  
    if(pChannel->TransType  == NOT_USE)
    {
        //not use channel
        return 0;
    }
    
    int recv_lens = 0;
    int recv_offest = 0;
    int recv_port = 0;
    int packet_num = 0;
    
    uint64_t rtc_time = 0;
    int RtcLen = sizeof(uint64_t);

    uint32_t uwTbSizeBytes = MAC_LEN;
    uint32_t uwMAcBodyBytes = MAC_DATA_LEN;

    uint8_t RecvBuf[UDP_BUF_SIZE];
    uint8_t RecvBufTmp[PF_BUF_SIZE];

    MAC_STRU tMacPck;
    uint8_t tempmac[6];
    uint8_t testmac[6];

    struct sockaddr_in cliaddr;
    int cliaddr_len = sizeof(cliaddr);
    int lSubIndex = 0;

    unsigned int rentest = 0;
    unsigned int* ptest = 0;

    if((pChannel->TransType == TCP_CLIENT) || (pChannel->TransType == TCP_SERVER))
    {
        recv_lens = tcp_recv(pChannel->SendSockfd, RecvBuf, UDP_BUF_SIZE);

        if(recv_lens < 0)
        {
            printf("tcp_recv error%s  reconnect\n",strerror(errno));
            return -1;
        }

        if(recv_lens == 0)
        {
            return 0;
        }
    }
    else
    {
        if (pChannel->TransType == TIME_SYNC_MESG) {
            recv_lens = recvfrom(pChannel->RecvSockfd, 
                    RecvBuf+RtcLen, (UDP_BUF_SIZE - RtcLen), 0, 
                    (struct sockaddr *)&cliaddr, (socklen_t *)&cliaddr_len);
        } else if (pChannel->TransType == PF_NORMAL) { // 用于数据转发
            recv_lens = recvfrom(pChannel->RecvSockfd, RecvBufTmp, PF_BUF_SIZE, MSG_TRUNC|MSG_DONTWAIT,NULL,NULL);
            if (recv_lens <= 0) {
                return 0;
            }
            if (recv_lens <= 14) {
                 printf("PF_NORMAL recv_len invalid!\n");
                 return 0;
            }
            get_test_ue_mac(testmac);
            if (get_mac_addr(pChannel->RecvSockfd, pChannel->srcNetcard, tempmac) != 0) {
                printf("get mac addr faild!\n");
                recv_lens = 0;
                return 0;
            }
            // reject: src MAC is local host
            if ((RecvBufTmp[6] == tempmac[0]) 
                    && (RecvBufTmp[7] == tempmac[1]) 
                    && (RecvBufTmp[8] == tempmac[2])
                    && (RecvBufTmp[9] == tempmac[3]) 
                    && (RecvBufTmp[10] == tempmac[4]) 
                    && (RecvBufTmp[11] == tempmac[5])) {
                recv_lens = 0;
                return 0;
            }
            // reject: dst mac is local host
            /*
            if ((RecvBufTmp[0] == tempmac[0]) 
                    && (RecvBufTmp[1] == tempmac[1]) 
                    && (RecvBufTmp[2] == tempmac[2])
                    && (RecvBufTmp[3] == tempmac[3]) 
                    && (RecvBufTmp[4] == tempmac[4]) 
                    && (RecvBufTmp[] == tempmac[5])) {
                recv_lens = 0;
                return 0;
            }
            */
            // TODO:used for test
            if (!((RecvBufTmp[6] == testmac[0])
                    && (RecvBufTmp[7] == testmac[1])
                    && (RecvBufTmp[8] == testmac[2])
                    && (RecvBufTmp[9] == testmac[3])
                    && (RecvBufTmp[10] == testmac[4])
                    && (RecvBufTmp[11] == testmac[5]))) {
                recv_lens = 0;
                return 0;
            }

            // roll back
            memcpy(RecvBuf, RecvBufTmp, recv_lens);
            memcpy(RecvBuf, RecvBufTmp + 6, 6);
            memcpy(RecvBuf + 6, RecvBufTmp, 6);
            memcpy(RecvBuf + 14 + 12, RecvBufTmp + 14 + 16, 4);
            memcpy(RecvBuf + 14 + 16, RecvBufTmp + 14 + 12, 4);

            /*
            // TODO test
            int result = pf_send_mesg(pChannel->SendSockfd, RecvBuf, 
                         recv_lens, pChannel->dstNetcard);
            if (result < 0) {
                perror("PF_send_mesg error \n");
            }
            */

        }
        /*
        else if (pChannel->TransType == PF_NORMAL_L3) {
            recv_lens = recvfrom(pChannel->RecvSockfd, RecvBufTmp, PF_BUF_SIZE, MSG_TRUNC|MSG_DONTWAIT,NULL,NULL);
            if (recv_lens <= 0) {
                return 0;
            }
            if (recv_lens <= 14) {
                 printf("PF_NORMAL recv_len invalid!\n");
                 return 0;
            }
            get_ue_local_mac_l3(UE_ID, tempmac);
            if ((RecvBufTmp[6] == tempmac[0]) 
                    && (RecvBufTmp[7] == tempmac[1]) 
                    && (RecvBufTmp[8] == tempmac[2])
                    && (RecvBufTmp[9] == tempmac[3]) 
                    && (RecvBufTmp[10] == tempmac[4]) 
                    && (RecvBufTmp[11] == tempmac[5])) {
                recv_lens = 0;
                return 0;
            }
            memcpy(RecvBuf,RecvBufTmp,recv_lens);

        } 
        */
        else { // udp resv
            recv_lens = recvfrom(pChannel->RecvSockfd, RecvBuf, 
                    UDP_BUF_SIZE, MSG_DONTWAIT, (struct sockaddr *)&cliaddr, (socklen_t *)&cliaddr_len);
        }

        if(recv_lens <= 0)
        {
            //没有收到任何东西继续
            return 0;
        } else {
            RecvBuf[recv_lens] = '\0';
        }
    }
    
    STATE_TRAN_CNT(TRAN_ETH_REV_CNT);
    if(cliaddr.sin_addr.s_addr == inet_addr(pChannel->auLocalIP))
    {
        //TODO:open local test, resv the data from local machine
        //return 0;
    }
    
    if(gstamp_swtich == 1)
    {
        UDP_recv_stamp(RecvBuf);
    }

    //对于同步channel 从FPGA获取时间戳

     if(pChannel->TransType == TIME_SYNC_MESG)
    {
        //获取RTC时间
        rtc_time = rtc_time_get(&openair0);
        memcpy(RecvBuf, &rtc_time, RtcLen);
        recv_lens = recv_lens+ RtcLen;
        
    }   
    
    while(recv_offest < recv_lens)
    {
        tMacPck.MacHeader.usMacChannelType = pChannel->usChannelPriority;
        tMacPck.MacHeader.usPacketSN = pChannel->PocketSN;
        tMacPck.MacHeader.usMacTotalLenBytes = recv_lens;
        tMacPck.MacHeader.usSubIndex = lSubIndex;
        //这里一定是数据的长度
        if((recv_offest + uwMAcBodyBytes) >= recv_lens)
        {
            tMacPck.MacHeader.usThisLen = recv_lens - recv_offest;
        }
        else
        {
            tMacPck.MacHeader.usThisLen = uwMAcBodyBytes;
        }
        tMacPck.MacHeader.ucFirstData = *(RecvBuf + recv_offest);
        memcpy(tMacPck.aucMacOffload, RecvBuf + recv_offest + 1, tMacPck.MacHeader.usThisLen);
        memcpy(pChannel->g_tx_buffer_queue[pChannel->phRecvIndex], (uint8_t*)&tMacPck, uwTbSizeBytes);
        //retrans
        memcpy(gReTransBuff[packet_num], (uint8_t*)&tMacPck, uwTbSizeBytes);
        pChannel->phRecvIndex++;
        if(pChannel->phRecvIndex >= MAX_BUFF_NUM) 
        {
            pChannel->phRecvIndex=0;
        }
        /* 
        //other retrans plan
        for(int ri = 0; ri < RETRANS_NUM; ri++)
        {
            memcpy(pChannel->g_tx_buffer_queue[pChannel->phRecvIndex], (uint8_t*)&tMacPck, uwTbSizeBytes);
            pChannel->phRecvIndex++;
            if(pChannel->phRecvIndex >= MAX_BUFF_NUM) 
            {
                pChannel->phRecvIndex=0;
            }
        }
        */
        
        recv_offest = recv_offest + uwMAcBodyBytes;
        lSubIndex++;
        packet_num++;
        STATE_TRAN_CNT(RLC_TX_QUE_CNT);
        
    }

    //重传
    for(int ki = 0; ki < packet_num; ki++)
    {
        memcpy(pChannel->g_tx_buffer_queue[pChannel->phRecvIndex], gReTransBuff[ki], uwTbSizeBytes);
        pChannel->phRecvIndex++;
        if(pChannel->phRecvIndex >= MAX_BUFF_NUM) 
        {
            pChannel->phRecvIndex=0;
        }
    }

    lSubIndex = 0;
    recv_offest = 0;
    packet_num = 0;
    pChannel->PocketSN++;

    return 0;
}

int blacklist_check(uint16_t now_sn, MAC_CHANNEL_STRU* pChannel)
{
    for(int bn = 0;bn < BLACKLIST_NUM ;bn++)
        {
            if(now_sn == pChannel->blacklist[bn])
            {
                return 1;
            }
        }

        return 0;
}

SOCKET_SEND_BUFF_STRU* get_send_buff_stru(uint16_t now_sn, SOCKET_SEND_BUFF_STRU* auSendStore, uint8_t Channeld)
{
    int bn , kn, pn, tmp_dt;
    int max_dt = 0;
    int max_sn = 0;

    //取出SN号相同的
    for(bn = 0;bn < SEND_STORE_NUM ;bn++)
    {
        if(auSendStore[bn].isHaveData == BUFF_EMPTY)
        {
            continue;
        }
        if(auSendStore[bn].now_packet_sn == now_sn)
        {
            return &(auSendStore[bn]);
        }
    }

    //new sn
    if(bn == SEND_STORE_NUM)
    {
        //找到一个没有使用的就放
        for(kn =0; kn < SEND_STORE_NUM ;kn++)
        {
            if(auSendStore[kn].isHaveData == BUFF_EMPTY)
            {
                auSendStore[kn].isHaveData = BUFF_HAVE_DATA;
                auSendStore[kn].now_packet_sn = now_sn;
                auSendStore[kn].send_colBuffer_len = 0;
                memset(auSendStore[kn].SubIndexRecvFlag , 0, UDP_BUFF_NUM);
                return &(auSendStore[kn]);
            }
        }

        //全都满了,找到离自己sn号差值最大的
        if(kn == SEND_STORE_NUM)
        {
            for(pn =0; pn < SEND_STORE_NUM ;pn++)
            {
                tmp_dt = abs(auSendStore[pn].now_packet_sn - now_sn);
                if( tmp_dt > max_dt)
                {
                    max_dt = tmp_dt;
                    max_sn = pn;
                }
            }

            //丢包打印
            
            printf("[get_send_buff_stru]!!!! MAC lost pocket Channeld %d !!!!SN :%d, Data Len: %d, now_sn %d\n",
                     Channeld, auSendStore[max_sn].now_packet_sn, auSendStore[max_sn].send_colBuffer_len, now_sn);
            
            auSendStore[max_sn].isHaveData = BUFF_HAVE_DATA;
            auSendStore[max_sn].now_packet_sn = now_sn;
            auSendStore[max_sn].send_colBuffer_len = 0;
            memset(auSendStore[max_sn].SubIndexRecvFlag , 0, UDP_BUFF_NUM);
            // TODO
            STATE_TRAN_CNT(TRAN_ETH_ERR_CNT);
            
            return &(auSendStore[max_sn]);
        }
        
    }

    return NULL;
}

void clear_store_buff(uint16_t now_sn, SOCKET_SEND_BUFF_STRU* auSendStore, uint8_t Channeld)
{
    int bn, tmp_dt = 0;

    if(now_sn <= 10* SEND_STORE_NUM)
    {
        //SN号过小时不做检查
        return;
    }
    
    for(bn = 0;bn < SEND_STORE_NUM ;bn++)
    {
         if(auSendStore[bn].isHaveData == BUFF_EMPTY)
        {
            continue;
        }
        //缓存的sn号和现在绝对值差超过10* SEND_STORE_NUM,认为一定丢包,清楚缓存
        tmp_dt = abs(auSendStore[bn].now_packet_sn - now_sn);        
        if(tmp_dt > 10* SEND_STORE_NUM)
        {
            printf("[clear_store_buff]!!!! MAC lost pocket Channeld %d !!!! SN :%d, Len: %d, now_sn %d\n",
                     Channeld, auSendStore[bn].now_packet_sn, auSendStore[bn].send_colBuffer_len,now_sn);
            
            auSendStore[bn].isHaveData = BUFF_EMPTY;
            auSendStore[bn].now_packet_sn = 0;
            auSendStore[bn].send_colBuffer_len = 0;
            memset(auSendStore[bn].socket_send_buffer, 0, UDP_BUF_SIZE*sizeof(uint8_t));
            memset(auSendStore[bn].SubIndexRecvFlag , 0, UDP_BUFF_NUM);
            // TODO
            STATE_TRAN_CNT(TRAN_ETH_ERR_CNT);
        }
    }
    return;
}

int trans_send_deal(MAC_CHANNEL_STRU* pChannel)
{  
    if(pChannel->TransType  == NOT_USE)
    {
        //无用的recv 信道不处理
        return 0;
    }

    int result = 0;
    int send_port = 0;
    int send_buff_len = 0;
    MAC_STRU* pSendData = NULL;
    int sub_len = 0;
    uint16_t now_sn = 0;
    SOCKET_SEND_BUFF_STRU* deal_send_buff = NULL;

    int64_t rtc_time = 0;
    uint8_t RtcSendBuf[UDP_BUF_SIZE];
    int RtcLen = sizeof(uint64_t);

     unsigned int rentest= 0;

    //printf("ttt %d %d \n",pChannel->phSendIndex, pChannel->phRxDealIndex);
    while(1)
    {
        if(pChannel->phSendIndex == pChannel->phRxDealIndex)
        {
            return 0;
        }
        pSendData = pChannel->g_rx_buffer_queue[pChannel->phSendIndex];
        now_sn = pSendData->MacHeader.usPacketSN;

        //black list check
        if(blacklist_check(now_sn, pChannel) == 1)
        {
            pChannel->phSendIndex++;
            if(pChannel->phSendIndex >= MAX_BUFF_NUM) {
                pChannel->phSendIndex=0;
            }
            
            continue;
        }
        
        deal_send_buff = get_send_buff_stru(now_sn, pChannel->auSendStore, pChannel->usChannelPriority);
        if(deal_send_buff == NULL)
        {
            printf("Error!! get_send_buff_stru is NULL!!Channeld %d\n", pChannel->usChannelPriority);
        }
        send_buff_len = pSendData->MacHeader.usMacTotalLenBytes;
        sub_len = pSendData->MacHeader.usThisLen;
        *(deal_send_buff->pSendStore[pSendData->MacHeader.usSubIndex]) = pSendData->MacHeader.ucFirstData;
        memcpy(deal_send_buff->pSendStore[pSendData->MacHeader.usSubIndex]+1, pSendData->aucMacOffload, sub_len - 1);

        // 丢弃掉重传的包
        if (deal_send_buff->SubIndexRecvFlag[pSendData->MacHeader.usSubIndex] == 0)
        {
            deal_send_buff->send_colBuffer_len = deal_send_buff->send_colBuffer_len + sub_len;
            deal_send_buff->SubIndexRecvFlag[pSendData->MacHeader.usSubIndex] = 1;
        }

        pChannel->phSendIndex++;
        if(pChannel->phSendIndex >= MAX_BUFF_NUM) {
            pChannel->phSendIndex=0;
        }

        if(deal_send_buff->send_colBuffer_len >= send_buff_len)
        {
            pChannel->Socket_send_buffer_finsh = 1;
            deal_send_buff->send_colBuffer_len = 0;
            pChannel->Socket_send_buffer = deal_send_buff->socket_send_buffer;
            deal_send_buff->isHaveData= BUFF_EMPTY;
            memset(deal_send_buff->SubIndexRecvFlag , 0, UDP_BUFF_NUM);
            break;
        }
        
    }
    
    if(pChannel->Socket_send_buffer_finsh == 1)
    {
        if(gstamp_swtich == 1)
        {
            UDP_send_stamp(pChannel->Socket_send_buffer);
        }

        if((pChannel->TransType == TCP_CLIENT) || (pChannel->TransType == TCP_SERVER))
        {
            result = tcp_send(pChannel->SendSockfd, pChannel->Socket_send_buffer, send_buff_len);

            if (result < 0)
            {
                printf("tcp_send error%s  reconnect\n",strerror(errno));
                return -1;
            }

             if(result == 0)
            {
                //try MAX_TCP_SEND_TRY_TIMES times
                int ti = 0;
                for(ti = 0; ti < MAX_TCP_SEND_TRY_TIMES;ti++)
                {
                    result = tcp_send(pChannel->SendSockfd, pChannel->Socket_send_buffer, send_buff_len);
                    if (result < 0)
                    {
                        printf("tcp_send error%s  reconnect\n",strerror(errno));
                        return -1;
                    }
                    else if(result > 0)
                    {
                        break;
                    }
                }
                
                if(ti >= MAX_TCP_SEND_TRY_TIMES) {
                    printf("tcp_send lost pocket !!\n");
                }
            }
        }
        else
        {
            if(pChannel->TransType == TIME_SYNC_MESG)
            {
                //同步命令不在这里发送，设置FPGA发送时间，由独立线程发送
                rtc_time = *((int64_t *)(pChannel->Socket_send_buffer));
                rtc_set_fpga_event_time(&openair0,(rtc_time + SYNC_DETAY_TIME));
                //printf("renjian rtc_time %lld set fpga event %lld, %lld\n", rtc_time_get(&openair0), rtc_time, (rtc_time + SYNC_DETAY_TIME));
                send_buff_len = send_buff_len - RtcLen;
                
                memcpy(RtcSendBuf, pChannel->Socket_send_buffer+RtcLen , send_buff_len);
                memcpy(pChannel->Socket_send_buffer, RtcSendBuf , send_buff_len);
                pChannel->Socket_send_len = send_buff_len;
                
            } else if(pChannel->TransType == PF_NORMAL) {
                printf("send pf data\n");
                result = pf_send_mesg(pChannel->SendSockfd, pChannel->Socket_send_buffer, 
                         send_buff_len, pChannel->dstNetcard);
                 if (result < 0) {
                    perror("PF_send_mesg error \n");
                 }
            }
            /*
            else if (pChannel->TransType == PF_NORMAL_L3) {
                result = pf_send_mesg_L3(pChannel->SendSockfd, pChannel->Socket_send_buffer, send_buff_len);
                if (result < 0) {
                    perror("pf_send_mesg_L3 error !\n");
                }
            } 
            */
            else {
                result = udp_send_mesg(pChannel->SendSockfd, 
                        pChannel->auPeerIP, pChannel->usPeerPort, pChannel->Socket_send_buffer, send_buff_len);
                if(result < 0)
                {
                    perror("UDP Send message error0 ! \n");
                } else {
                    //printf("udp seng msg!!!peer port %s:%d\n", pChannel->auPeerIP, pChannel->usPeerPort);
                }
            }

        }

        STATE_TRAN_CNT(TRAN_ETH_SND_CNT);
        //memset(pChannel->Socket_send_buffer, 0, UDP_BUF_SIZE);
        pChannel->Socket_send_buffer_finsh = 0;

        pChannel->blacklist[pChannel->blacklist_index] = now_sn;
        pChannel->blacklist_index++;
        if(pChannel->blacklist_index >= BLACKLIST_NUM) pChannel->blacklist_index=0;
        
    }

    pChannel->buffclearIdx++;
    if((pChannel->buffclearIdx % pChannel->clear_buff_times) == 0)
    {
        clear_store_buff(now_sn, pChannel->auSendStore, pChannel->usChannelPriority);
    }

    return 0;
}

//only udp recv
static void* UDP_nom_recv_func( void* param)
{
    printf("[UDP_nom_recv_func] (TID %ld) started on CPU %d\n", gettid(), sched_getcpu());
    while(1)
    {
        //usleep(0);
        for(int prion = 0; prion< MAX_SOCKET_CHANNEL_NUM; prion++)
        {
            if(gChannel[prion].TransType  == NOT_USE)
            {
                //无用的recv 信道不处理
                continue;
            }

            if(gChannel[prion].TransType  != UDP_NORMAL)
            {
                continue;
            }
            
            if(trans_recv_deal(&gChannel[prion]) != 0)
            {
                printf("UDP CHannel %d recv_deal error !!!\n", prion);
            }
        }

    }
    
    return;
}
//only udp send
static void* UDP_nom_send_func( void* param)
{
    printf("[UDP_nom_send_func] (TID %ld) started on CPU %d\n", gettid(), sched_getcpu());
    while(1)
    {
        usleep(0);

        for(int prion = 0; prion< MAX_SOCKET_CHANNEL_NUM; prion++)
        {
            if(gChannel[prion].TransType  == NOT_USE)
            {
                //无用的recv 信道不处理
                continue;
            }

            if(gChannel[prion].TransType  != UDP_NORMAL)
            {
                continue;
            }

            if(trans_send_deal(&gChannel[prion]) != 0)
            {
                printf("UDP CHannel %d send_deal error !!!\n", prion);
            }
            
        } // end for prion loop

    } //end for while
    return;
    
}//end for function

static void* UDP_time_sync_send_func( void* param)
{
    printf("[UDP_time_sync_send_func] (TID %ld) started on CPU %d\n", gettid(), sched_getcpu());

    int result = 0;
    sleep(4);
    while(1)
    {
    
        //rtc_get_fpga_event(&openair0);
        
        for(int pk = 0; pk< time_sync_num; pk++)
        {
                if(gChannel[time_sync_channel[pk]].Socket_send_len == 0)
                {
                    continue;
                }

                result = udp_send_mesg(gChannel[time_sync_channel[pk]].SendSockfd, gChannel[time_sync_channel[pk]].auPeerIP, gChannel[time_sync_channel[pk]].usPeerPort,
                                gChannel[time_sync_channel[pk]].Socket_send_buffer, gChannel[time_sync_channel[pk]].Socket_send_len);
                if(result < 0)
                {
                    perror("UDP Send message error1 ! \n");
                }

                gChannel[time_sync_channel[pk]].Socket_send_len = 0;
        }

    } //end for while
    return;
    
}//end for function

static void* UDP_time_sync_recv_func( void* param)
{
    printf("[UDP_time_sync_recv_func] (TID %ld) started on CPU %d\n", gettid(), sched_getcpu());
    int pk = 0;
    
    sleep(4);
    
    while(1)
    {
        for(pk = 0; pk< time_sync_num; pk++)
        {
            if(trans_recv_deal(&gChannel[time_sync_channel[pk]]) != 0)
            {
                printf("sync CHannel %d send_deal error !!!\n", time_sync_channel[pk]);
            }
        }
    } //end for while
    
    return;
}//end for function


//only udp recv
static void* UDP_mb_recv_func( void* param)
{
    
    printf("[UDP_mb_recv_func] (TID %ld) started on CPU %d\n", gettid(), sched_getcpu());

    while(1)
    {
        //usleep(0);
        
        for(int prion = 0; prion< MAX_SOCKET_CHANNEL_NUM; prion++)
        {
            if(gChannel[prion].TransType  == NOT_USE)
            {
                //无用的recv 信道不处理
                continue;
            }

            if((gChannel[prion].TransType  == UDP_BROADCAST)||(gChannel[prion].TransType  == UDP_MULTICAST))
            {
                if(trans_recv_deal(&gChannel[prion]) != 0)
                {
                    printf("UDP CHannel %d trans_recv_deal error !!!\n", prion);
                }
            }
                        
        }

    }
    
    return;
}
//only udp send
static void* UDP_mb_send_func( void* param)
{
    printf("[UDP_mb_send_func] (TID %ld) started on CPU %d\n", gettid(), sched_getcpu());
    while(1)
    {
        usleep(0);

        for(int prion = 0; prion< MAX_SOCKET_CHANNEL_NUM; prion++)
        {
            if(gChannel[prion].TransType  == NOT_USE)
            {
                //无用的recv 信道不处理
                continue;
            }

            if((gChannel[prion].TransType  == UDP_BROADCAST)||(gChannel[prion].TransType  == UDP_MULTICAST)||(gChannel[prion].TransType  == TIME_SYNC_MESG))
            {
                if(trans_send_deal(&gChannel[prion]) != 0)
                {
                    printf("UDP CHannel %d send_deal error !!!\n", prion);
                }
            }
            
        } // end for prion loop

    } //end for while
    return;
    
}//end for function


static void* PF_nom_recv_func( void* param)  {
    printf("[PF_nom_recv_func] (TID %ld) started on CPU %d\n", gettid(), sched_getcpu());
    while (1) {
        for (int prion = 0; prion< MAX_SOCKET_CHANNEL_NUM; prion++) {
            if (gChannel[prion].TransType  == NOT_USE) {
                continue;
            }
            if ((gChannel[prion].TransType != PF_NORMAL) 
                    && (gChannel[prion].TransType != PF_NORMAL_L3)) {
                continue;
            }
            if (trans_recv_deal(&gChannel[prion]) != 0) {
                printf("PF CHannel %d recv_deal error !!!\n", prion);
            }
        }
    }
    return;
}

static void* PF_nom_send_func( void* param) {
    printf("[PF_nom_send_func] (TID %ld) started on CPU %d\n", gettid(), sched_getcpu());
    while (1) {
        for (int prion = 0; prion< MAX_SOCKET_CHANNEL_NUM; prion++) {
            if (gChannel[prion].TransType  == NOT_USE) {
                continue;
            }
            if ((gChannel[prion].TransType != PF_NORMAL) 
                    && (gChannel[prion].TransType != PF_NORMAL_L3)) {
                continue;
            }
            if (trans_send_deal(&gChannel[prion]) != 0) {
                printf("PF CHannel %d recv_deal error !!!\n", prion);
            }
        }
    }
    return;

}

void *tcp_recv_deal(void *RecvParam)
{
    Recv_Input_t *proc = (Recv_Input_t*)RecvParam;
    int fd = proc->socketfd;
    int prion = proc->prion;

    printf("[tcp_recv_deal]  fd:%d prion:%d\n",fd, prion);

    int iRet = -1;

    uint8_t RecvBuf[UDP_BUF_SIZE];

    gChannel[prion].RecvSockfd = fd;
    gChannel[prion].SendSockfd = fd;
    
    while(1)
    {
        //sleep(0);

        if(local_channel == prion)
        {
            if(local_socket_cmd == STOP_CMD)
            {
                printf("Peer restart the channel %d\n",prion);
             close(fd);
                local_channel = INVALD_CHANNEL_NO;
                local_socket_cmd = NONE_CMD;
                rt_sleep_ns(8*1000*1000);
                return NULL;
            }
            
        }
        
        iRet = trans_recv_deal(&gChannel[prion]);
        if (iRet < 0)
        {
            printf("tcp_recv down %d !\n", prion);
            close(fd);
            peer_socket_cmd = STOP_CMD;
            peer_channel = prion;
            rt_sleep_ns(8*1000*1000);
            return NULL;
        }

        iRet = trans_send_deal(&gChannel[prion]);
        if (iRet < 0)
        {
            printf("tcp_send down %d !\n", prion);
            close(fd);
            peer_socket_cmd = STOP_CMD;
            peer_channel = prion;
            rt_sleep_ns(8*1000*1000);
            return NULL;
        }
    }

    return NULL;
}

void* tcp_server_Thread_func( void* param)
{
    int tcp_pi = *((int *)param);
    int prion = tcp_server_channel[tcp_pi]; 

    printf("[tcp_server_Thread_func] (TID %ld) started on CPU %d TCP ID is %d ,prion is %d \n", gettid(), sched_getcpu(),tcp_pi, prion);
    
    struct sockaddr_in SerAddr,CliAddr;
    pthread_attr_t attr;
    int serfd = -1,clfd = -1,iSize = -1,count = 0;
    int bufSize = TCP_BUFFER_TY;
    pthread_t tid[256];
    Recv_Input_t RecvParam;

    pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
    RecvParam.prion = prion;

    iSize = sizeof(SerAddr);
    serfd = socket(AF_INET,SOCK_STREAM,0);
    if (0 > serfd)
    {
        printf("init socket failed:%d\n",serfd);
        return NULL;
    }

    memset(&(SerAddr),0x00,sizeof(struct sockaddr_in));
    memset(&(CliAddr),0x00,sizeof(struct sockaddr_in));
    SerAddr.sin_family = AF_INET;
    SerAddr.sin_port = htons(gChannel[prion].usLocalPort);
    SerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if ( 0  > bind(serfd,(struct sockaddr *)&SerAddr,iSize))
    {
        printf("server bind  to port %d failed \n",gChannel[prion].usLocalPort);
        return NULL;
    }
    /*
    if ( 0 > setsockopt(serfd,SOL_SOCKET,SO_RCVBUF,&bufSize,sizeof(bufSize)))
    {
        printf("setsockopt error:%s",strerror(errno));
        return NULL;
    }
    */
    if (0 > listen(serfd,MAX_LISTEN))
    {
        printf("listen failed\n");
        return NULL;
    }

    
    while(1)
    {
        clfd = accept(serfd,(struct sockaddr *)&CliAddr,(socklen_t *)&iSize);
        if (0 > clfd)
        {
            printf("accept failed %s",strerror(errno));
            sleep(1);
            continue;
        }

        peer_channel = prion;
        peer_socket_cmd = START_CMD;
        rt_sleep_ns(20*1000*1000);

        if (count < 256)
        { 
            RecvParam.socketfd = clfd;
            pthread_create(&tid[count++],&attr,tcp_recv_deal,(void *)(&RecvParam));
        }
        else
        {
            printf("\n\ntoo many threads \n\n");
            return NULL;
        }   
    }

    pthread_attr_destroy(&attr);
    return NULL;
}

int connect_server(char *ip, int port)
{
    struct sockaddr_in SerAddr;
    int tcpfd = -1,iCount = 0;
    int sndBufSize = TCP_BUFFER_TY;
    int iPort = port;
    
    tcpfd = socket(AF_INET,SOCK_STREAM,0);
    
    if(tcpfd < 0)
    {
        printf("socket error:%s\n",strerror(errno));
        return -1;
    }
    /*
    if ( 0 > setsockopt(tcpfd, SOL_SOCKET, SO_SNDBUF, (char *)&sndBufSize, sizeof(sndBufSize)))
    {
        printf("make_tcp(): setsockopt failed for server address\n");
        return -1;
    } 
    */

    memset(&(SerAddr),0x00,sizeof(SerAddr));
    SerAddr.sin_family = AF_INET;
    SerAddr.sin_port = htons(iPort);
    SerAddr.sin_addr.s_addr = inet_addr(ip);

    while(1)
    {
       if (0 > connect(tcpfd,(struct sockaddr *)&SerAddr,sizeof(struct sockaddr)))
      {
        //printf("the %d times connect to machine: %s,port:%d failed \n",iCount++,ip,iPort);
      }
      else
      {
        printf("[TCP connect success!] to machine: %s,port:%d \n",ip,iPort);
        return tcpfd;
      }
    }
    /*    
    for (iCount = 0; iCount < 3; iCount++)
    {
    if (0 > connect(tcpfd,(struct sockaddr *)&SerAddr,sizeof(struct sockaddr)))
    {
    printf("the %d times connect to machine: %s,port:%d failed \n",iCount + 1,SERV_IP,iPort);
    }
    else
    {
    return tcpfd;
    } 
    } 
    */
    return -1; 

} 

void* tcp_client_Thread_func( void* param)
{
    int tcp_pi = *((int *)param);
    int prion = tcp_client_channel[tcp_pi]; 

    printf("[tcp_client_Thread_func] (TID %ld) started on CPU %d,TCP ID is %d,Prion %d\n", gettid(), sched_getcpu(),tcp_pi, prion);
    
    int tcpfd = 0;
    int iRet = -1;

    tcpfd = connect_server(gChannel[prion].auPeerIP, gChannel[prion].usPeerPort);
    if ( 0 > tcpfd )
    {
        printf("connect to server failed!\n");
        return NULL; 
    }
    
    RTIME time_now = 0;

    gChannel[prion].RecvSockfd = tcpfd;
    gChannel[prion].SendSockfd = tcpfd;
    while(1)
    {
        usleep(0);

        if(local_channel == prion)
        {
            if(local_socket_cmd == STOP_CMD)
            {
                printf("Peer stop the channel %d\n",prion);
            close(tcpfd);
                tcpfd = NULL;
                rt_sleep_ns(10*1000*1000);
            }

            if(local_socket_cmd == START_CMD)
            {  
                printf("Peer start the channel %d\n",prion);
                if(tcpfd != NULL)
                {
                    close(tcpfd);
                    tcpfd = NULL;
                }
                tcpfd = connect_server(gChannel[prion].auPeerIP, gChannel[prion].usPeerPort);
                if ( 0 > tcpfd )
                {
                    printf("connect to server failed!\n");
                    return NULL; 
                }
            }
            
            gChannel[prion].RecvSockfd = tcpfd;
            gChannel[prion].SendSockfd = tcpfd;

            local_channel = INVALD_CHANNEL_NO;
            local_socket_cmd = NONE_CMD;
        }

        if(gChannel[prion].SendSockfd == NULL)
        {
            continue;
        }

        iRet = trans_send_deal(&gChannel[prion]);
        if (iRet < 0)
        {
            printf("select error%s  reconnect\n",strerror(errno));
        close(tcpfd);
            tcpfd = NULL;
            peer_socket_cmd = STOP_CMD;
            peer_channel = prion;
            
        rt_sleep_ns(40*1000*1000);
        tcpfd = connect_server(gChannel[prion].auPeerIP, gChannel[prion].usPeerPort);
            if ( 0 > tcpfd )
            {
                printf("connect to server failed!\n");
                return NULL; 
            }
            peer_socket_cmd = START_CMD;
            peer_channel = prion;
            
            gChannel[prion].RecvSockfd = tcpfd;
            gChannel[prion].SendSockfd = tcpfd;
            rt_sleep_ns(20*1000*1000);
            continue;
        }

        iRet = trans_recv_deal(&gChannel[prion]);
        if (iRet < 0)
        {
            printf("select error%s  reconnect\n",strerror(errno));
        close(tcpfd);
            tcpfd = NULL;
            peer_socket_cmd = STOP_CMD;
            peer_channel = prion;
            
        rt_sleep_ns(40*1000*1000);
        tcpfd = connect_server(gChannel[prion].auPeerIP, gChannel[prion].usPeerPort);
            if ( 0 > tcpfd )
            {
                printf("connect to server failed!\n");
                return NULL; 
            }
            peer_socket_cmd = START_CMD;
            peer_channel = prion;
            
            gChannel[prion].RecvSockfd = tcpfd;
            gChannel[prion].SendSockfd = tcpfd;
            rt_sleep_ns(20*1000*1000);
            
            continue;
        }
    }

    close(tcpfd);
    return NULL;   
}//end for function

int init_data_tran_proc(int stamp_swtich)
{
    Channel_init();

    char name[16]; 
    //RAW SOCKET INIT
    cpu_set_t mask_raw_data_threads;
    pthread_attr_t attr_raw_data_threads;
    struct sched_param sched_param_raw_data_threads;


    CPU_ZERO(&mask_raw_data_threads);
    pthread_attr_init (&attr_raw_data_threads);
    pthread_attr_setstacksize(&attr_raw_data_threads, 64 * PTHREAD_STACK_MIN);

    sched_param_raw_data_threads.sched_priority = sched_get_priority_max(SCHED_FIFO) - 3;
    pthread_attr_setschedparam(&attr_raw_data_threads, &sched_param_raw_data_threads);
    pthread_attr_setschedpolicy(&attr_raw_data_threads, SCHED_FIFO);

    pthread_create(&pf_nom_recv_data_pthread, &attr_raw_data_threads, PF_nom_recv_func, NULL);
    snprintf(name, sizeof(name), "PF_nom_recv_data_pthread");
    pthread_setname_np(pf_nom_recv_data_pthread, name);
    CPU_SET(PF_RECV_CPUID, &mask_raw_data_threads);
    if (pthread_setaffinity_np(pf_nom_recv_data_pthread, sizeof(mask_raw_data_threads), &mask_raw_data_threads)<0) {
        printf("set pf_nom_recv_data_pthread affinity failed\n");
    }

    CPU_ZERO(&mask_raw_data_threads);
    pthread_create( &pf_nom_send_data_pthread, &attr_raw_data_threads, PF_nom_send_func, NULL);
    snprintf(name, sizeof(name), "pf_nom_send_data_pthread");
    pthread_setname_np(pf_nom_send_data_pthread, name);
    CPU_SET(PF_SEND_CPUID, &mask_raw_data_threads); 
    if (pthread_setaffinity_np(pf_nom_send_data_pthread, sizeof(mask_raw_data_threads), &mask_raw_data_threads)<0) {
        printf("set pf_nom_send_data_pthread affinity failed\n");
    } 

    //UDP INIT
    cpu_set_t mask_recv_data_threads;
    pthread_attr_t attr_recv_data_threads;
    struct sched_param   sched_param_recv_data_threads;


    CPU_ZERO(&mask_recv_data_threads);
    pthread_attr_init (&attr_recv_data_threads);
    pthread_attr_setstacksize(&attr_recv_data_threads,64*PTHREAD_STACK_MIN);

    sched_param_recv_data_threads.sched_priority = sched_get_priority_max(SCHED_FIFO) - 3;
    pthread_attr_setschedparam  (&attr_recv_data_threads, &sched_param_recv_data_threads);
    pthread_attr_setschedpolicy (&attr_recv_data_threads, SCHED_FIFO);

    pthread_create( &udp_nom_recv_data_pthread, &attr_recv_data_threads, UDP_nom_recv_func, NULL);
    snprintf( name, sizeof(name), "udp_nom_recv_data");
    pthread_setname_np(udp_nom_recv_data_pthread, name );
    CPU_SET(UDP_RECV_NOM_CPUID, &mask_recv_data_threads);
    if (pthread_setaffinity_np(udp_nom_recv_data_pthread, sizeof(mask_recv_data_threads), &mask_recv_data_threads)<0)
    {
      printf("set udp_nom_ affinity failed\n");
    }

    pthread_create( &udp_mb_recv_data_pthread, &attr_recv_data_threads, UDP_mb_recv_func, NULL);
    snprintf( name, sizeof(name), "udp_mb_recv_data_pthread");
    pthread_setname_np(udp_mb_recv_data_pthread, name );
    CPU_SET(UDP_RECV_MB_CPUID, &mask_recv_data_threads);
    if (pthread_setaffinity_np(udp_mb_recv_data_pthread, sizeof(mask_recv_data_threads), &mask_recv_data_threads)<0)
    {
      printf("set udp_mb_recv_data_pthread affinity failed\n");
    }
    

    cpu_set_t mask_send_data_threads;
    pthread_attr_t attr_send_data_threads;
    struct sched_param   sched_param_send_data_threads;

    CPU_ZERO(&mask_send_data_threads);
    pthread_attr_init (&attr_send_data_threads);
    pthread_attr_setstacksize(&attr_send_data_threads,64*PTHREAD_STACK_MIN);

    sched_param_send_data_threads.sched_priority = sched_get_priority_max(SCHED_FIFO) - 3;
    pthread_attr_setschedparam  (&attr_send_data_threads, &sched_param_send_data_threads);
    pthread_attr_setschedpolicy (&attr_send_data_threads, SCHED_FIFO);

    char name1[16];
    pthread_create( &udp_nom_send_data_pthread, &attr_send_data_threads, UDP_nom_send_func, NULL);
    snprintf( name1, sizeof(name1), "udp_nom_send_data_pthread");
    pthread_setname_np(udp_nom_send_data_pthread, name1 );
    CPU_SET(UDP_SEND_NOM_CPUID, &mask_send_data_threads);
    if (pthread_setaffinity_np(udp_nom_send_data_pthread, sizeof(mask_send_data_threads), &mask_send_data_threads)<0)
    {
      printf("set udp_nom_send_data_pthread affinity failed\n");
    }

    pthread_create( &udp_mb_send_data_pthread, &attr_send_data_threads, UDP_mb_send_func, NULL);
    snprintf( name1, sizeof(name1), "udp_mb_send_data_pthread");
    pthread_setname_np(udp_mb_send_data_pthread, name1 );
    CPU_SET(UDP_SEND_MB_CPUID, &mask_send_data_threads);
    if (pthread_setaffinity_np(udp_mb_send_data_pthread, sizeof(mask_send_data_threads), &mask_send_data_threads)<0)
    {
      printf("set udp_mb_send_data_pthread affinity failed\n");
    }

    //TCP INIT

    if(tcp_server_num >= MAX_TCP_SERVER)
    {
        printf("%d is Over MAX_TCP_SERVER!!! \n", tcp_server_num);
        return -1;
    }

    cpu_set_t mask_tcp_server_data_threads;
    pthread_attr_t attr_tcp_server_data_threads;
    struct sched_param   sched_param_tcp_server_data_threads;

    CPU_ZERO(&mask_tcp_server_data_threads);
    pthread_attr_init (&attr_tcp_server_data_threads);
    pthread_attr_setstacksize(&attr_tcp_server_data_threads,64*PTHREAD_STACK_MIN);

    sched_param_tcp_server_data_threads.sched_priority = sched_get_priority_max(SCHED_FIFO) - 4;
    pthread_attr_setschedparam  (&attr_tcp_server_data_threads, &sched_param_tcp_server_data_threads);
    pthread_attr_setschedpolicy (&attr_tcp_server_data_threads, SCHED_FIFO);

    char name2[16];
    for(int pi = 0;pi < tcp_server_num; pi++)
    {
        pthread_create( &tcp_server_pthread[pi], &attr_tcp_server_data_threads, tcp_server_Thread_func, (void *)&pi);
        sleep(1);
        snprintf( name2, sizeof(name2), "tcp_server_pthread");
        pthread_setname_np(tcp_server_pthread[pi], name2 );
        CPU_SET(TCP_SERVER_CPUID, &mask_tcp_server_data_threads);
        if (pthread_setaffinity_np(tcp_server_pthread[pi], sizeof(mask_tcp_server_data_threads), &mask_tcp_server_data_threads)<0)
        {
          printf("set tcp_server_pthread affinity failed\n");
        }
    }

    if(tcp_client_num >= MAX_TCP_CLIENT)
    {
        printf("%d is Over tcp_client_num!!! \n", tcp_client_num);
        return -1;
    }

    cpu_set_t mask_tcp_client_data_threads;
    pthread_attr_t attr_tcp_client_data_threads;
    struct sched_param   sched_param_tcp_client_data_threads;

    CPU_ZERO(&mask_tcp_client_data_threads);
    pthread_attr_init (&attr_tcp_client_data_threads);
    pthread_attr_setstacksize(&attr_tcp_client_data_threads,64*PTHREAD_STACK_MIN);

    sched_param_tcp_client_data_threads.sched_priority = sched_get_priority_max(SCHED_FIFO) - 4;
    pthread_attr_setschedparam  (&attr_tcp_client_data_threads, &sched_param_tcp_client_data_threads);
    pthread_attr_setschedpolicy (&attr_tcp_client_data_threads, SCHED_FIFO);

    char name3[16];
    for(int pti = 0;pti < tcp_client_num; pti++)
    {
        
        pthread_create( &tcp_client_pthread[pti], &attr_tcp_client_data_threads, tcp_client_Thread_func, (void *)&pti);
        sleep(1);
        snprintf( name3, sizeof(name3), "tcp_client_pthread");
        pthread_setname_np(tcp_client_pthread[pti], name3 );
        CPU_SET(TCP_CLIENT_CPUID, &mask_tcp_client_data_threads);
        if (pthread_setaffinity_np(tcp_client_pthread[pti], sizeof(mask_tcp_client_data_threads), &mask_tcp_client_data_threads)<0)
        {
          printf("set tcp_client_pthread affinity failed\n");
        }
    }
    /*
    udp_sync_pthread = (thread_together*)malloc(sizeof(thread_together));
    threadstruct_init(udp_sync_pthread);
    udp_sync_pthread->schedparam.sched_priority = sched_get_priority_max(SCHED_FIFO) - 2;
    udp_sync_pthread->threadflag = UDP_SYNC;
    udp_sync_pthread->cpuid = SYNC_CPU;
    snprintf(udp_sync_pthread->name, sizeof(udp_sync_pthread->name), "udp_sync_pthread" );
    thread_gather(udp_sync_pthread,UDP_time_sync_send_func, NULL);   

    udp_sync_recv_pthread = (thread_together*)malloc(sizeof(thread_together));
    threadstruct_init(udp_sync_recv_pthread);
    udp_sync_recv_pthread->schedparam.sched_priority = sched_get_priority_max(SCHED_FIFO) - 2;
    udp_sync_recv_pthread->threadflag = UDP_SYNC;
    udp_sync_recv_pthread->cpuid = SYNC_CPU;
    snprintf(udp_sync_recv_pthread->name, sizeof(udp_sync_recv_pthread->name), "sync_recv_pthread" );
    thread_gather(udp_sync_recv_pthread,UDP_time_sync_recv_func, NULL);
    
    */
    gstamp_swtich = stamp_swtich;
    return 0;

}

int kill_data_tran_proc(void)
{

    int result;
    int *status;

    pthread_join( udp_nom_recv_data_pthread, (void**)&status );
    pthread_join( udp_mb_recv_data_pthread, (void**)&status );

    pthread_join( udp_nom_send_data_pthread, (void**)&status );
    pthread_join( udp_mb_send_data_pthread, (void**)&status );
      
    for(int pi = 0;pi < tcp_server_num; pi++)
    {
      result = pthread_join(tcp_server_pthread[pi], (void**)&status );
      if (result != 0) {
          printf( "Error joining tcp_server_pthread.\n" );
      } else {
        if (status) {
          printf( "status %d\n", *status );
      } else {
          printf( "The thread was killed. No status available.\n" );
          }
      }
    }

    for(int pi = 0;pi < tcp_client_num; pi++)
    {
      result = pthread_join(tcp_client_pthread[pi], (void**)&status );
      if (result != 0) {
          printf( "Error joining tcp_client_pthread.\n" );
      } else {
        if (status) {
          printf( "status %d\n", *status );
      } else {
          printf( "The thread was killed. No status available.\n" );
          }
      }
    }

    thread_end(udp_sync_pthread);
    free(udp_sync_pthread);
    
    thread_end(udp_sync_recv_pthread);
    free(udp_sync_recv_pthread);      
      Channel_free();

      return 0;
}


void PrintTime()
{
    printf("[get_recv_buffer] Get In Que Time[%lld]\n",rt_get_time_ns());
    return ;
}
