#include "time_manage.h"

volatile int gUsrpDataCount = 0;
volatile int gSlot = 0;
volatile int gSubframe = 0;
volatile int gFrame = 0;
volatile long int gStartDelay = 0;

volatile int gGetSlot = 0;
volatile int gGetSubframe = 0;
volatile int gGetFrame = 0;

//UE NB Ö¡ºÅ¶ÔÆë
volatile int  gdetaslot = 0;
volatile int  gdetasubframe = 0;
volatile int  gdetaframe = 0;

extern openair0_config_t openair0_cfg;

void init_slot_frame()
{
    gSlot = 0;
    gSubframe = 0;
    gFrame = 0;
    gStartDelay = 0;
    gUsrpDataCount = 0;
    
    gGetSlot = 0;
    gGetSubframe = 0;
    gGetFrame = 0;
}

int update_slot_frame(int* detaslot, int* detasubframe, int* detaframe)
{
    gGetSlot = gSlot;
    gGetSubframe = gSubframe;
    gGetFrame = gFrame;

    gStartDelay++;
    gUsrpDataCount++;

    if(gUsrpDataCount == DATA_LEN)
    {
        gUsrpDataCount = 0;
    }

    gSlot++;
    gSlot=gSlot+(*detaslot);
    //*detaslot=0;
    if(gSlot >= SLOT_NUM)
    {
        gSlot=gSlot-SLOT_NUM;
        gSubframe ++;
    }
    else if(gSlot < 0)
    {
        gSlot=gSlot+SLOT_NUM;
        gSubframe --;
    }

    if(*detaslot !=0)
    {
        gSlot=gSlot+RX_OFFSET;
        *detaslot=0;
        if(gSlot >= SLOT_NUM)
        {
            gSlot=gSlot-SLOT_NUM;
            gSubframe ++;
        }
    }

    gSubframe=gSubframe+(*detasubframe);
    *detasubframe=0;

    if(gSubframe >= SUBFRAME_NUM)
    {
        gSubframe=gSubframe-SUBFRAME_NUM;
        gFrame++;
    }
    if(gSubframe < 0)
    {
        gSubframe=gSubframe+SUBFRAME_NUM;
        gFrame--;
    }

    gFrame=gFrame+(*detaframe);
    *detaframe=0;
    if(gFrame >= FRAME_NUM)
    {
        gFrame=gFrame-FRAME_NUM;
    } 

    if(gFrame < 0)
    {
        gFrame=gFrame+FRAME_NUM;
    }

    return 0;
}


void fix_slot_frame(int tpstamp)
{
    gUsrpDataCount = (tpstamp%(openair0_cfg.samples_per_frame))/RX_DATA_MSEC_LEN;
    gSlot = (tpstamp%(openair0_cfg.samples_per_frame/10))/RX_DATA_MSEC_LEN;
    gSubframe = (tpstamp%(openair0_cfg.samples_per_frame))/((openair0_cfg.samples_per_frame)/10);
    gFrame = (tpstamp%(openair0_cfg.samples_per_frame*1024))/(openair0_cfg.samples_per_frame);
    return 0;
}


void get_SIB_deta_time (int *detaslot, int *detasubframe, int *detaframe)
{
    int lSlot , lSubframe, lFrame;

    if(detaslot != NULL)
    {
        *detaslot = gdetaslot;
        gdetaslot=0;
    }

    if(detasubframe != NULL)
    {
        *detasubframe = gdetasubframe;
        gdetasubframe=0;
    }

    if(detaframe != NULL)
    {

        *detaframe = gdetaframe;
        gdetaframe=0;
    }
    if((*detaslot != 0)||(*detasubframe != 0)||(*detaframe != 0))
    {
        printf("deta in SIB get time %d,%d,%d\n",*detaframe,*detasubframe, *detaslot);
    }
    return ;
    
}


void get_manage_frame(int wSysSlot, int wSysSubFrame, int wSysFrame, int *targtSubSlot, int *targtSubFrame, int *targtFrame)
{

    if(wSysSlot == 0)
    {
        *targtSubSlot = 6;
        *targtSubFrame = wSysSubFrame;
        *targtFrame = wSysFrame;
    }
    else if(wSysSlot == 1)
    {
        *targtSubSlot = 7;
        *targtSubFrame = wSysSubFrame;
        *targtFrame = wSysFrame;
    }
    else
    {
        *targtSubSlot = wSysSlot - 2;
        *targtSubFrame = wSysSubFrame +1;
        if(*targtSubFrame >=SUBFRAME_NUM)
        {
            *targtSubFrame = 0;
            if((wSysFrame + 1) >= FRAME_NUM)
            {
                *targtFrame = 0;
            }
            else
            {
                *targtFrame = wSysFrame + 1;
            }
        }
        else
        {
            *targtFrame = wSysFrame;
        }
    }
    return ;
}


void get_target_Time_Node(int *Slot, int *Subframe, int *Frame, int TargetOffset)
{

    int lSlot , lSubframe, lFrame;

    if(Slot != NULL)
    {
        lSlot = gGetSlot;
    }

    if(Subframe != NULL)
    {
        lSubframe = gGetSubframe;
    }

    if(Frame != NULL)
    {

        lFrame = gGetFrame;
    }
 
    *Slot=lSlot;
    *Subframe=lSubframe;
    *Frame=lFrame;
    
    return ;
}



void fix_send_frametime( int slot, int subframe, int frame, int* wslot, int *wsubframe, int* wframe, int offset)
{

	*wslot = offset+slot;
	*wsubframe = subframe;
	*wframe = frame;
	if(*wslot >= SLOT_NUM)
	{
		*wslot = *wslot-SLOT_NUM;
		*wsubframe = *wsubframe+1;
    }
    if(*wsubframe >= SUBFRAME_NUM)
    {
        *wsubframe = *wsubframe-SUBFRAME_NUM;
        *wframe = *wframe+1;
    }
    if(*wframe >= FRAME_NUM)
    {
        *wframe = *wframe-FRAME_NUM;
    }
}

