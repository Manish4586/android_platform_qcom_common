/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
  Copyright (c) 2013 Qualcomm Technologies, Inc.  All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.
=============================================================================*/
/*=============================================================================*/
/* DO NOT EDIT THIS FILE IF YOU DO NOT HAVE ACCESS TO SOURCE FOR IZAT BINARIES.
   THIS PROXY ACTS AS A LINK TO IZAT BINARIES.
   EDITING AND RECOMPILING THIS FILE WITHOUT RECOMPILING IZAT BINARIES
   CAN CAUSE UNKNOWN BEHAVIOR*/
/*=============================================================================*/

#include <LBSApiV02.h>
#include <IzatApiV02.h>
#include <LocApiProxy.h>

#ifdef _HAS_LOC_V02_
LocApiProxyV02 :: LocApiProxyV02(LBSApiV02 *lbsApi) :
    mLBSApi(lbsApi),
    mIzatApi(new IzatApiV02(this))
{
    LOC_LOGD("%s:%d]: LocApiProxyV02 created:%p, mLBSApi:%p, mIzatApi:%p, ",
             __func__, __LINE__, this, mLBSApi, mIzatApi);
}

LocApiProxyV02 :: ~LocApiProxyV02()
{
    delete mIzatApi;
}

int LocApiProxyV02 :: eventCb(locClientHandleType client_handle,
                          uint32_t loc_event_id,
                          locClientEventIndUnionType loc_event_payload)
{
    return mIzatApi->eventCb(client_handle, loc_event_id, loc_event_payload);
}

#endif //_HAS_LOC_V02_

#ifdef _HAS_LOC_RPC_
LocApiProxyRpc :: LocApiProxyRpc(LBSApiRpc *lbsApi) :
    mLBSApi(lbsApi),
    mIzatApi(new IzatApiRpc(this))
{
    LOC_LOGD("%s:%d]: LocApiProxyRpc created:%p, mLBSApi:%p, mIzatApi:%p, ",
             __func__, __LINE__, this, mLBSApi, mIzatApi);
}

LocApiProxyRpc :: ~LocApiProxyRpc()
{
    delete mIzatApi;
}

int LocApiProxyRpc :: locEventCB(rpc_loc_client_handle_type handle,
                                 rpc_loc_event_mask_type event,
                                 const rpc_loc_event_payload_u_type* payload)
{
    return mIzatApi->locEventCB(handle, event, payload);
}
#endif _HAS_LOC_RPC_
