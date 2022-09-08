/******************************************************************************

                 版权所有 (C), 2001-2015, 华为技术有限公司

******************************************************************************
 文 件 名   : Bch.c
 版 本 号   : 初稿
 作    者   : lizheng
 生成日期   : 2015年12月14日
 最近修改   :
 功能描述   : L2主文件
 修改历史   : 新建文件

*******************************头文件包含****************************************/

#include <time.h>
#include <stdbool.h>
#include "L2_procedures.h"
#include "phy_procedures.h"
#include "rt_wrapper.h"
#include "common_lib.h"

#include "udp.h"
#include "data_trans.h"
#include <NGRP/time_helper.h>

extern RTIME base_sys_time;
extern int time_count;
extern openair0_device openair0;
extern pthread_mutex_t mac_data_mutex;
extern uint32_t Get_TBSize_Byte();

volatile int  l2_phy_stub_switch = 0;  //L2 数据打桩
volatile int  l2_1588_stub = 0;  //L2 数据打桩
uint32_t testk = 0;
MAC_STRU gstru_StubMacData = {0};
__thread volatile uint8_t* gpMacTx[MAX_SUB_CARRIER_UE_NUM];

extern EIOT_properties_t* pEIOT_properties;
extern Logical_ue_map_t* p_ue_map;
extern uint8_t peer_socket_cmd;
extern uint8_t peer_channel;
extern uint8_t local_socket_cmd;
extern uint8_t local_channel;
extern uint8_t * g_rx_buffer_wtsn_followup;
extern uint8_t * g_rx_buffer_wtsn_delayresp;
extern void PrintTime();

volatile RTIME UE_RxTx_Time = -1;
pthread_mutex_t rxtx_time_mutex;
pthread_mutex_t wait_slot_mutex;
int rxtx_time_flag = 0;
#define TIME_CNT 9
RTIME rxtx_times[TIME_CNT];
int waitSlot[TIME_CNT];
int slot_write_sf = 0;
int slot_read_sf = 0;
int write_sf = 0;
int read_sf = 0;
 
volatile RTIME total_time = -1;

volatile int test_time_count = 0;
//by wz 一个包的时延
volatile RTIME signal_time_max = -1;
volatile RTIME signal_time_min = -1;

int64_t last_index0 = 0;
struct DataStruct {
	char name[20];
	RTIME time_now;
	char msg[60];
	uint64_t index;
};

void L2_init(void)
{
    //dummy mac packet
    gstru_StubMacData.MacHeader.usMacChannelType   = INVALID_TYPE;
    gstru_StubMacData.MacHeader.usMacTotalLenBytes = 0xEE;
    gstru_StubMacData.MacHeader.usPacketSN = 0;
    gstru_StubMacData.MacHeader.usSubIndex = 0;
    //gstru_StubMacData.MacHeader.usThisLen = 137;
    //gstru_StubMacData.MacHeader.usThisLen = 573;/*zwx467374 on 2017-08-14 modify 137 to 573 RB[0,99]*/
    gstru_StubMacData.MacHeader.usThisLen = 137;/*zwx467374 on 2017-08-16 modify 573 to  RB[0,99]*/
    
    return;
}

int init_tx_mac_buffer(void)
{   
    for (int i = 0; i < MAX_SUB_CARRIER_UE_NUM; ++i) {
        gpMacTx[i] = (uint8_t*)malloc(MAC_LEN);
        memset(gpMacTx[i], 0, MAC_LEN);
    }
    
    return 0;
}
int free_tx_mac_buffer(void)
{   for (int i = 0; i < MAX_SUB_CARRIER_UE_NUM; ++i) {
        if(gpMacTx[i] != NULL) {
            free(gpMacTx[i]); 
        }
    }
    return 0;
}
uint8_t* get_tx_mac_pnt(uint8_t index)
{
    return (uint8_t *)gpMacTx[index];
}



int Rx_Mac(int wSysFrame,int wSysSubFrame,int wSysSlot, uint8_t* rx_mac_date , int mac_date_len, int* sib_flag)
{
    MAC_STRU* pMacRx = (MAC_STRU*)rx_mac_date;
    uint8_t eNBUEflag = pEIOT_properties->NBUE_flag;

    if (pEIOT_properties->cycle_time_flag == 1) {
        if(NB == eNBUEflag)
        {
            TIME_STAMP_STRU* pstamp = (TIME_STAMP_STRU*)pMacRx->aucMacOffload;
            if (pstamp->stamptime1 > 0) {                
                total_time = total_time + (common_get_time_ns() - pstamp->stamptime1);
                //by wz test 获取 max min
                
                if(test_time_count==0){
                  signal_time_max =(common_get_time_ns() - pstamp->stamptime1);
                  signal_time_min =(common_get_time_ns() - pstamp->stamptime1);
                }
                signal_time_max = (common_get_time_ns() - pstamp->stamptime1) > signal_time_max ? (common_get_time_ns() - pstamp->stamptime1) : signal_time_max;
                signal_time_min = (common_get_time_ns() - pstamp->stamptime1) < signal_time_min ? (common_get_time_ns() - pstamp->stamptime1) : signal_time_min;
                
                test_time_count++;
                // TODO:test
                //printf("recv slot:%lld, time_dis:%lld,time_stamp:%lld\n",
                //pstamp->send_slot, common_get_time_ns() - pstamp->stamptime1, pstamp->stamptime1);
            }
        } else {
            TIME_STAMP_STRU* pstamp = (TIME_STAMP_STRU*)pMacRx->aucMacOffload;
            /*
            if ((write_sf + 1) % TIME_CNT == read_sf ) {
                printf("TIME QUEUE is FULL\n");
            }
            while (true) {
                if ((write_sf + 1) % TIME_CNT != read_sf) {
                    break;
                }
            }
            */
            pthread_mutex_lock(&rxtx_time_mutex);
            UE_RxTx_Time = pstamp->stamptime1;
            pthread_mutex_unlock(&rxtx_time_mutex);
            /*
            pthread_mutex_lock(&rxtx_time_mutex);
            rxtx_times[write_sf] = pstamp->stamptime1;
            write_sf = (write_sf + 1) % TIME_CNT;
            rxtx_time_flag = 1;
            pthread_mutex_unlock(&rxtx_time_mutex);
            */
            // printf("ue recv time:%lld, slot:[%d.%d.%d]\n", UE_RxTx_Time, wSysFrame, wSysSubFrame, wSysSlot);
        } 
    }
    else if (pEIOT_properties->cycle_time_flag == 0) {
        /*
        TIME_STAMP_STRU* pstamp = (TIME_STAMP_STRU*)pMacRx->aucMacOffload;
        struct timespec lnow;
        clockid_t lclock_id = CLOCK_MONOTONIC;
        clock_gettime(lclock_id, &lnow);
        pstamp->stamptime3 = (lnow.tv_sec*(long long int)1000000000+lnow.tv_nsec);
        */
        //uint8_t* rx_date_buffer = NULL;
        
        //如果是l2打桩
        if(l2_phy_stub_switch == 1)
        {
             if (pthread_mutex_lock(&mac_data_mutex) != 0) {
              LOG(LOG_L2,ERROR, "[L2] error locking mutex for Rx_Mac proc %d\n" );
              return 0;
            }
            time_count++;
            if (pthread_mutex_unlock(&mac_data_mutex) != 0) {
              LOG(LOG_L2,ERROR,"[L2] error unlocking mutex for Rx_Mac proc %d\n");
              return 0;
            }

            return 0;
        }
        
        if(pMacRx->MacHeader.usMacTotalLenBytes > 0)
        {
            if(pMacRx->MacHeader.usMacChannelType < MAX_SOCKET_CHANNEL_NUM)
            {
                //printf("receive app data,channeltype:%d----------------- \n\n\n\n\n\n\n\n",pMacRx->MacHeader.usMacChannelType) ;/
                // TODO:UE can process on this slot
          //*********************************************test start         
           /*    struct DataStruct* Data_decode_rxmac;
		       Data_decode_rxmac = ((struct DataStruct*)(&(rx_mac_date[7])));
			   MAC_STRU* pMacRx_o =(MAC_STRU*) (&(rx_mac_date));//? by wxy
		  
			    if(pMacRx_o->MacHeader.usMacChannelType == 1){ 
                	if(last_index0 != Data_decode_rxmac->index){
                		if(last_index0 + 1!= Data_decode_rxmac->index){
							printf("L2 Rx Mac index:%ld,last_index:%ld,time_now:%ld\n \n",Data_decode_rxmac->index,last_index0,Data_decode_rxmac->time_now);
	        				}	
					}
					last_index0 = Data_decode_rxmac ->index;//by wxy
			    }*/
		  //*********************************************test end
                if (can_rxtx(wSysFrame, wSysSubFrame, wSysSlot)) {
                    set_socket_send_buffer(rx_mac_date);
                }
		
            }

            if(pMacRx->MacHeader.usMacChannelType ==  WTSN_MESG_FOLLOE_TYPE)
            {
                //printf("FXB: jump in [WTSN_MESG_TYPE]____________________\n");
                proc_wtsn_mesg(rx_mac_date);
            }

            if(pMacRx->MacHeader.usMacChannelType ==  WTSN_MESG_DELAY_TYPE)
            {
                printf("recv wtsn mesg\n");
                //proc_wtsn_mesg(rx_mac_date);
            }

            if(pMacRx->MacHeader.usMacChannelType ==  WTSN_SIB_TYPE) 
            {
                *sib_flag = 1;
                wTSN_SIB_recv(rx_mac_date, wSysFrame, wSysSubFrame, wSysSlot); 
            }
        }

        if (pthread_mutex_lock(&mac_data_mutex) != 0) {
            LOG(LOG_L2,ERROR, "[L2] error locking mutex for Rx_Mac proc %d\n" );
            return -1;
        }
        time_count++;
        if (pthread_mutex_unlock(&mac_data_mutex) != 0) {
            LOG(LOG_L2,ERROR,"[L2] error unlocking mutex for Rx_Mac proc %d\n");
            return -1;
        }
    }

    return 0;
}

/* 物理层配置  获得当前 TBsize Bytes
Transmod        TB        G       Scramble      Mod         mapping  

  1T1R        1096bit    2856      1428*2      QPSK      300*5-72=1428 sym


              1096bit    3056      1528*2       QPSK     1428+100=1528 sym
         
2T1R/2T2R

              2192bit    6112      1526*4       16QAM       1528 sym   

*/

uint8_t* get_recv_buffer(int ue_idx)
{
    uint8_t* tx_date_buffer = NULL;
    uint8_t eNBUEflag = pEIOT_properties->NBUE_flag;

    tx_date_buffer = get_socket_recv_buffer(ue_idx);
    if((tx_date_buffer == NULL) && (NB == eNBUEflag))
    {   
        tx_date_buffer = get_wtsn_mesg();

    }
    
    return tx_date_buffer;
}

int Tx_Mac(int wSysFrame,int wSysSubFrame,int wSysSlot, int *targtFrame, int *targtSubFrame, int *targtSubSlot, int* channelMask)
{
    uint8_t* tx_date_buffer = NULL;
    uint32_t Payload_Bytes = 0;
    uint32_t uwRst = 0;
    uint8_t eNBUEflag = pEIOT_properties->NBUE_flag;

    Payload_Bytes = MAC_LEN;

    get_manage_frame(wSysSlot, wSysSubFrame, wSysFrame, targtSubSlot, targtSubFrame, targtFrame);

    //打桩开关
    if(l2_phy_stub_switch == 1)
    {               
        return 0;
    }
    
    if (pEIOT_properties->multi_freq_ue_flag == 0) {
        (*channelMask) |= (1 << (pEIOT_properties->default_channel));
    } else {
        if (pEIOT_properties-> NBUE_flag == NB) {
            for (int i = 0; i < MAX_SUB_CARRIER_UE_NUM; ++i) {
                (*channelMask) |= (1 << i);
            }
        } 
    }

    // TODO:UE can process on this slot
    if (!can_rxtx(*targtFrame, *targtSubFrame, *targtSubSlot)) {
        for (int i = 0; i < MAX_SUB_CARRIER_UE_NUM; ++i) {
            memset(gpMacTx[i], 0, MAC_LEN);
        }
        return 0;
    }
    
    int txues[MAX_SUB_CARRIER_UE_NUM];
    get_rxtx_ueids(*targtFrame, *targtSubFrame, *targtSubSlot, txues, channelMask);
    int print_flag = 0 ;
    for (int i = 0; i < MAX_SUB_CARRIER_UE_NUM; ++i) {
        if ((i != (pEIOT_properties->default_channel)) 
                && (pEIOT_properties->multi_freq_ue_flag == 0)) {
            // not multi freq ue
            continue;
        }
        if (pEIOT_properties->cycle_time_flag == 1) {
            // 环回演示
            tx_date_buffer = (uint8_t*)malloc(MAC_LEN*sizeof(uint8_t));
            if(NB == eNBUEflag)
            {
                MAC_STRU* pMacRx = (MAC_STRU*)tx_date_buffer;
                TIME_STAMP_STRU* pstamp = (TIME_STAMP_STRU*)pMacRx->aucMacOffload;
                //pstamp->stamptime1 = rt_get_time_ns();
                pstamp->stamptime1 = common_get_time_ns();
            }
            else
            {
                MAC_STRU* pMacRx = (MAC_STRU*)tx_date_buffer;
                TIME_STAMP_STRU* pstamp = (TIME_STAMP_STRU*)pMacRx->aucMacOffload;
                pthread_mutex_lock(&rxtx_time_mutex);
                pstamp->stamptime1 = UE_RxTx_Time;
                UE_RxTx_Time = -1;
                pthread_mutex_unlock(&rxtx_time_mutex);
                /*
                if (rxtx_time_flag == 1) {
                    if (write_sf == read_sf) {
                        if ((slot_write_sf  + 1) == slot_read_sf) {
                            printf("system time is wrong\n");
                        }
                        pthread_mutex_lock(&wait_slot_mutex);
                        waitSlot[slot_write_sf] = wSysSlot;
                        slot_write_sf = (slot_write_sf + 1) % TIME_CNT;
                        pthread_mutex_unlock(&wait_slot_mutex);
                        //printf("rxtx times is empty!\n");
                        while (true) {
                            printf("write_sf:%d, read_sf:%d, wait_slot:%d, cur_slot:%d\n", 
                                    write_sf, read_sf, waitSlot[slot_read_sf], wSysSlot);
                            if ((write_sf != read_sf) && (waitSlot[slot_read_sf] == wSysSlot)) {
                                pthread_mutex_lock(&wait_slot_mutex);
                                slot_read_sf = (slot_read_sf + 1) % TIME_CNT;
                                pthread_mutex_unlock(&wait_slot_mutex);
                                break;
                            }
                        }
                    }
                    pthread_mutex_lock(&rxtx_time_mutex);
                    pstamp->stamptime1 = rxtx_times[read_sf];
                    read_sf = (read_sf + 1) % TIME_CNT;
                    pthread_mutex_unlock(&rxtx_time_mutex);
                }
                */
                // TODO:zach
                pstamp->send_slot = *targtSubSlot;
                if ( pstamp->stamptime1 > 0) {
                    //printf("ue send time:%lld, slot:[%d.%d.%d], channel:%d\n", pstamp->stamptime1,
                    //    wSysFrame, wSysSubFrame, wSysSlot, i);
                }
            }

            memcpy(gpMacTx[i], tx_date_buffer, Payload_Bytes);
            free(tx_date_buffer);
        } else {
            // 实际数据收发
            if ((pEIOT_properties->multi_freq_ue_flag == 1) && (txues[i] == -1)) {
                // do not process data, but channel should send SIB message
                if ((i == (pEIOT_properties->sib_channel)) && (NB == eNBUEflag)) {
                //if ((i == 0) && is_sib_msg_slot_nb(*targtFrame, *targtSubFrame, *targtSubSlot)) {
                    *channelMask |= (1 << (pEIOT_properties->sib_channel));
                    uwRst = wTSN_SIB_send(*targtFrame, *targtSubFrame, *targtSubSlot, i);
                    if(uwRst != 0){
                        printf("wTSN_SIB_send error!Error code %d \n", uwRst );
                        return -1;
                    }
                } else {
                    memset(gpMacTx[i], 0, MAC_LEN);
                }
                continue;
            }
            int ue_idx = 0;
            if (pEIOT_properties->multi_freq_ue_flag == 1) {
                ue_idx = txues[i];
            } else {
                // get all channel data
                ue_idx = -1; 
            }
            uint8_t* tx_date_buffer = get_recv_buffer(ue_idx);
            Payload_Bytes = MAC_LEN;
            if(tx_date_buffer != NULL)
            {     


//modified by gf start
            if(i == 1)
	    {print_flag = 1;
	/*	struct	DataStruct* Data_decode;
				MAC_STRU*  pMacTx;//modified by gf				
			//	pMacTx = ((MAC_STRU*)(tx_date_buffer)); 
			//	Data_decode = ((struct DataStruct*)(pMacTx->aucMacOffload));
                   Data_decode = ((struct DataStruct*)(&(tx_date_buffer[7]))) ;
				
			if(last_index0 != Data_decode->index){
			if(last_index0+1 != Data_decode->index){
			printf("index:%d, last_index:%d, time_now:%ld ,buffer:%s\n",Data_decode->index,last_index0,Data_decode->time_now,tx_date_buffer);// by wxy
		        }
                        } 
 			last_index0 = Data_decode->index;
*/
	    }
				//modified by gf end


           //printf("SEND test info:%d.%d.%d ue_idx:%d\n", *targtFrame, *targtSubFrame, *targtSubSlot, i);

                #if 0
                MAC_STRU* pMacRx = (MAC_STRU*)tx_date_buffer;
                TIME_STAMP_STRU* pstamp = (TIME_STAMP_STRU*)pMacRx->aucMacOffload;
                struct timespec lnow;
                clockid_t lclock_id = CLOCK_MONOTONIC;
                clock_gettime(lclock_id, &lnow);
                pstamp->stamptime2 = (lnow.tv_sec*(long long int)1000000000+lnow.tv_nsec);
                #endif
                //printf("Tx_Mac\n");
                memcpy(gpMacTx[i], tx_date_buffer, Payload_Bytes);
                //printf("PDst[%x],PSrc[%x],Len[%d],Data[0x%x/0x%x/0x%x/0x%x]\n",gpMacTx,tx_date_buffer,Payload_Bytes,gpMacTx[0],gpMacTx[1],gpMacTx[2],gpMacTx[3]);
                //gpMacTx = tx_date_buffer;
                STATE_TRAN_CNT(L2_SND_TRAFFIC_CNT);
                //PrintTime();
            } else {
                //if((NB == eNBUEflag) && ((0 == wSysFrame%WTSN_PERIOD) && (0 == wSysSubFrame) && (0 == wSysSlot))) {
                if (is_sib_msg_slot_nb(*targtFrame, *targtSubFrame, *targtSubSlot)) {
                    uwRst = wTSN_SIB_send(*targtFrame, *targtSubFrame, *targtSubSlot, i);
                    if(uwRst !=0 )
                    {
                        printf("wTSN_SIB_send error!Error code %d \n", uwRst );
                        return -1;
                    }
                } else {
                    gstru_StubMacData.MacHeader.usPacketSN++;

                    if(peer_socket_cmd != NONE_CMD) {
                        gstru_StubMacData.MacHeader.usMacTotalLenBytes = peer_socket_cmd;
                    } else {
                        gstru_StubMacData.MacHeader.usMacTotalLenBytes = NONE_CMD;
                        gstru_StubMacData.MacHeader.usMacChannelType = TCP_SYNC_TYPE;
                    }
                    
                    if(peer_channel != INVALD_CHANNEL_NO) {
                        //printf("CMD %d peer_channel %d\n", peer_socket_cmd, peer_channel);
                        gstru_StubMacData.MacHeader.usSubIndex = peer_channel;//借用subInex号传递这个
                    } else {
                        gstru_StubMacData.MacHeader.usSubIndex = INVALD_CHANNEL_NO;
                        gstru_StubMacData.MacHeader.usMacChannelType = TCP_SYNC_TYPE;
                    }
                    memcpy(gpMacTx[i], (uint8_t *)&gstru_StubMacData, Payload_Bytes);
                    STATE_TRAN_CNT(L2_SND_STUB_CNT);
                    peer_channel = INVALD_CHANNEL_NO;
                    peer_socket_cmd = NONE_CMD;
                }
            }
        }

    }
	//*********************************************test start
	/*MAC_STRU* pMacTx_o =(MAC_STRU*) (gpMacTx[1]);
	if(pMacTx_o->MacHeader.usMacChannelType == 1){
 		
 	struct DataStruct* Data_decode_txmac;
		//modified by gf				
	//printf("index:%d ,txue[i]:%d, sib_channel:%d----------------------------------------------------------------------\n ",ue_idx,txues[ue_idx],get_sib_channel());

	Data_decode_txmac = ((struct DataStruct*)(gpMacTx[1]+7)) ; 
	if(last_index0 != Data_decode_txmac->index){
		if(last_index0+1 != Data_decode_txmac->index){
			printf("L2 Tx_Mac:index:%d, last_index:%d \n",Data_decode_txmac->index,last_index0);
		}
	}
	last_index0 = Data_decode_txmac->index;
	}
     if(!((*channelMask) & (1<<1) ))
 	{
	     printf("channel_mask err:%d\n",*channelMask);
        }*/
    //*********************************************test end
    return 0;
}

bool can_rxtx(int wSysFrame, int wSysSubFrame, int wSysSlot) {
    bool can_rxtx = true;
    if (pEIOT_properties->NBUE_flag == UE) {
        if (pEIOT_properties->multi_freq_ue_flag == 0) {
            can_rxtx = false;
            for (int i = 0; i < NUM_UE_SLOT_MAX; ++i) {
                if (pEIOT_properties->ue_slots[i] == wSysSlot) {
                    can_rxtx = true;
                    break; 
                } else if (pEIOT_properties->ue_slots[i] == UE_SLOT_INVALID) {
                    break;
                }
            }
        } else if (pEIOT_properties->multi_freq_ue_flag == 1) {
            can_rxtx = false;
            if (wSysSlot >= MAX_SLOT_UE_NUM) {
                printf("wSysSlot is invalid\n");
            } else {
               for (int i = 0; i < MAX_SUB_CARRIER_UE_NUM; ++i) {
                   if (p_ue_map->subcarrier_ue_map[wSysSlot][i] == pEIOT_properties->ue_id) {
                       can_rxtx = true;
                       break;
                   }
               }
            }
        } else {
            printf("multi_ue_flag is invalid\n");
        }
    }
    return can_rxtx;
}

void get_rxtx_ueids(int wSysFrame, int wSysSubFrame, int wSysSlot, int ueids[MAX_SUB_CARRIER_UE_NUM], int* channelMask) {
    *channelMask = 0;
    if (wSysSlot >= MAX_SLOT_UE_NUM) {
        printf("Error:wSysSlot is invalid\n");
        for (int i = 0; i < MAX_SUB_CARRIER_UE_NUM; ++i) {
            ueids[i] = -1;
            (*channelMask) |= (1 << i);
        }
        return;
    }
    if (pEIOT_properties->NBUE_flag == UE) {
        if (pEIOT_properties->multi_freq_ue_flag == 0) {
            (*channelMask) |= (1 << (pEIOT_properties->default_channel));
            for (int i = 0; i < MAX_SUB_CARRIER_UE_NUM; ++i) {
                ueids[i] = -1;
            }
            return;
        }
        for (int i = 0; i < MAX_SUB_CARRIER_UE_NUM; ++i) {
            if (p_ue_map->subcarrier_ue_map[wSysSlot][i] == pEIOT_properties->ue_id) {
                ueids[i] = pEIOT_properties->ue_id;
                (*channelMask) |= (1 << i);
            } else {
                ueids[i] = -1;
            }
        }
    } else {
        if (pEIOT_properties->multi_freq_ue_flag == 0) {
            (*channelMask) |= (1 << (pEIOT_properties->default_channel));
            for (int i = 0; i < MAX_SUB_CARRIER_UE_NUM; ++i) {
                ueids[i] = -1;
            }
            return;
        }
        for (int i = 0; i < MAX_SUB_CARRIER_UE_NUM; ++i) {
            // system may not use this channel
            ueids[i] = p_ue_map->subcarrier_ue_map[wSysSlot][i];
            if (ueids[i] != -1) {
                (*channelMask) |= (1 << i);
            }
        }
    }
    return;
}


bool is_sib_msg_slot_nb(int wSysFrame, int wSysSubFrame, int wSysSlot) {
    if((pEIOT_properties->NBUE_flag == NB) 
            && (0 == wSysFrame%WTSN_PERIOD) 
            && (0 == wSysSubFrame) && (0 == wSysSlot)) {
        return true;
    }
    return false;
}

bool is_sib_msg_slot_ue(int wSysFrame, int wSysSubFrame, int wSysSlot) {
    if((pEIOT_properties->NBUE_flag == UE) 
            && (0 == wSysFrame%WTSN_PERIOD) 
            && (0 == wSysSubFrame) && (0 == wSysSlot)) {
        return true;
    }
    return false;
}

bool is_sib_dect_frame(int wSysFrame) {
    return ((pEIOT_properties->NBUE_flag == UE) 
            && (0 == wSysFrame % WTSN_PERIOD));
}

bool is_multi_freq_mode() {
    return (pEIOT_properties->multi_freq_ue_flag == 1);
}

int8_t get_sib_channel() {
    return (pEIOT_properties->sib_channel);
}



