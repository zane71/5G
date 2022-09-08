
#include <time.h>
#include "L2_procedures.h"
#include "phy_procedures.h"
#include "rt_wrapper.h"
#include "common_lib.h"
#include "wTSN.h"


#include "threadpackage.h"
#include "global_def.h"

//wTSN  ����
int64_t gT1_flag = 0;
int64_t gT2_flag = 0;
int64_t gT3_flag = 0;
int64_t gT4_flag = 0;
    
uint8_t gSync_flag = 0;
uint8_t gDelayreq_flag = 0;
int32_t gwtsn_T2_corr = 0;  //T2����ֵ
int32_t gwtsn_T4_corr = 0;  //T4����ֵ
uint8_t gwtsn_tx_buffer_flag = 0;

int last_slot;
int last_subframe;
int last_frame;

WTSN_TIMESTAMP_STRU *gwtsn_Timestamp;
WTSN_TIMEFRE_OFFSET_STRU *gwtsn_TimeFreoffset;
uint64_t clock_sync_cnt = 0;  //ִ��ʱ��ͬ������ֵ

extern openair0_device openair0;
//UE NB ֡�Ŷ���
extern int  gdetaslot;
extern int  gdetasubframe;
extern int  gdetaframe;


//extern uint8_t* gpMacTx;
extern EIOT_properties_t* pEIOT_properties;


#define  WTSN_SUCCESS 0

thread_together* wtsn_pthread;

MAC_FRAME_STRU T1;
MAC_FRAME_STRU T2;
MAC_FRAME_STRU T3;
MAC_FRAME_STRU T4;

void invert( double *input, double *result )
{
    int n = 2;

    double b[ 2 ][ 4 ];
    int i, j, m;
    double t;
    //�����������b
    for( i = 0; i < n; i++ )
        for( j = 0; j < n; j++)
            b[ i ][ j ] = *( input + i * n + j );

    for( i = 0; i < n; i++ )
        for( j = n; j < 2 * n; j++ )
            b[ i ][ j ] = 0;

    for( i = 0; i < n; i++ )
        b[ i ][ n + i ] = 1;

    for( m = 0; m < n; m++ )
    {
        t = b[ m ][ m ];
        i = m;
        while( b[ m ][ m ] == 0 )
        {
            b[ m ][ m ] = b[ i + 1 ][ m ];
            i++;
        }

        if( i > m )
        {
            b[ i ][ m ] = t;

            for( j = 0; j < m; j++ )
            {
                t = b[ m ][ j ];
                b[ m ][ j ] = b[ i ][ j ];
                b[ i ][ j ] = t;
            }

            for( j = m + 1; j < 2 * n; j++ )
            {
                t = b[ m ][ j ];
                b[ m ][ j ] = b[ i ][ j ];
                b[ i ][ j ] = t;
            }
        }

        for( i = m + 1; i < n; i++ )
            for( j  = 2 * n - 1; j >= m; j-- )
                b[ i ][ j ] -= b[ i ][ m ] * b[ m ][ j ] / b[ m ][ m ]; 

        for( j = 2 * n - 1; j >= m; j-- )
            b[ m ][ j ] /= b[ m ][ m ];   
    }

    m = n - 1;
    while( m > 0 )
    {
        for( i = 0; i < m; i++ )
            for( j = 2 * n - 1; j >= m; j-- )           
                b[ i ][ j ] -= b[ i ][ m ] * b[ m ][ j ];
        m--;
    }

    //�����������ά����c��?
    for( i = 0; i < n; i++ )                         
        for( j = 0; j < n; j++ )
            *( result + i * n + j ) = b[ i ][ n + j ];
}


void wTSN_init(void)
{
   
    uint32_t uwTbSizeBytes = MAC_LEN;
    
    g_tx_buffer_wtsn = (uint8_t*)malloc(uwTbSizeBytes*sizeof(uint8_t));
    g_rx_buffer_wtsn_followup= (uint8_t*)malloc(uwTbSizeBytes*sizeof(uint8_t));
    g_rx_buffer_wtsn_delayresp= (uint8_t*)malloc(uwTbSizeBytes*sizeof(uint8_t));

    gwtsn_Timestamp = (WTSN_TIMESTAMP_STRU*)malloc(sizeof(WTSN_TIMESTAMP_STRU));

   
    gwtsn_TimeFreoffset = (WTSN_TIMEFRE_OFFSET_STRU*)malloc(sizeof(WTSN_TIMEFRE_OFFSET_STRU));
    gwtsn_TimeFreoffset->wtsn_offset = (double*)malloc(WTSN_SYNC_NUM*sizeof(double));
    gwtsn_TimeFreoffset->T_timestamp = (double *)malloc(WTSN_SYNC_NUM*4*sizeof(double));
    gwtsn_TimeFreoffset->wT1stamp = (double *)malloc(WTSN_SYNC_NUM*sizeof(double));
    gwtsn_TimeFreoffset->wT2stamp= (double *)malloc(WTSN_SYNC_NUM*sizeof(double));
    gwtsn_TimeFreoffset->wT3stamp= (double *)malloc(WTSN_SYNC_NUM*sizeof(double));
    gwtsn_TimeFreoffset->wT4stamp= (double *)malloc(WTSN_SYNC_NUM*sizeof(double));
    gwtsn_TimeFreoffset->Ts_init_matrix = (double*)malloc(2*WTSN_SYNC_NUM*sizeof(double));  //2*WTSN_SYNC_NUM ������
    gwtsn_TimeFreoffset->Tp_init_matrix = (double*)malloc(4*WTSN_SYNC_NUM*sizeof(double));  //2*WTSN_SYNC_NUM��   2��  ����
    gwtsn_TimeFreoffset->Vtp_inter_var = (double*)malloc(4*sizeof(double));  //�м��� 2*2
    gwtsn_TimeFreoffset->Utp_inter_var = (double*)malloc(4*sizeof(double));  //�м��� 2*2
    gwtsn_TimeFreoffset->Stp_inter_var = (double*)malloc(4*WTSN_SYNC_NUM*WTSN_SYNC_NUM*sizeof(double));  //2*WTSN_SYNC_NUM��  2*WTSN_SYNC_NUM��
    gwtsn_TimeFreoffset->Utp_mult_Tp = (double*)malloc(4*WTSN_SYNC_NUM*sizeof(double));
    gwtsn_TimeFreoffset->Tp_mult_Utp_Tp = (double*)malloc(4*WTSN_SYNC_NUM*WTSN_SYNC_NUM*sizeof(double));
    gwtsn_TimeFreoffset->One_mult_Stp = (double*)malloc(2*WTSN_SYNC_NUM*WTSN_SYNC_NUM*sizeof(double));
    gwtsn_TimeFreoffset->Ts_mult_Stp = (double*)malloc(2*WTSN_SYNC_NUM*WTSN_SYNC_NUM*sizeof(double));
    

    gwtsn_Timestamp->uwoffset_AlphaFlg = 0;
  
    return;
}

void wTSN_free(void)
{
       //wTSN  �ڴ��ͷ�
        free(gwtsn_Timestamp);
        free(gwtsn_TimeFreoffset->T_timestamp);
        free(gwtsn_TimeFreoffset->wT1stamp);
        free(gwtsn_TimeFreoffset->wT2stamp);
        free(gwtsn_TimeFreoffset->wT3stamp);
        free(gwtsn_TimeFreoffset->wT4stamp);
        free(gwtsn_TimeFreoffset->Ts_init_matrix);
        free(gwtsn_TimeFreoffset->Tp_init_matrix);
        free(gwtsn_TimeFreoffset->Vtp_inter_var);
        free(gwtsn_TimeFreoffset->Utp_inter_var);
        free(gwtsn_TimeFreoffset->Stp_inter_var);
        free(gwtsn_TimeFreoffset->Utp_mult_Tp);
        free(gwtsn_TimeFreoffset->Tp_mult_Utp_Tp);
        free(gwtsn_TimeFreoffset->One_mult_Stp);
        free(gwtsn_TimeFreoffset->Ts_mult_Stp);
        free(gwtsn_TimeFreoffset);
        free(g_tx_buffer_wtsn);
        free(g_rx_buffer_wtsn_followup);
        free(g_rx_buffer_wtsn_delayresp);
}



int proc_wtsn_mesg(uint8_t* inputBuffer)
{

    //������BUFF���������
    
    
    MAC_STRU* pMacBuff = (MAC_STRU*)inputBuffer;
    uint32_t uwTbSizeBytes = MAC_DATA_LEN;
    //uint8_t tChannelType = pMacBuff->MacHeader.usMacChannelType;

    if(pMacBuff->MacHeader.usMacChannelType ==  WTSN_MESG_FOLLOE_TYPE)
    {
          memcpy(g_rx_buffer_wtsn_followup, pMacBuff->aucMacOffload, uwTbSizeBytes);
    }

   if(pMacBuff->MacHeader.usMacChannelType ==  WTSN_MESG_DELAY_TYPE)
    {
          memcpy(g_rx_buffer_wtsn_delayresp, pMacBuff->aucMacOffload, uwTbSizeBytes);
    }
    
    
    return 0;
    
}

uint8_t* get_wtsn_mesg (void)
{

    //��    WTSN BUFFER��������ݣ�û�з���NULL
    uint8_t* pRet = NULL;
    
    if(gwtsn_tx_buffer_flag)
    {
        pRet = g_tx_buffer_wtsn;
        gwtsn_tx_buffer_flag = 0;
        //printf("get_wtsn_mesg\n");
    }
   
    return pRet;
}

//wTSN ��Ϣ����
int wtsn_nb_deal()
{
   WTSN_P2P_MESSAGE wtsn_p2p_message;
   MAC_STRU tMacPck;
   uint32_t uwTbSizeBytes = MAC_LEN;
   uint32_t uwMAcBodyBytes = MAC_DATA_LEN;
   uint8_t RecvBuf[MAC_DATA_LEN];
   int64_t  t1_time,t4_time;
    
    //gFollowup_flag = (0 == gGetFrame%10) && (0 == gGetSubframe) && (1 == gGetSlot); //wTSN ����ʹ��
    //gDelayresp_flag = (0 == gGetFrame%10) && (0 == gGetSubframe) && (3 == gGetSlot);  //wTSN ����ʹ��

    usleep(WIAT_TIME_WTSN);
    
    t1_time = get_send_rtc (&openair0);
    t4_time = get_recv_rtc();


        if(0 != t1_time)
        {
            wtsn_p2p_message.MessageType = P2P_FOLLOW_UP;
            wtsn_p2p_message.wtimestamp = t1_time;  //wTSN�ӽӿں���ȡֵ

            
            tMacPck.MacHeader.usMacChannelType = WTSN_MESG_FOLLOE_TYPE ;
            tMacPck.MacHeader.usPacketSN = 0;
            tMacPck.MacHeader.usMacTotalLenBytes = uwMAcBodyBytes;
            tMacPck.MacHeader.usSubIndex= 0;
            tMacPck.MacHeader.usThisLen= uwMAcBodyBytes;
            tMacPck.MacHeader.ucFirstData =0;

             //printf("JY_________________test1\n");

            memcpy(RecvBuf,&wtsn_p2p_message,sizeof(wtsn_p2p_message));
            memcpy(tMacPck.aucMacOffload,RecvBuf,tMacPck.MacHeader.usThisLen*sizeof(uint8_t));
            memcpy(g_tx_buffer_wtsn, (uint8_t*)&tMacPck, uwTbSizeBytes*sizeof(uint8_t)); 
            gwtsn_tx_buffer_flag = 1;

            //printf("Follow_up tx test timestamp =%lld\n",wtsn_p2p_message.wtimestamp);
        }
        
        if(0 != t4_time)
        {
            wtsn_p2p_message.MessageType = P2P_DELAY_RESP;
            wtsn_p2p_message.wtimestamp = t4_time;  //wTSN�ӽӿں���ȡֵ


            //printf("Delay_resp tx test timestamp =%llx\n",t4_time);
            tMacPck.MacHeader.usMacChannelType = WTSN_MESG_DELAY_TYPE ;
            tMacPck.MacHeader.usPacketSN = 0;
            tMacPck.MacHeader.usMacTotalLenBytes = uwMAcBodyBytes;
            tMacPck.MacHeader.usSubIndex= 0;
            tMacPck.MacHeader.usThisLen= uwMAcBodyBytes;
            tMacPck.MacHeader.ucFirstData =0;

             //printf("JY_________________test1\n");

            memcpy(RecvBuf,&wtsn_p2p_message,sizeof(wtsn_p2p_message));
            memcpy(tMacPck.aucMacOffload,RecvBuf,tMacPck.MacHeader.usThisLen*sizeof(uint8_t));
            memcpy(g_tx_buffer_wtsn, (uint8_t*)&tMacPck, uwTbSizeBytes*sizeof(uint8_t)); 
            gwtsn_tx_buffer_flag = 1;
            
        }
   
    return 0;  
}


//wTSN ���յ�wtsn��buffer����

//wTSN  P2P����ʱ��offset
#define WTSN_Alpha_Factor 1
uint8_t wtsn_P2PTimeoffset(WTSN_TIMESTAMP_STRU *wtsn_Timestamp)
{
   int64_t wtsn_offset = 0;
   wtsn_Timestamp->wT2stamp_real = wtsn_Timestamp->wT2stamp + 2*gwtsn_T2_corr*SAMPLE_CONVER;
   wtsn_Timestamp->wT4stamp_real = wtsn_Timestamp->wT4stamp ;// gwtsn_T4_corr*SAMPLE_CONVER;
     
    wtsn_offset = -(((wtsn_Timestamp->wT2stamp_real- wtsn_Timestamp->wT1stamp)
    -(wtsn_Timestamp->wT4stamp_real - wtsn_Timestamp->wT3stamp))/2);

    wtsn_Timestamp->wtsn_offset = wtsn_offset;
    //printf("wtsn_P2PTimeoffset  history = %lld , new =%lld\n",wtsn_Timestamp->wtsn_offsetHis,wtsn_offset);
   //offset ֵ,Alpha�˲�
   if(0 == wtsn_Timestamp->uwoffset_AlphaFlg)
   {
      //wtsn_Timestamp->wtsn_offset= 0;
      wtsn_Timestamp->wtsn_offsetHis = 0;
      wtsn_Timestamp->uwoffset_AlphaFlg = 1;
    
   }
   else
   {
       wtsn_Timestamp->wtsn_offset = (1- WTSN_Alpha_Factor)*(wtsn_Timestamp->wtsn_offsetHis)
                                          + WTSN_Alpha_Factor*(wtsn_Timestamp->wtsn_offset);
       wtsn_Timestamp->wtsn_offsetHis = wtsn_Timestamp->wtsn_offset;
                                          
   }
   
   return 0;
}

//WTSN Ƶ��ƫ���OCXOƵ�ʵ�����֮��Ĺ�ϵ
uint8_t wtsn_freq_point_conv(double wtsn_fskew,uint8_t* wtsn_fFreAdj_point)
{
  unsigned long long sample_rate =  pEIOT_properties->sample_rate;  //3072000
  double rel_wtsn_fFreAdj = -(wtsn_fskew-1)*sample_rate + WTSN_FREQ_POINT;
  uint8_t  wtsn_fFreAdj_tmp = 0;
  //double fFredj_conv_factor = sample_rate/rx_freq;
  
  //double rel_wtsn_fFreAdj = wtsn_fFreAdj*fFredj_conv_factor;
  //printf("[wtsn_freq_point_conv] freq range =%f \n",wtsn_fskew);
  if((0 <= rel_wtsn_fFreAdj) && (rel_wtsn_fFreAdj < 13))
  {
     wtsn_fFreAdj_tmp = (uint8_t)((10/13)*rel_wtsn_fFreAdj);
  }

  else if((13 <= rel_wtsn_fFreAdj) && (rel_wtsn_fFreAdj < 20))
  {
     wtsn_fFreAdj_tmp = (uint8_t)((10/7)*rel_wtsn_fFreAdj- 60.0/7);
  }

  else if((20 <= rel_wtsn_fFreAdj) && (rel_wtsn_fFreAdj < 25))
  {
     wtsn_fFreAdj_tmp = (uint8_t)(2*rel_wtsn_fFreAdj- 20);
  }

  else if((25 <= rel_wtsn_fFreAdj) && (rel_wtsn_fFreAdj < 28))
  {
     wtsn_fFreAdj_tmp = (uint8_t)((10/3)*rel_wtsn_fFreAdj- 160.0/3);
  }

  else if((28 <= rel_wtsn_fFreAdj) && (rel_wtsn_fFreAdj < 34))
  {
     wtsn_fFreAdj_tmp = (uint8_t)(5*rel_wtsn_fFreAdj- 100);
  }

  else if((34 <= rel_wtsn_fFreAdj) && (rel_wtsn_fFreAdj < 41))
  {
     wtsn_fFreAdj_tmp = (uint8_t)(10*rel_wtsn_fFreAdj- 270);
  }

  else if((41 <= rel_wtsn_fFreAdj) && (rel_wtsn_fFreAdj < 43))
  {
     wtsn_fFreAdj_tmp = (uint8_t)(5*rel_wtsn_fFreAdj- 65);
  }

  else if((43 <= rel_wtsn_fFreAdj) && (rel_wtsn_fFreAdj < 47))
  {
     wtsn_fFreAdj_tmp = (uint8_t)(10*rel_wtsn_fFreAdj- 280);
  }

  else if((47 <= rel_wtsn_fFreAdj) && (rel_wtsn_fFreAdj < 51))
  {
     wtsn_fFreAdj_tmp = (uint8_t)(5*rel_wtsn_fFreAdj- 45);
  }

  else if((51 <= rel_wtsn_fFreAdj) && (rel_wtsn_fFreAdj < 54))
  {
     wtsn_fFreAdj_tmp = (uint8_t)((10/3)*rel_wtsn_fFreAdj + 40);
  }


  else if((54 <= rel_wtsn_fFreAdj) && (rel_wtsn_fFreAdj < 62))
  {
     wtsn_fFreAdj_tmp = (uint8_t)(2.5*rel_wtsn_fFreAdj + 85);
  }

  else if((62 <= rel_wtsn_fFreAdj) && (rel_wtsn_fFreAdj < 70))
  {
     wtsn_fFreAdj_tmp = (uint8_t)(1.25*rel_wtsn_fFreAdj + 162.5);
  }

  else if((70 <= rel_wtsn_fFreAdj) && (rel_wtsn_fFreAdj < 73))
  {
     wtsn_fFreAdj_tmp = (uint8_t)((5/3)*rel_wtsn_fFreAdj + 400.0/3);
  }
  else
  {
     wtsn_fFreAdj_tmp = RTC_FRE_INIT;
     //printf("[wtsn_freq_point_conv] out off freq range =%f \n",wtsn_fskew);
  }

  *wtsn_fFreAdj_point = wtsn_fFreAdj_tmp;
        
  return 0;

}
//wTSN  ����Ƶ��ƫ��
uint8_t wtsn_P2Pfreqoffset(WTSN_TIMEFRE_OFFSET_STRU *wtsn_TimeFreoffset)
{
   double* T_timestamp = wtsn_TimeFreoffset->T_timestamp;
   double* Ts_init_matrix = wtsn_TimeFreoffset->Ts_init_matrix;  //2*WTSN_SYNC_NUM ������
   double* Tp_init_matrix = wtsn_TimeFreoffset->Tp_init_matrix;  //2*WTSN_SYNC_NUM��   2��  ����
   double* Vtp_inter_var = wtsn_TimeFreoffset->Vtp_inter_var;  //�м��� 2*2
   double* Utp_inter_var = wtsn_TimeFreoffset->Utp_inter_var;  //�м��� 2*2
   double* Stp_inter_var = wtsn_TimeFreoffset->Stp_inter_var;  //2*WTSN_SYNC_NUM��  2*WTSN_SYNC_NUM��
   double* Utp_mult_Tp = wtsn_TimeFreoffset->Utp_mult_Tp;
   double* Tp_mult_Utp_Tp = wtsn_TimeFreoffset->Tp_mult_Utp_Tp;
   double* One_mult_Stp = wtsn_TimeFreoffset->One_mult_Stp;
   double* Ts_mult_Stp = wtsn_TimeFreoffset->Ts_mult_Stp;
   unsigned long long rx_freq = pEIOT_properties->rx_freq; //Ƶ�� 
   double wtsn_fFreAdj = 0;
   uint8_t wtsn_fFreAdj_point = 0;
   double delay = 0;
   double delay_deno = 0;
   double delay_mol = 0;
   double delay_mol0 = 0;
   double delay_mol1 = 0;
   double p[2]={0};     //2*1 
   double wtsn_fskew;
   
   uint32_t uwLoop;
   uint32_t uwLoop_r;
   uint32_t uwLoop_c;
   double  Vtp_value;
 
    //�����ʼ������Ts  Tp
   for(uwLoop = 0 ;uwLoop < WTSN_SYNC_NUM; uwLoop++)
   {
      *(Ts_init_matrix + uwLoop) = *(T_timestamp + 4*uwLoop) - *T_timestamp;
      *(Ts_init_matrix + uwLoop + WTSN_SYNC_NUM) = -*(T_timestamp + 4*uwLoop + 3) + *T_timestamp;
      
      *(Tp_init_matrix + 2*uwLoop) = *(T_timestamp + 4*uwLoop + 1) - *T_timestamp;
      *(Tp_init_matrix + 2*uwLoop + 2*WTSN_SYNC_NUM) = -*(T_timestamp + 4*uwLoop + 2) + *T_timestamp;
      *(Tp_init_matrix + 2*uwLoop + 1) = -1;
      *(Tp_init_matrix + 2*uwLoop + 2*WTSN_SYNC_NUM + 1) = 1;   
        
   }

   //�����м����  Vtp  Utp  Stp
   //����Vtp
   for(uwLoop_r = 0 ;uwLoop_r < 2; uwLoop_r++)
   {
       for(uwLoop_c = 0 ;uwLoop_c < 2; uwLoop_c++)
       {
          *(Vtp_inter_var + 2 * uwLoop_r + uwLoop_c) = 0;
          for(uwLoop = 0 ;uwLoop < 2 * WTSN_SYNC_NUM; uwLoop++)
          {
             *(Vtp_inter_var + 2*uwLoop_r + uwLoop_c) +=  (*(Tp_init_matrix + 2*uwLoop + uwLoop_r)) * (*(Tp_init_matrix + 2*uwLoop + uwLoop_c));
          }
       }
      
   }

   
   //����Utp
   invert( Vtp_inter_var, Utp_inter_var);


   //����Stp
     for(uwLoop_r = 0 ;uwLoop_r < 2; uwLoop_r++)
     {
       for(uwLoop_c = 0 ;uwLoop_c < 2 * WTSN_SYNC_NUM; uwLoop_c++)
       {
          *(Utp_mult_Tp + 2 * WTSN_SYNC_NUM * uwLoop_r + uwLoop_c) = 0;
          for(uwLoop= 0 ;uwLoop < 2; uwLoop++)
          {
            *(Utp_mult_Tp + 2 * WTSN_SYNC_NUM * uwLoop_r + uwLoop_c) += (*(Utp_inter_var + uwLoop + 2 * uwLoop_r)) * (*(Tp_init_matrix + uwLoop + 2 * uwLoop_c));
          }
   
       }
    }
  
   for(uwLoop_r = 0 ;uwLoop_r < 2 * WTSN_SYNC_NUM; uwLoop_r++)
   {
       for(uwLoop_c = 0 ;uwLoop_c < 2*WTSN_SYNC_NUM; uwLoop_c++)
       {
          *(Tp_mult_Utp_Tp + 2 * WTSN_SYNC_NUM * uwLoop_r + uwLoop_c) = 0;
          
          for(uwLoop = 0 ;uwLoop < 2; uwLoop++)
          {
            *(Tp_mult_Utp_Tp + 2 * WTSN_SYNC_NUM * uwLoop_r + uwLoop_c) +=  
                (*(Tp_init_matrix + uwLoop + 2 * uwLoop_r)) * 
                (*(Utp_mult_Tp + 2 * WTSN_SYNC_NUM * uwLoop + uwLoop_c));
            
          } 
          *(Stp_inter_var + 2 * WTSN_SYNC_NUM *uwLoop_r + uwLoop_c) = 1.0 
            - *(Tp_mult_Utp_Tp + 2 * WTSN_SYNC_NUM * uwLoop_r +uwLoop_c);
       }
   }

   //����Delay�ķ�ĸ
   for(uwLoop_c = 0 ;uwLoop_c < 2 * WTSN_SYNC_NUM; uwLoop_c++)
   {
      *(One_mult_Stp + uwLoop_c) = 0;
      for(uwLoop_r = 0 ;uwLoop_r < 2 * WTSN_SYNC_NUM; uwLoop_r++)
      {
         *(One_mult_Stp + uwLoop_c) += (*(Stp_inter_var + 2 * WTSN_SYNC_NUM*uwLoop_r + uwLoop_c))*2;
      }
      delay_deno += *(One_mult_Stp + uwLoop_c);
   }

  //����Delay�ķ���
  for(uwLoop = 0 ;uwLoop < 2 * WTSN_SYNC_NUM; uwLoop++)
  {
      delay_mol0 += (*(One_mult_Stp + uwLoop)) * (*(Ts_init_matrix+ uwLoop));
      for(uwLoop_c = 0 ;uwLoop_c < 2 * WTSN_SYNC_NUM; uwLoop_c++)
      {
         *(Ts_mult_Stp + uwLoop_c) = (*(Ts_init_matrix + uwLoop_c)) * (*(Stp_inter_var + 2 * WTSN_SYNC_NUM * uwLoop + uwLoop_c));; 
      }
      delay_mol1 += *(Ts_mult_Stp + uwLoop);    
  }
  delay_mol = delay_mol0 + delay_mol1;

  delay = -delay_mol/delay_deno;
    
  //���� Pֵ
  for(uwLoop_r = 0 ;uwLoop_r < 2; uwLoop_r++)
  {
     p[uwLoop_r] = 0;
     for(uwLoop_c = 0 ;uwLoop_c < 2 * WTSN_SYNC_NUM; uwLoop_c++)
     {
          p[uwLoop_r] += (*(Utp_mult_Tp + 2 * WTSN_SYNC_NUM * uwLoop_r + uwLoop_c)) * 
                         ( *(Ts_init_matrix + uwLoop_c) + delay);
     }
  }
  
  wtsn_fskew = 1.0/p[1];

  wtsn_fFreAdj = (wtsn_fskew-1.0)*rx_freq;
 
  wtsn_freq_point_conv(wtsn_fskew,&wtsn_fFreAdj_point);
 
  wtsn_TimeFreoffset->wtsn_fFreAdj = wtsn_fFreAdj;

  wtsn_TimeFreoffset->wtsn_fFreAdj_point = wtsn_fFreAdj_point;
      
   return 0;
}

uint32_t wtsn_P2P_ClockAdjust(WTSN_TIMESTAMP_STRU *wtsn_Timestamp,WTSN_TIMEFRE_OFFSET_STRU *wtsn_TimeFreoffset)
{
    double* T_timestamp = wtsn_TimeFreoffset->T_timestamp;  //WTSN_SYNC_NUM*4 ����
    double* wT1stamp = wtsn_TimeFreoffset->wT1stamp;
    double* wT2stamp = wtsn_TimeFreoffset->wT2stamp;
    double* wT3stamp = wtsn_TimeFreoffset->wT3stamp;
    double* wT4stamp = wtsn_TimeFreoffset->wT4stamp;
    double* wtsn_offset = wtsn_TimeFreoffset->wtsn_offset;
    double wtsn_woffset_sum = 0;
    uint32_t index;
    uint32_t uwLoop;
    double temp_wskew;

     //����ʱ��offset
     wtsn_P2PTimeoffset(wtsn_Timestamp);

     set_rtc_timeoffset(&openair0,(wtsn_Timestamp->wtsn_offset+WTSN_OFFSET));
     //printf("wtsn_Timestamp->wtsn_offset = %lld,  set =%lld ,t1=%lld, t2=%lld, t3=%lld, t4=%lld, deta_t2=%lld\n",(wtsn_Timestamp->wtsn_offset),(wtsn_Timestamp->wtsn_offset+51),wtsn_Timestamp->wT1stamp,wtsn_Timestamp->wT2stamp,wtsn_Timestamp->wT3stamp,wtsn_Timestamp->wT4stamp,gwtsn_T2_corr*SAMPLE_CONVER);//,wtsn_Timestamp->wT2stamp_real,wtsn_Timestamp->wT4stamp_real);

/*
    index = clock_sync_cnt % WTSN_SYNC_NUM;

    *(wT1stamp + index) = wtsn_Timestamp->wT1stamp*WTSN_SAMPLE_TIME_FACTOR;
    *(wT2stamp + index) = wtsn_Timestamp->wT2stamp_real*WTSN_SAMPLE_TIME_FACTOR;
    *(wT3stamp + index) = wtsn_Timestamp->wT3stamp*WTSN_SAMPLE_TIME_FACTOR;
    *(wT4stamp + index) = wtsn_Timestamp->wT4stamp_real*WTSN_SAMPLE_TIME_FACTOR;
    *(wtsn_offset + index) = wtsn_Timestamp->wtsn_offset*WTSN_SAMPLE_TIME_FACTOR;
    
    for(uwLoop=0 ;uwLoop < index ;uwLoop++)
    {        
        wtsn_woffset_sum +=  *(wtsn_offset + uwLoop);
    }

    *(T_timestamp + index*4) = *(wT1stamp + index);
    *(T_timestamp + index*4 + 1) = *(wT2stamp + index) - wtsn_woffset_sum;
    *(T_timestamp + index*4 + 2) = *(wT3stamp+ index) - wtsn_woffset_sum;
    *(T_timestamp + index*4 + 3) = *(wT4stamp + index);

    //printf(" %d : %f, %f, %f, %f\n", index, *(T_timestamp + index*4), *(T_timestamp + index*4 + 1), *(T_timestamp + index*4 + 2), *(T_timestamp + index*4 + 3));


    //����Ƶ��offset
    if(9 == index)
    {
        temp_wskew = (*(wT2stamp + 9) - *(wT2stamp))/( *(wT1stamp + 9) - *(wT1stamp)); 
        //printf("t2_10 = %f, t2_1=%f, t1_10=%f, t1_1=%f , temp_wskew=%f\n",*(wT2stamp + 9),*(wT2stamp),*(wT1stamp + 9),*(wT1stamp),temp_wskew);
        wtsn_P2Pfreqoffset(wtsn_TimeFreoffset);
        
        
        //P2P offsetӦ��  
    
        //set_rtc_freqoffset(&openair0,(int64_t)(wtsn_TimeFreoffset->wtsn_fFreAdj_point));
    }

    
  */  
     return 0;
}

//P2P ��Ϣ����
int wtsn_ue_deal()
{

   //printf("FXB: jump in [wtsn_ue_deal]______________________\n");
   uint32_t uwRst = WTSN_SUCCESS;

   WTSN_TIMESTAMP_STRU *wtsn_Timestamp = gwtsn_Timestamp;
   WTSN_TIMEFRE_OFFSET_STRU *wtsn_TimeFreoffset = gwtsn_TimeFreoffset;
   WTSN_P2P_MESSAGE *wtsn_followup_message = (WTSN_P2P_MESSAGE*)(g_rx_buffer_wtsn_followup); 
   WTSN_P2P_MESSAGE *wtsn_delayresp_message = (WTSN_P2P_MESSAGE*)(g_rx_buffer_wtsn_delayresp); 

   wtsn_TimeFreoffset->wtsn_offset = gwtsn_TimeFreoffset->wtsn_offset;
   wtsn_TimeFreoffset->T_timestamp = gwtsn_TimeFreoffset->T_timestamp;
   wtsn_TimeFreoffset->wT1stamp = gwtsn_TimeFreoffset->wT1stamp;
   wtsn_TimeFreoffset->wT2stamp =  gwtsn_TimeFreoffset->wT2stamp;
   wtsn_TimeFreoffset->wT3stamp = gwtsn_TimeFreoffset->wT3stamp;
   wtsn_TimeFreoffset->wT4stamp = gwtsn_TimeFreoffset->wT4stamp;
   wtsn_TimeFreoffset->Ts_init_matrix = gwtsn_TimeFreoffset->Ts_init_matrix; 
   wtsn_TimeFreoffset->Tp_init_matrix = gwtsn_TimeFreoffset->Tp_init_matrix; 
   wtsn_TimeFreoffset->Vtp_inter_var = gwtsn_TimeFreoffset->Vtp_inter_var; 
   wtsn_TimeFreoffset->Utp_inter_var =  gwtsn_TimeFreoffset->Utp_inter_var; 
   wtsn_TimeFreoffset->Stp_inter_var =  gwtsn_TimeFreoffset->Stp_inter_var;  
   wtsn_TimeFreoffset->Utp_mult_Tp = gwtsn_TimeFreoffset->Utp_mult_Tp;
   wtsn_TimeFreoffset->Tp_mult_Utp_Tp = gwtsn_TimeFreoffset->Tp_mult_Utp_Tp;
   wtsn_TimeFreoffset->One_mult_Stp = gwtsn_TimeFreoffset->One_mult_Stp;
   wtsn_TimeFreoffset->Ts_mult_Stp = gwtsn_TimeFreoffset->Ts_mult_Stp;

   int64_t  t2_time,t3_time;

   usleep(WIAT_TIME_WTSN);

   t2_time = get_recv_rtc();
   if(t2_time > 0)
   {
       //printf("t2= %lld\n", t2_time);
       gT2_flag = t2_time;
       gT1_flag=0;
       gT3_flag=0;
       gT4_flag=0; 
       
   }

   if(wtsn_followup_message->MessageType == P2P_FOLLOW_UP)
   {
      //printf("t1 =%lld\n",wtsn_followup_message->wtimestamp);
      gT1_flag = wtsn_followup_message->wtimestamp;
      wtsn_followup_message->MessageType = P2P_INVALUE;
      
   }

   if((gT2_flag > 0) && (gT1_flag == 0))
   {
      if(wtsn_followup_message->MessageType == P2P_FOLLOW_UP)
       {
          //printf("t1 =%lld\n",wtsn_followup_message->wtimestamp);
          gT1_flag = wtsn_followup_message->wtimestamp;
          wtsn_followup_message->MessageType = P2P_INVALUE;
          
       }
   }

   if((gT2_flag > 0) && (gT1_flag > 0) && (gT3_flag == 0))
   {
       t3_time = get_send_rtc (&openair0);
       if(0 != t3_time)
       {
           //printf("t3= %lld\n", t3_time);
           gT3_flag = t3_time;
       }
   }


   if((gT2_flag > 0) && (gT1_flag > 0)&& (gT3_flag > 0) && (gT4_flag == 0))
   {
      if(wtsn_delayresp_message->MessageType == P2P_DELAY_RESP)
       {
           //printf("t4 =%lld\n",wtsn_delayresp_message->wtimestamp);
           gT4_flag = wtsn_delayresp_message->wtimestamp;
           wtsn_delayresp_message->MessageType = P2P_INVALUE;
       }
   }
  
   if( (gT1_flag!=0) && (gT2_flag!=0) && (gT3_flag!=0) && (gT4_flag!=0) )
   {

       wtsn_Timestamp->wT1stamp = gT1_flag;//��������֡�ţ���֡����δ���
       wtsn_Timestamp->wT2stamp = gT2_flag;//��������֡�ţ���֡����δ���
       wtsn_Timestamp->wT3stamp = gT3_flag;//��������֡�ţ���֡����δ���
       wtsn_Timestamp->wT4stamp = gT4_flag;//��������֡�ţ���֡����δ���
       uwRst = wtsn_P2P_ClockAdjust(wtsn_Timestamp,wtsn_TimeFreoffset);
       //printf("get_wtsn deta t= %lld\n",wtsn_TimeFreoffset->wtsn_offset);
       if(WTSN_SUCCESS != uwRst)
       {
           return uwRst;
       }

       clock_sync_cnt++;
       if(clock_sync_cnt >= WTSN_SYNC_PER)
       {
           clock_sync_cnt = 0;
       }
          
       gT1_flag = 0;
       gT2_flag = 0;
       gT3_flag = 0;
       gT4_flag = 0;        
              
   }
    return 0;
}



static void* wtsn_P2P_func( void* param)
{
    //printf("[P2P_func] (TID %ld) started on CPU %d\n", gettid(), sched_getcpu());
    uint8_t eNBUEflag = pEIOT_properties->NBUE_flag;

    sleep(4);

    while(1)
    {

            if(UE == eNBUEflag)
            {
                wtsn_ue_deal();
            }
            else
            {
                wtsn_nb_deal();
            }      
        
    }
    return;

}


int init_wtsn_tran_proc(void)
{
    wtsn_pthread = (thread_together*)malloc(sizeof(thread_together));
    threadstruct_init(wtsn_pthread);
    wtsn_pthread->schedparam.sched_priority = sched_get_priority_max(SCHED_FIFO) - 5;
    wtsn_pthread->threadflag = WTSNTHREAD;
    wtsn_pthread->cpuid = WTSN_CPUID;
    snprintf(wtsn_pthread->name, sizeof(wtsn_pthread->name), "wtsn_pthread" );
    thread_gather(wtsn_pthread,wtsn_P2P_func,NULL);   

    return 0;
}

int kill_wtsn_tran_proc(void)
{
      thread_end(wtsn_pthread);
      //free(wtsn_send_pthread);
      free(wtsn_pthread);
      return 0;
}



//WTSN SIB��Ϣ����
int wTSN_SIB_send( int targtFrame,  int targtSubFrame, int targtSubSlot, int ue_idx)
{
    uint8_t *pMacTx = NULL;
    //wTSN  SIB
    WTSN_SIB_MESSAGE wtsn_sibmessage;
    MAC_STRU tMacPck;
    uint32_t uwTbSizeBytes = MAC_LEN;
    uint32_t uwMAcBodyBytes = MAC_DATA_LEN;

    pMacTx = get_tx_mac_pnt(ue_idx);
    
    tMacPck.MacHeader.usMacChannelType = WTSN_SIB_TYPE;
    tMacPck.MacHeader.usPacketSN = 0;
    tMacPck.MacHeader.usMacTotalLenBytes = uwMAcBodyBytes;
    tMacPck.MacHeader.usSubIndex= 0;
    tMacPck.MacHeader.usThisLen= uwMAcBodyBytes;
    tMacPck.MacHeader.ucFirstData =0; 

    wtsn_sibmessage.MessageType = P2P_SIB;
    wtsn_sibmessage.wFrmIdx = targtFrame;
    wtsn_sibmessage.wSubFrmIdx = targtSubFrame;
    wtsn_sibmessage.wSlotIdx = targtSubSlot;
    //printf("SEND_SIB_MSG-------wFrmIdx %d,%d,%d\n",wtsn_sibmessage.wFrmIdx,
    //        wtsn_sibmessage.wSubFrmIdx,wtsn_sibmessage.wSlotIdx);
    //printf("Jy test--------------------\n");

    memcpy(tMacPck.aucMacOffload,&wtsn_sibmessage,sizeof(wtsn_sibmessage));
    memcpy(pMacTx,(uint8_t*)&tMacPck,uwTbSizeBytes);

    return 0;

    
}



int wTSN_SIB_recv(uint8_t* inputBuffer, int wSysFrame,int wSysSubFrame,int wSysSlot)
{
   MAC_STRU* pMacBuff = (MAC_STRU*)inputBuffer;
   uint32_t uwTbSizeBytes = MAC_LEN;
   WTSN_SIB_MESSAGE *wtsn_sib_message = (WTSN_SIB_MESSAGE*)(pMacBuff->aucMacOffload);
   
  if(wtsn_sib_message->MessageType == P2P_SIB)
   {
            
       /*
        printf("RECV SIB MESSAGE-------[RECV]%d.%d.%d, [Client]%d.%d.%d\n",
                    wtsn_sib_message->wFrmIdx, wtsn_sib_message->wSubFrmIdx, wtsn_sib_message->wSlotIdx,
                    wSysFrame, wSysSubFrame, wSysSlot);
       */    
       // if((wtsn_sib_message->wFrmIdx==last_frame)&&(wtsn_sib_message->wSubFrmIdx==last_subframe)&&(wtsn_sib_message->wSlotIdx==last_slot))
              //  return 0;

           
           /*
            printf("receive SIB message:ue sfn[%d.%d.%d], nb sfn[%d.%d.%d]\n",
                   wSysFrame, wSysSubFrame, wSysSlot,
                   wtsn_sib_message->wFrmIdx, wtsn_sib_message->wSubFrmIdx, wtsn_sib_message->wSlotIdx);
            */

           if((wSysSlot-RX_OFFSET)<0)
           {
                wSysSlot=wSysSlot-RX_OFFSET+SLOT_NUM;
                wSysSubFrame=wSysSubFrame-1;
           }
           if(wSysSubFrame<0)
           {
                wSysSubFrame=wSysSubFrame+SUBFRAME_NUM;
                wSysFrame=wSysFrame-1;

           }
           if(wSysFrame<0)
           {
                wSysFrame=wSysFrame+FRAME_NUM;
           }

           gdetaframe = wtsn_sib_message->wFrmIdx -wSysFrame;
           gdetasubframe = wtsn_sib_message->wSubFrmIdx - wSysSubFrame;
           gdetaslot = wtsn_sib_message->wSlotIdx - wSysSlot;

           //last_frame=wtsn_sib_message->wFrmIdx;
           //last_subframe=wtsn_sib_message->wSubFrmIdx;
           //last_slot=wtsn_sib_message->wSlotIdx;
           // printf("recv SIB MSG**deta_wFrmIdx %d,%d,%d\n",gdetaframe,gdetasubframe, gdetaslot);
            /*

            if((0 == gdetaframe)&&(0 == gdetasubframe)&&(0== gdetaslot))
            {
                printf("JY test rx-------gdetaframe %d,%d,%d\n",gdetaframe,gdetasubframe
                ,gdetaslot);

            }
            */
             
    }

  return 0;
}



