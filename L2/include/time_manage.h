#include "global_def.h"
#include "common_lib.h"

void init_slot_frame();
void fix_send_frametime(int slot, int subframe, int frame, int* wslot, int *wsubframe, int* wframe, int offset);
int  update_slot_frame(int *detaslot, int *detasubframe, int *detaframe);
void fix_slot_frame(int tpstamp);
void get_manage_frame(int wSysSlot, int wSysSubFrame, int wSysFrame, int *targtSubSlot, int *targtSubFrame, int *targtFrame);
void get_target_Time_Node(int *Slot, int *Subframe, int *Frame, int TargetOffset);
void get_SIB_deta_time (int *detaslot, int *detasubframe, int *detaframe);