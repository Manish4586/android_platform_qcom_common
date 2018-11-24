/*
 * Copyright (c) 2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 * Qualcomm Technologies Proprietary and Confidential.
 *
 * Not a Contribution.
 * Apache license notifications and license are retained
 * for attribution purposes only.
 */

/******************************************************************************
 *
 *  Copyright (C) 2010-2014 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/


/******************************************************************************
 *
 *  NFA interface for device management
 *
 ******************************************************************************/
#include <string.h>
#include "nfa_api.h"
#include "nfa_sys.h"
#include "nfa_dm_int.h"
#include "nfa_ce_int.h"
#include "nfa_sys_int.h"
#include "ndef_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

#ifdef DTA // <DTA>
#include "dta_mode.h"
#endif // </DTA>
/*****************************************************************************
**  Constants
*****************************************************************************/
#ifdef DTA
    /* This is defined as extern in src/include/dta_flag.h
     * Here is the first setting of value to it
     */
int dta_Pattern_Number = -1;
#define CONFIG_MAX_LEN 256 // as in HAL
static UINT8 sConfig [CONFIG_MAX_LEN]; // normal mode
static UINT8 sConfigDTA [CONFIG_MAX_LEN]; //dta mode
static UINT8 sConfigDTA2 [CONFIG_MAX_LEN]; //dta for poll B w. sleep response
static UINT8 sConfigDefault[] = {0x03,0x80,0x01,0x00};
static UINT8* p_values = (UINT8 *)sConfigDefault;
static UINT8* p_valuesDTA = (UINT8 *)sConfigDefault;
static UINT8* p_valuesDTA2 = (UINT8 *)sConfigDefault;
#endif

/*****************************************************************************
**  APIs
*****************************************************************************/

#ifdef DTA
/*******************************************************************************
**
** Function         NFA_DTA_Set_Pattern_Number
**
** Description      This function is needed to give lower levels knowledge of
**                  the current DTA pattern number. If pattern number is
**                  0 or greater the DTA mode is on. This is stored in a file
**                  in the data partition. The file is also read by the HAL.
**                  This is the only place the file is written.
**
**                  pattern: The pattern number to be set. Negative values set
**                      normal mode, positive values set DTA mode with
**                      corresponding pattern number.
**
*******************************************************************************/
tNFA_STATUS NFA_DTA_Set_Pattern_Number (int pattern)
{

    tNFA_DM_DTA_SET_PATTERN *p_msg = NULL;
    UINT8 length = 0;
    UINT8 data[CONFIG_MAX_LEN];
    BOOLEAN success = NFA_DTA_WritePatternToFile(pattern);

    if(!success)
    {
        NFA_TRACE_WARNING0("Couldn't store pattern to file");
    }

    NFA_TRACE_API1 ("NFA_DTA_Set_Pattern_Number: %d", pattern);

    memset(data, 0x00, CONFIG_MAX_LEN);
    if(pattern < 0)
    { // -1 normal, -2 EMVCO
         length = p_values[0];
         memcpy(data, &p_values[1], length);
    }
    else if (pattern == 43)
    {
         length = p_valuesDTA2[0];
         memcpy(data, &p_valuesDTA2[1], length);
    }
    else
    { // (pattern >= 0 and != 43)
         length = p_valuesDTA[0];
         memcpy(data, &p_valuesDTA[1], length);
    }
    dta_Pattern_Number = pattern;
    if ((p_msg = (tNFA_DM_DTA_SET_PATTERN *) GKI_getbuf ((UINT16) (sizeof (tNFA_DM_API_SET_CONFIG) + length ))) != NULL)
    {
        p_msg->hdr.event = NFA_DM_API_DTA_SET_PATTERN_NUM;
        p_msg->length   = length;
        p_msg->p_data   = (UINT8 *) (p_msg + 1);
        /* Copy parameter data */
        memcpy (p_msg->p_data, data, length);
        nfa_sys_sendmsg (p_msg);
        return (NFA_STATUS_OK);
    }
    return (NFA_STATUS_FAILED);
}
/*******************************************************************************
**
** Function         NFA_DTA_Get_Pattern_Number
**
** Description      This function is needed to give lower levels knowledge of
**                  the current DTA pattern number. If pattern number is
**                  0 or greater the DTA mode is on.
**
** Returns          The current pattern number.
**
*******************************************************************************/
int NFA_DTA_Get_Pattern_Number ()
{
  dta_Pattern_Number = (int)NFA_DTA_ReadPatternFromFile();
  NFA_TRACE_API1 ("NFA_DTA_Get_Pattern_Number: %d", dta_Pattern_Number);
  return dta_Pattern_Number;
}

/*******************************************************************************
**
** Function         NFA_NFC_Deactivate
**
** Description      This function is needed to send NFC-DEP DSL request.
**                  If NFA_Deactivate function is called with sleep_mode=TRUE
**                  the execution goes to nfa_dm_act_deactivate where sleep
**                  mode is not allowed for NFC-DEP
**
**                  If rls_request=TRUE:
**                      Deactivate the connection with RLS request.
**
**                  If rls_request=FALSE:
**                      Deselect the activated device by deactivating into sleep
**                      mode. This is done by sending a DSL request.
**
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_NFC_Deactivate (BOOLEAN rls_request)
{
  NFA_TRACE_API0 ("NFA_NFC_Deactivate ()");

  tNFC_STATUS status;

  // NFC_DEACTIVATE_TYPE_IDLE corresponds to RLS_REQ and
  // NFC_DEACTIVATE_TYPE_SLEEP_AF to DSL_REQ
  if (rls_request) {

    status = NFC_Deactivate(NFC_DEACTIVATE_TYPE_IDLE);
  } else {

    status = NFC_Deactivate(NFC_DEACTIVATE_TYPE_SLEEP_AF);
  }

  if (status == NFC_STATUS_OK) {

    return NFA_STATUS_OK;
  }
  else {

    return NFA_STATUS_FAILED;
  }
}

/*******************************************************************************
**
** Function         NFA_DTA_Mode
**
** Description      This function reads the startup configuration from the
**                  configuration file and stores them in a buffer.
**                  Moreover it checks whether the NFA thread should
**                  startup in DTA or in normal mode.
**
**
** Returns          None
**
*******************************************************************************/
void NFA_DTA_Mode (void) {
    int actualLen = 0;
    long int pattern = NFA_DTA_ReadPatternFromFile();

    if(pattern >= 0) {
        NFA_TRACE_API0 ("NFA_DTA_MODE");
    } else {
        NFA_TRACE_API0 ("NFA_NORMAL_MODE");
    }

    actualLen = GetStrValue (NAME_NFA_DM_START_UP_CFG, (char*)sConfig, sizeof(sConfig));
    if(!actualLen) {
        NFA_TRACE_ERROR1("Couldn't read %s from config file", NAME_NFA_DM_START_UP_CFG);
    } else {
        p_values = (UINT8 *)sConfig;
    }

    actualLen = GetStrValue (NAME_NFA_DM_START_UP_CFG_DTA, (char*)sConfigDTA, sizeof(sConfigDTA));
    if(!actualLen) {
        NFA_TRACE_ERROR1("Couldn't read %s from config file", NAME_NFA_DM_START_UP_CFG_DTA);
    } else {
        p_valuesDTA = (UINT8 *)sConfigDTA;
    }

    actualLen = GetStrValue (NAME_NFA_DM_START_UP_CFG_DTA2, (char*)sConfigDTA2, sizeof(sConfigDTA2));
    if(!actualLen) {
        NFA_TRACE_ERROR1("Couldn't read %s from config file", NAME_NFA_DM_START_UP_CFG_DTA2);
    } else {
        p_valuesDTA2 = (UINT8 *)sConfigDTA2;
    }

    dta_Pattern_Number = (int)pattern;
    return;
}


/*******************************************************************************
**
** Function         NFA_DTA_WritePatternToFile
**
** Description      This function writes the pattern to the data
**                  partition. Not exported.
**
** Returns          TRUE on success
**                  FALSE otherwise
**
*******************************************************************************/
BOOLEAN NFA_DTA_WritePatternToFile(int pattern) {
    int size = 0;
    int wSize = 0;
    char patternBuffer[MAX_PATTERN_LENGTH] = {0};
    FILE *patternFile = NULL;
    int ret = 0;

    size = snprintf(patternBuffer, sizeof(patternBuffer), "%d", pattern);
    if(size == 0) {
        NFA_TRACE_ERROR1("Couldn't convert pattern: %d", pattern);
    }
    errno = 0;
    patternFile = fopen(PATTERN_FILE_NAME, "w");

    if(patternFile != NULL) {
        errno = 0;
        wSize = fwrite(patternBuffer, sizeof(char), size+1, patternFile);
        if(wSize == 0) {
          NFA_TRACE_ERROR1("Couldn't write to file: %s", strerror(errno));
        }
        ret = fclose(patternFile);
        if(ret) {
          NFA_TRACE_ERROR1("Couldn't close pattern file: %s", PATTERN_FILE_NAME);
          return FALSE;
        }
        return TRUE;
    } else {
        NFA_TRACE_ERROR1("Cannot open pattern file: %s", strerror(errno));
        return FALSE;
    }
}

/*******************************************************************************
**
** Function         NFA_DTA_ReadPatternFromFile
**
** Description      This function reads the stored pattern from the data
**                  partition. Not exported.
**
** Returns          The stored pattern (-1,0,...,FFFF)
**                  -1 for errors to enforce normal mode
**
*******************************************************************************/
long int NFA_DTA_ReadPatternFromFile(void) {
    long int pattern = -1;
    FILE *patternFile = NULL;
    long int lSize = 0;
    size_t rSize = 0;
    char patternBuffer[MAX_PATTERN_LENGTH] = {0};
    char *endp = NULL;
    int ret = 0;

    errno = 0;
    patternFile = fopen(PATTERN_FILE_NAME, "r");
    if(patternFile != NULL) {
        // obtain file size:
        ret = fseek (patternFile, 0, SEEK_END);
        if(ret) {
            goto err;
        }
        lSize = ftell (patternFile);
        rewind (patternFile);
        // boundary check
        if(lSize > (long int)sizeof(patternBuffer)) {
            lSize = sizeof(patternBuffer);
        }
        errno = 0;
        rSize = fread(patternBuffer, sizeof(char), lSize, patternFile);
        if (!rSize) {
            NFA_TRACE_ERROR1("Couldn't read from file: %s", strerror(errno));
            goto err;
        } else {
            if(rSize < sizeof(patternBuffer)) {
                patternBuffer[rSize] = '\0';
            } else {
                patternBuffer[MAX_PATTERN_LENGTH-1] = '\0';
            }
        }
        errno = 0;
        pattern = strtol (patternBuffer, &endp, 0);
        // error checking for out of range and conversion errors
        if (errno == ERANGE) {
            NFA_TRACE_ERROR1("Conversion failure Out of Bounds, assume normal mode, contents read: %s", patternBuffer);
            goto err;
        } else if(pattern == 0L) {
            // Check whether real zero or error condition
            if(patternBuffer != endp && *endp == '\0') {
                //actual zero read
            } else {
                NFA_TRACE_ERROR1("Conversion failure No Real Zero, assume normal mode, contents read: %s", patternBuffer);
                goto err;
            }
        }
        ret = fclose(patternFile);
        if(ret) {
            NFA_TRACE_ERROR1("Couldn't close pattern file: %s", PATTERN_FILE_NAME);
        }
    } else {
        NFA_TRACE_ERROR1("Cannot open pattern file: %s", strerror(errno));
    }
    return pattern;
err:
    ret = fclose(patternFile);
    if(ret) {
        NFA_TRACE_ERROR1("Couldn't close pattern file: %s", PATTERN_FILE_NAME);
    }
    return -1;
}
#endif // </DTA>
/*******************************************************************************
**
** Function         NFA_Init
**
** Description      This function initializes control blocks for NFA
**
**                  p_hal_entry_tbl points to a table of HAL entry points
**
**                  NOTE: the buffer that p_hal_entry_tbl points must be
**                  persistent until NFA is disabled.
**
** Returns          none
**
*******************************************************************************/
void NFA_Init(tHAL_NFC_ENTRY *p_hal_entry_tbl)
{
    NFA_TRACE_API0 ("NFA_Init ()");
    nfa_sys_init();
    nfa_dm_init();
    nfa_p2p_init();
    nfa_cho_init();
    nfa_snep_init(FALSE);
    nfa_rw_init();
    nfa_ce_init();
    nfa_ee_init();
    if (nfa_ee_max_ee_cfg != 0)
    {
        nfa_dm_cb.get_max_ee    = p_hal_entry_tbl->get_max_ee;
        nfa_hci_init();
    }


    /* Initialize NFC module */
    NFC_Init (p_hal_entry_tbl);
}

/*******************************************************************************
**
** Function         NFA_Enable
**
** Description      This function enables NFC. Prior to calling NFA_Enable,
**                  the NFCC must be powered up, and ready to receive commands.
**                  This function enables the tasks needed by NFC, opens the NCI
**                  transport, resets the NFC controller, downloads patches to
**                  the NFCC (if necessary), and initializes the NFC subsystems.
**
**                  This function should only be called once - typically when NFC
**                  is enabled during boot-up, or when NFC is enabled from a
**                  settings UI. Subsequent calls to NFA_Enable while NFA is
**                  enabling or enabled will be ignored. When the NFC startup
**                  procedure is completed, an NFA_DM_ENABLE_EVT is returned to the
**                  application using the tNFA_DM_CBACK.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_Enable (tNFA_DM_CBACK        *p_dm_cback,
                        tNFA_CONN_CBACK      *p_conn_cback)
{
    tNFA_DM_API_ENABLE *p_msg;

    NFA_TRACE_API0 ("NFA_Enable ()");
#ifdef DTA
    NFA_DTA_Mode();
#endif

    /* Validate parameters */
    if ((!p_dm_cback) || (!p_conn_cback))
    {
        NFA_TRACE_ERROR0 ("NFA_Enable (): error null callback");
        return (NFA_STATUS_FAILED);
    }

    if ((p_msg = (tNFA_DM_API_ENABLE *) GKI_getbuf (sizeof (tNFA_DM_API_ENABLE))) != NULL)
    {
        p_msg->hdr.event    = NFA_DM_API_ENABLE_EVT;
        p_msg->p_dm_cback   = p_dm_cback;
        p_msg->p_conn_cback = p_conn_cback;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}
/*****************************************************************************************
**
** Function         NFA_StoreShutdownReason
**
** Description      This function is called to store the reason of shut down in stack.

** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************************/
tNFA_STATUS NFA_StoreShutdownReason (UINT8 reason)
{
    tNFA_DM_STORE_REASON *p_msg;

    NFA_TRACE_API1 ("NFA_StoreShutdownReason (reason=%d)", reason);

    if ((p_msg = (tNFA_DM_API_DISABLE *) GKI_getbuf (sizeof (tNFA_DM_API_DISABLE))) != NULL)
    {
        p_msg->hdr.event = NFA_DM_STORE_REASON_OF_SHUTDOWN;
        p_msg->reason  = reason;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}
/*******************************************************************************
**
** Function         NFA_CheckDeviceResetStatus
**
** Description      This function will collect information whether the device has
**                  been reset and will pass this info further to HAL.
**
** Returns          void
**
*******************************************************************************/
void NFA_CheckDeviceResetStatus (BOOLEAN status)
{
    tNFA_DM_CHECK_STATUS *p_msg;

    NFA_TRACE_API1 ("NFA_CheckDeviceResetStatus (status=%X)", status);

    if ((p_msg = (tNFA_DM_API_DISABLE *) GKI_getbuf (sizeof (tNFA_DM_API_DISABLE))) != NULL)
    {
        p_msg->hdr.event = NFA_DM_CHECK_DEVICE_RESET_STATUS;
        p_msg->status  = status;

        nfa_sys_sendmsg (p_msg);

        return;
    }

    return;
}
/*******************************************************************************
**
** Function         NFA_Disable
**
** Description      This function is called to shutdown NFC. The tasks for NFC
**                  are terminated, and clean up routines are performed. This
**                  function is typically called during platform shut-down, or
**                  when NFC is disabled from a settings UI. When the NFC
**                  shutdown procedure is completed, an NFA_DM_DISABLE_EVT is
**                  returned to the application using the tNFA_DM_CBACK.
**
**                  The platform should wait until the NFC_DISABLE_REVT is
**                  received before powering down the NFC chip and NCI transport.
**                  This is required to so that NFA can gracefully shut down any
**                  open connections.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_Disable (BOOLEAN graceful)
{
    tNFA_DM_API_DISABLE *p_msg;

    NFA_TRACE_API1 ("NFA_Disable (graceful=%i)", graceful);

    if ((p_msg = (tNFA_DM_API_DISABLE *) GKI_getbuf (sizeof (tNFA_DM_API_DISABLE))) != NULL)
    {
        p_msg->hdr.event = NFA_DM_API_DISABLE_EVT;
        p_msg->graceful  = graceful;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_SetConfig
**
** Description      Set the configuration parameters to NFCC. The result is
**                  reported with an NFA_DM_SET_CONFIG_EVT in the tNFA_DM_CBACK
**                  callback.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function. Most Configuration
**                  parameters are related to RF discovery.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_BUSY if previous setting is on-going
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_SetConfig (tNFA_PMID param_id,
                           UINT8     length,
                           UINT8    *p_data)
{
    tNFA_DM_API_SET_CONFIG *p_msg;

    NFA_TRACE_API1 ("NFA_SetConfig (): param_id:0x%X", param_id);

    if ((p_msg = (tNFA_DM_API_SET_CONFIG *) GKI_getbuf ((UINT16) (sizeof (tNFA_DM_API_SET_CONFIG) + length))) != NULL)
    {
        p_msg->hdr.event = NFA_DM_API_SET_CONFIG_EVT;

        p_msg->param_id = param_id;
        p_msg->length   = length;
        p_msg->p_data   = (UINT8 *) (p_msg + 1);

        /* Copy parameter data */
        memcpy (p_msg->p_data, p_data, length);

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_GetConfig
**
** Description      Get the configuration parameters from NFCC. The result is
**                  reported with an NFA_DM_GET_CONFIG_EVT in the tNFA_DM_CBACK
**                  callback.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_GetConfig (UINT8 num_ids,
                           tNFA_PMID *p_param_ids)
{
    tNFA_DM_API_GET_CONFIG *p_msg;

    NFA_TRACE_API1 ("NFA_GetConfig (): num_ids: %i", num_ids);


    if ((p_msg = (tNFA_DM_API_GET_CONFIG *) GKI_getbuf ((UINT16) (sizeof (tNFA_DM_API_GET_CONFIG) + num_ids))) != NULL)
    {
        p_msg->hdr.event = NFA_DM_API_GET_CONFIG_EVT;

        p_msg->num_ids = num_ids;
        p_msg->p_pmids = (tNFA_PMID *) (p_msg+1);

        /* Copy the param IDs */
        memcpy (p_msg->p_pmids, p_param_ids, num_ids);

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_GetRouting
**
** Description      Get the Listen Mode Routing Table Entries from NFCC. The result is
**                  reported with an NFA_DM_GET_ROUTING_EVT in the tNFA_DM_CBACK
**                  callback.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_GetRouting(void)
{
  tNFA_DM_API_GET_ROUTING *p_msg;

  if ((p_msg = (tNFA_DM_API_GET_ROUTING *)GKI_getbuf((UINT16)(sizeof (tNFA_DM_API_GET_ROUTING)))) != NULL)
  {
    p_msg->hdr.event = NFA_DM_API_GET_ROUTING_EVT;

    nfa_sys_sendmsg(p_msg);

    return (NFA_STATUS_OK);
  }

  return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_RequestExclusiveRfControl
**
** Description      Request exclusive control of NFC.
**                  - Previous behavior (polling/tag reading, DH card emulation)
**                    will be suspended .
**                  - Polling and listening will be done based on the specified
**                    params
**
**                  The NFA_EXCLUSIVE_RF_CONTROL_STARTED_EVT event of
**                  tNFA_CONN_CBACK indicates the status of the operation.
**
**                  NFA_ACTIVATED_EVT and NFA_DEACTIVATED_EVT indicates link
**                  activation/deactivation.
**
**                  NFA_SendRawFrame is used to send data to the peer. NFA_DATA_EVT
**                  indicates data from the peer.
**
**                  If a tag is activated, then the NFA_RW APIs may be used to
**                  send commands to the tag. Incoming NDEF messages are sent to
**                  the NDEF callback.
**
**                  Once exclusive RF control has started, NFA will not activate
**                  LLCP internally. The application has exclusive control of
**                  the link.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_RequestExclusiveRfControl  (tNFA_TECHNOLOGY_MASK poll_mask,
                                            tNFA_LISTEN_CFG      *p_listen_cfg,
                                            tNFA_CONN_CBACK      *p_conn_cback,
                                            tNFA_NDEF_CBACK      *p_ndef_cback)
{
    tNFA_DM_API_REQ_EXCL_RF_CTRL *p_msg;

    NFA_TRACE_API1 ("NFA_RequestExclusiveRfControl () poll_mask=0x%x", poll_mask);

    if (!p_conn_cback)
    {
        NFA_TRACE_ERROR0 ("NFA_RequestExclusiveRfControl (): error null callback");
        return (NFA_STATUS_FAILED);
    }

    if ((p_msg = (tNFA_DM_API_REQ_EXCL_RF_CTRL *) GKI_getbuf (sizeof (tNFA_DM_API_REQ_EXCL_RF_CTRL))) != NULL)
    {
        p_msg->hdr.event = NFA_DM_API_REQUEST_EXCL_RF_CTRL_EVT;
        p_msg->poll_mask    = poll_mask;
        p_msg->p_conn_cback = p_conn_cback;
        p_msg->p_ndef_cback = p_ndef_cback;

        if (p_listen_cfg)
            memcpy (&p_msg->listen_cfg, p_listen_cfg, sizeof (tNFA_LISTEN_CFG));
        else
            memset (&p_msg->listen_cfg, 0x00, sizeof (tNFA_LISTEN_CFG));

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_ReleaseExclusiveRfControl
**
** Description      Release exclusive control of NFC. Once released, behavior
**                  prior to obtaining exclusive RF control will resume.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_ReleaseExclusiveRfControl (void)
{
    BT_HDR *p_msg;

    NFA_TRACE_API0 ("NFA_ReleaseExclusiveRfControl ()");

    if (!nfa_dm_cb.p_excl_conn_cback)
    {
        NFA_TRACE_ERROR0 ("NFA_ReleaseExclusiveRfControl (): Exclusive rf control is not in progress");
        return (NFA_STATUS_FAILED);
    }

    if ((p_msg = (BT_HDR *) GKI_getbuf (sizeof (BT_HDR))) != NULL)
    {
        p_msg->event = NFA_DM_API_RELEASE_EXCL_RF_CTRL_EVT;
        nfa_sys_sendmsg (p_msg);
        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}


/*******************************************************************************
**
** Function         NFA_EnablePolling
**
** Description      Enable polling for technologies specified by poll_mask.
**
**                  The following events (notified using the connection
**                  callback registered with NFA_Enable) are generated during
**                  polling:
**
**                  - NFA_POLL_ENABLED_EVT indicates whether or not polling
**                    successfully enabled.
**                  - NFA_DISC_RESULT_EVT indicates there are more than one devices,
**                    so application must select one of tags by calling NFA_Select().
**                  - NFA_SELECT_RESULT_EVT indicates whether previous selection was
**                    successful or not. If it was failed then application must select
**                    again or deactivate by calling NFA_Deactivate().
**                  - NFA_ACTIVATED_EVT is generated when an NFC link is activated.
**                  - NFA_NDEF_DETECT_EVT is generated if tag is activated
**                  - NFA_LLCP_ACTIVATED_EVT/NFA_LLCP_DEACTIVATED_EVT is generated
**                    if NFC-DEP is activated
**                  - NFA_DEACTIVATED_EVT will be returned after deactivating NFC link.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_EnablePolling (tNFA_TECHNOLOGY_MASK poll_mask)
{
    tNFA_DM_API_ENABLE_POLL *p_msg;

    NFA_TRACE_API1 ("NFA_EnablePolling () 0x%X", poll_mask);

    if ((p_msg = (tNFA_DM_API_ENABLE_POLL *) GKI_getbuf (sizeof (tNFA_DM_API_ENABLE_POLL))) != NULL)
    {
        p_msg->hdr.event = NFA_DM_API_ENABLE_POLLING_EVT;
        p_msg->poll_mask = poll_mask;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_DisablePolling
**
** Description      Disable polling
**                  NFA_POLL_DISABLED_EVT will be returned after stopping polling.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_DisablePolling (void)
{
    BT_HDR *p_msg;

    NFA_TRACE_API0 ("NFA_DisablePolling ()");

    if ((p_msg = (BT_HDR *) GKI_getbuf (sizeof (BT_HDR))) != NULL)
    {
        p_msg->event = NFA_DM_API_DISABLE_POLLING_EVT;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_EnableListening
**
** Description      Enable listening.
**                  NFA_LISTEN_ENABLED_EVT will be returned after listening is allowed.
**
**                  The actual listening technologies are specified by other NFA
**                  API functions. Such functions include (but not limited to)
**                  NFA_CeConfigureUiccListenTech.
**                  If NFA_DisableListening () is called to ignore the listening technologies,
**                  NFA_EnableListening () is called to restore the listening technologies
**                  set by these functions.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_EnableListening (void)
{
    BT_HDR *p_msg;

    NFA_TRACE_API0 ("NFA_EnableListening ()");

    if ((p_msg = (BT_HDR *) GKI_getbuf (sizeof (BT_HDR))) != NULL)
    {
        p_msg->event = NFA_DM_API_ENABLE_LISTENING_EVT;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_DisableListening
**
** Description      Disable listening
**                  NFA_LISTEN_DISABLED_EVT will be returned after stopping listening.
**                  This function is called to exclude listen at RF discovery.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_DisableListening (void)
{
    BT_HDR *p_msg;

    NFA_TRACE_API0 ("NFA_DisableListening ()");

    if ((p_msg = (BT_HDR *) GKI_getbuf (sizeof (BT_HDR))) != NULL)
    {
        p_msg->event = NFA_DM_API_DISABLE_LISTENING_EVT;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_PauseP2p
**
** Description      Pause P2P services.
**                  NFA_P2P_PAUSED_EVT will be returned after P2P services are
**                  disabled.
**
**                  The P2P services enabled by NFA_P2p* API functions are not
**                  available. NFA_ResumeP2p() is called to resume the P2P
**                  services.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_PauseP2p (void)
{
    BT_HDR *p_msg;

    NFA_TRACE_API0 ("NFA_PauseP2p ()");

    if ((p_msg = (BT_HDR *) GKI_getbuf (sizeof (BT_HDR))) != NULL)
    {
        p_msg->event = NFA_DM_API_PAUSE_P2P_EVT;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_ResumeP2p
**
** Description      Resume P2P services.
**                  NFA_P2P_RESUMED_EVT will be returned after P2P services are.
**                  enables again.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_ResumeP2p (void)
{
    BT_HDR *p_msg;

    NFA_TRACE_API0 ("NFA_ResumeP2p ()");

    if ((p_msg = (BT_HDR *) GKI_getbuf (sizeof (BT_HDR))) != NULL)
    {
        p_msg->event = NFA_DM_API_RESUME_P2P_EVT;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_SetP2pListenTech
**
** Description      This function is called to set listen technology for NFC-DEP.
**                  This funtion may be called before or after starting any server
**                  on NFA P2P/CHO/SNEP.
**                  If there is no technology for NFC-DEP, P2P listening will be
**                  stopped.
**
**                  NFA_SET_P2P_LISTEN_TECH_EVT without data will be returned.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_SetP2pListenTech (tNFA_TECHNOLOGY_MASK tech_mask)
{
    tNFA_DM_API_SET_P2P_LISTEN_TECH *p_msg;

    NFA_TRACE_API1 ("NFA_P2pSetListenTech (): tech_mask:0x%X", tech_mask);

    if ((p_msg = (tNFA_DM_API_SET_P2P_LISTEN_TECH *) GKI_getbuf (sizeof (tNFA_DM_API_SET_P2P_LISTEN_TECH))) != NULL)
    {
        p_msg->hdr.event = NFA_DM_API_SET_P2P_LISTEN_TECH_EVT;
        p_msg->tech_mask = tech_mask;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_StartRfDiscovery
**
** Description      Start RF discovery
**                  RF discovery parameters shall be set by other APIs.
**
**                  An NFA_RF_DISCOVERY_STARTED_EVT indicates whether starting was successful or not.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_StartRfDiscovery (void)
{
    BT_HDR *p_msg;

    NFA_TRACE_API0 ("NFA_StartRfDiscovery ()");

    if ((p_msg = (BT_HDR *) GKI_getbuf (sizeof (BT_HDR))) != NULL)
    {
        p_msg->event = NFA_DM_API_START_RF_DISCOVERY_EVT;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_StopRfDiscovery
**
** Description      Stop RF discovery
**
**                  An NFA_RF_DISCOVERY_STOPPED_EVT indicates whether stopping was successful or not.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_StopRfDiscovery (void)
{
    BT_HDR *p_msg;

    NFA_TRACE_API0 ("NFA_StopRfDiscovery ()");

    if ((p_msg = (BT_HDR *) GKI_getbuf (sizeof (BT_HDR))) != NULL)
    {
        p_msg->event = NFA_DM_API_STOP_RF_DISCOVERY_EVT;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_SetRfDiscoveryDuration
**
** Description      Set the duration of the single discovery period in [ms].
**                  Allowable range: 0 ms to 0xFFFF ms.
**
**                  If discovery is already started, the application should
**                  call NFA_StopRfDiscovery prior to calling
**                  NFA_SetRfDiscoveryDuration, and then call
**                  NFA_StartRfDiscovery afterwards to restart discovery using
**                  the new duration.
**
** Note:            If RF discovery is started, NFA_StopRfDiscovery()/NFA_RF_DISCOVERY_STOPPED_EVT
**                  should happen before calling this function
**
** Returns:
**                  NFA_STATUS_OK, if command accepted
**                  NFA_STATUS_FAILED: otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_SetRfDiscoveryDuration (UINT16 discovery_period_ms)
{
    tNFA_DM_API_SET_RF_DISC_DUR *p_msg;

    NFA_TRACE_API0 ("NFA_SetRfDiscoveryDuration ()");

    /* Post the API message */
    if ((p_msg = (tNFA_DM_API_SET_RF_DISC_DUR *) GKI_getbuf (sizeof (BT_HDR))) != NULL)
    {
        p_msg->hdr.event = NFA_DM_API_SET_RF_DISC_DURATION_EVT;

        /* Set discovery duration */
        p_msg->rf_disc_dur_ms = discovery_period_ms;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_Select
**
** Description      Select one from detected devices during discovery
**                  (from NFA_DISC_RESULT_EVTs). The application should wait for
**                  the final NFA_DISC_RESULT_EVT before selecting.
**
**                  An NFA_SELECT_RESULT_EVT indicates whether selection was successful or not.
**                  If failed then application must select again or deactivate by NFA_Deactivate().
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_INVALID_PARAM if RF interface is not matched protocol
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_Select (UINT8             rf_disc_id,
                        tNFA_NFC_PROTOCOL protocol,
                        tNFA_INTF_TYPE    rf_interface)
{
    tNFA_DM_API_SELECT *p_msg;

    NFA_TRACE_API3 ("NFA_Select (): rf_disc_id:0x%X, protocol:0x%X, rf_interface:0x%X",
                    rf_disc_id, protocol, rf_interface);

    if (  ((rf_interface == NFA_INTERFACE_ISO_DEP) && (protocol != NFA_PROTOCOL_ISO_DEP))
        ||((rf_interface == NFA_INTERFACE_NFC_DEP) && (protocol != NFA_PROTOCOL_NFC_DEP))  )
    {
        NFA_TRACE_ERROR0 ("NFA_Select (): RF interface is not matched protocol");
        return (NFA_STATUS_INVALID_PARAM);
    }

    if ((p_msg = (tNFA_DM_API_SELECT *) GKI_getbuf ((UINT16) (sizeof (tNFA_DM_API_SELECT)))) != NULL)
    {
        p_msg->hdr.event     = NFA_DM_API_SELECT_EVT;
        p_msg->rf_disc_id    = rf_disc_id;
        p_msg->protocol      = protocol;
        p_msg->rf_interface  = rf_interface;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_UpdateRFCommParams
**
** Description      This function is called to update RF Communication parameters
**                  once the Frame RF Interface has been activated.
**
**                  An NFA_UPDATE_RF_PARAM_RESULT_EVT indicates whether updating
**                  was successful or not.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_UpdateRFCommParams (tNFA_RF_COMM_PARAMS *p_params)
{
    tNFA_DM_API_UPDATE_RF_PARAMS *p_msg;

    NFA_TRACE_API0 ("NFA_UpdateRFCommParams ()");

    if ((p_msg = (tNFA_DM_API_UPDATE_RF_PARAMS *) GKI_getbuf ((UINT16) (sizeof (tNFA_DM_API_UPDATE_RF_PARAMS)))) != NULL)
    {
        p_msg->hdr.event     = NFA_DM_API_UPDATE_RF_PARAMS_EVT;
        memcpy (&p_msg->params, p_params, sizeof (tNFA_RF_COMM_PARAMS));

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_Deactivate
**
** Description
**                  If sleep_mode=TRUE:
**                      Deselect the activated device by deactivating into sleep mode.
**
**                      An NFA_DEACTIVATE_FAIL_EVT indicates that selection was not successful.
**                      Application can select another discovered device or deactivate by NFA_Deactivate ()
**                      after receiving NFA_DEACTIVATED_EVT.
**
**                      Deactivating to sleep mode is not allowed when NFCC is in wait-for-host-select
**                      mode, or in listen-sleep states; NFA will deactivate to idle or discovery state
**                      for these cases respectively.
**
**
**                  If sleep_mode=FALSE:
**                      Deactivate the connection (e.g. as a result of presence check failure)
**                      NFA_DEACTIVATED_EVT will indicate that link is deactivated.
**                      Polling/listening will resume (unless the nfcc is in wait_for-all-discoveries state)
**
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_Deactivate (BOOLEAN sleep_mode)
{
    tNFA_DM_API_DEACTIVATE *p_msg;

    NFA_TRACE_API1 ("NFA_Deactivate (): sleep_mode:%i", sleep_mode);

    if ((p_msg = (tNFA_DM_API_DEACTIVATE *) GKI_getbuf ((UINT16) (sizeof (tNFA_DM_API_DEACTIVATE)))) != NULL)
    {
        p_msg->hdr.event    = NFA_DM_API_DEACTIVATE_EVT;
        p_msg->sleep_mode   = sleep_mode;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_SendRawFrame
**
** Description      Send a raw frame over the activated interface with the NFCC.
**                  This function can only be called after NFC link is activated.
**
**                  If the activated interface is a tag and auto-presence check is
**                  enabled then presence_check_start_delay can be used to indicate
**                  the delay in msec after which the next auto presence check
**                  command can be sent. NFA_DM_DEFAULT_PRESENCE_CHECK_START_DELAY
**                  can be used as the default value for the delay.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_SendRawFrame (UINT8  *p_raw_data,
                              UINT16  data_len,
                              UINT16  presence_check_start_delay)
{
    BT_HDR *p_msg;
    UINT16  size;
    UINT8  *p;

    NFA_TRACE_API1 ("NFA_SendRawFrame () data_len:%d", data_len);

    /* Validate parameters */
    if ((data_len == 0) || (p_raw_data == NULL))
        return (NFA_STATUS_INVALID_PARAM);

    size = BT_HDR_SIZE + NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE + data_len;
    if ((p_msg = (BT_HDR *) GKI_getbuf (size)) != NULL)
    {
        p_msg->event  = NFA_DM_API_RAW_FRAME_EVT;
        p_msg->layer_specific = presence_check_start_delay;
        p_msg->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE;
        p_msg->len    = data_len;

        p = (UINT8 *) (p_msg + 1) + p_msg->offset;
        memcpy (p, p_raw_data, data_len);

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
** NDEF Handler APIs
*******************************************************************************/

/*******************************************************************************
**
** Function         NFA_RegisterNDefTypeHandler
**
** Description      This function allows the applications to register for
**                  specific types of NDEF records. When NDEF records are
**                  received, NFA will parse the record-type field, and pass
**                  the record to the registered tNFA_NDEF_CBACK.
**
**                  For records types which were not registered, the record will
**                  be sent to the default handler. A default type-handler may
**                  be registered by calling this NFA_RegisterNDefTypeHandler
**                  with tnf=NFA_TNF_DEFAULT. In this case, all un-registered
**                  record types will be sent to the callback. Only one default
**                  handler may be registered at a time.
**
**                  An NFA_NDEF_REGISTER_EVT will be sent to the tNFA_NDEF_CBACK
**                  to indicate that registration was successful, and provide a
**                  handle for this record type.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_RegisterNDefTypeHandler (BOOLEAN         handle_whole_message,
                                         tNFA_TNF        tnf,
                                         UINT8           *p_type_name,
                                         UINT8           type_name_len,
                                         tNFA_NDEF_CBACK *p_ndef_cback)
{
    tNFA_DM_API_REG_NDEF_HDLR *p_msg;

    NFA_TRACE_API2 ("NFA_RegisterNDefTypeHandler (): handle whole ndef message: %i, tnf=0x%02x", handle_whole_message, tnf);

    /* Check for NULL callback */
    if (!p_ndef_cback)
    {
        NFA_TRACE_ERROR0 ("NFA_RegisterNDefTypeHandler (): error - null callback");
        return (NFA_STATUS_INVALID_PARAM);
    }


    if ((p_msg = (tNFA_DM_API_REG_NDEF_HDLR *) GKI_getbuf ((UINT16) (sizeof (tNFA_DM_API_REG_NDEF_HDLR) + type_name_len))) != NULL)
    {
        p_msg->hdr.event = NFA_DM_API_REG_NDEF_HDLR_EVT;

        p_msg->flags = (handle_whole_message ? NFA_NDEF_FLAGS_HANDLE_WHOLE_MESSAGE : 0);
        p_msg->tnf = tnf;
        p_msg->name_len = type_name_len;
        p_msg->p_ndef_cback = p_ndef_cback;
        memcpy (p_msg->name, p_type_name, type_name_len);

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_RegisterNDefUriHandler
**
** Description      This API is a special-case of NFA_RegisterNDefTypeHandler
**                  with TNF=NFA_TNF_WKT, and type_name='U' (URI record); and allows
**                  registering for specific URI types (e.g. 'tel:' or 'mailto:').
**
**                  An NFA_NDEF_REGISTER_EVT will be sent to the tNFA_NDEF_CBACK
**                  to indicate that registration was successful, and provide a
**                  handle for this registration.
**
**                  If uri_id=NFA_NDEF_URI_ID_ABSOLUTE, then p_abs_uri contains the
**                  unabridged URI. For all other uri_id values, the p_abs_uri
**                  parameter is ignored (i.e the URI prefix is implied by uri_id).
**                  See [NFC RTD URI] for more information.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_RegisterNDefUriHandler (BOOLEAN          handle_whole_message,
                                                       tNFA_NDEF_URI_ID uri_id,
                                                       UINT8            *p_abs_uri,
                                                       UINT8            uri_id_len,
                                                       tNFA_NDEF_CBACK  *p_ndef_cback)
{
    tNFA_DM_API_REG_NDEF_HDLR *p_msg;

    NFA_TRACE_API2 ("NFA_RegisterNDefUriHandler (): handle whole ndef message: %i, uri_id=0x%02x", handle_whole_message, uri_id);

    /* Check for NULL callback */
    if (!p_ndef_cback)
    {
        NFA_TRACE_ERROR0 ("NFA_RegisterNDefUriHandler (): error - null callback");
        return (NFA_STATUS_INVALID_PARAM);
    }


    if ((p_msg = (tNFA_DM_API_REG_NDEF_HDLR *) GKI_getbuf ((UINT16) (sizeof (tNFA_DM_API_REG_NDEF_HDLR) + uri_id_len))) != NULL)
    {
        p_msg->hdr.event = NFA_DM_API_REG_NDEF_HDLR_EVT;

        p_msg->flags = NFA_NDEF_FLAGS_WKT_URI;

        if (handle_whole_message)
        {
            p_msg->flags |= NFA_NDEF_FLAGS_HANDLE_WHOLE_MESSAGE;
        }

        /* abs_uri is only valid fir uri_id=NFA_NDEF_URI_ID_ABSOLUTE */
        if (uri_id != NFA_NDEF_URI_ID_ABSOLUTE)
        {
            uri_id_len = 0;
        }

        p_msg->tnf = NFA_TNF_WKT;
        p_msg->uri_id = uri_id;
        p_msg->name_len = uri_id_len;
        p_msg->p_ndef_cback = p_ndef_cback;
        memcpy (p_msg->name, p_abs_uri, uri_id_len);

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_DeregisterNDefTypeHandler
**
** Description      Deregister NDEF record type handler.
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
NFC_API extern tNFA_STATUS NFA_DeregisterNDefTypeHandler (tNFA_HANDLE ndef_type_handle)
{
    tNFA_DM_API_DEREG_NDEF_HDLR *p_msg;

    NFA_TRACE_API1 ("NFA_DeregisterNDefHandler (): handle 0x%08x", ndef_type_handle);


    if ((p_msg = (tNFA_DM_API_DEREG_NDEF_HDLR *) GKI_getbuf ((UINT16) (sizeof (tNFA_DM_API_DEREG_NDEF_HDLR)))) != NULL)
    {
        p_msg->hdr.event = NFA_DM_API_DEREG_NDEF_HDLR_EVT;
        p_msg->ndef_type_handle = ndef_type_handle;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_PowerOffSleepMode
**
** Description      This function is called to enter or leave NFCC Power Off Sleep mode
**                  NFA_DM_PWR_MODE_CHANGE_EVT will be sent to indicate status.
**
**                  start_stop : TRUE if entering Power Off Sleep mode
**                               FALSE if leaving Power Off Sleep mode
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_PowerOffSleepMode (BOOLEAN start_stop)
{
    BT_HDR *p_msg;

    NFA_TRACE_API1 ("NFA_PowerOffSleepState () start_stop=%d", start_stop);

    if (nfa_dm_cb.flags & NFA_DM_FLAGS_SETTING_PWR_MODE)
    {
        NFA_TRACE_ERROR0 ("NFA_PowerOffSleepState (): NFA DM is busy to update power mode");
        return (NFA_STATUS_FAILED);
    }
    else
    {
        nfa_dm_cb.flags |= NFA_DM_FLAGS_SETTING_PWR_MODE;
    }

    if ((p_msg = (BT_HDR *) GKI_getbuf (sizeof (BT_HDR))) != NULL)
    {
        p_msg->event          = NFA_DM_API_POWER_OFF_SLEEP_EVT;
        p_msg->layer_specific = start_stop;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_RegVSCback
**
** Description      This function is called to register or de-register a callback
**                  function to receive Proprietary NCI response and notification
**                  events.
**                  The maximum number of callback functions allowed is NFC_NUM_VS_CBACKS
**
** Returns          tNFC_STATUS
**
*******************************************************************************/
tNFC_STATUS NFA_RegVSCback (BOOLEAN          is_register,
                            tNFA_VSC_CBACK   *p_cback)
{
    tNFA_DM_API_REG_VSC *p_msg;

    NFA_TRACE_API1 ("NFA_RegVSCback() is_register=%d", is_register);

    if (p_cback == NULL)
    {
        NFA_TRACE_ERROR0 ("NFA_RegVSCback() requires a valid callback function");
        return (NFA_STATUS_FAILED);
    }

    if ((p_msg = (tNFA_DM_API_REG_VSC *) GKI_getbuf (sizeof(tNFA_DM_API_REG_VSC))) != NULL)
    {
        p_msg->hdr.event        = NFA_DM_API_REG_VSC_EVT;
        p_msg->is_register      = is_register;
        p_msg->p_cback          = p_cback;

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_SendVsCommand
**
** Description      This function is called to send an NCI Vendor Specific
**                  command to NFCC.
**
**                  oid             - The opcode of the VS command.
**                  cmd_params_len  - The command parameter len
**                  p_cmd_params    - The command parameter
**                  p_cback         - The callback function to receive the command
**                                    status
**
** Returns          NFA_STATUS_OK if successfully initiated
**                  NFA_STATUS_FAILED otherwise
**
*******************************************************************************/
tNFA_STATUS NFA_SendVsCommand (UINT8            oid,
                               UINT16            cmd_params_len,
                               UINT8            *p_cmd_params,
                               tNFA_VSC_CBACK    *p_cback)
{
    tNFA_DM_API_SEND_VSC *p_msg;
    UINT16  size = sizeof(tNFA_DM_API_SEND_VSC) + cmd_params_len;

    NFA_TRACE_API1 ("NFA_SendVsCommand() oid=0x%x", oid);

    if ((p_msg = (tNFA_DM_API_SEND_VSC *) GKI_getbuf (size)) != NULL)
    {
        p_msg->hdr.event        = NFA_DM_API_SEND_VSC_EVT;
        p_msg->oid              = oid;
        p_msg->p_cback          = p_cback;
        if (cmd_params_len && p_cmd_params)
        {
            p_msg->cmd_params_len   = cmd_params_len;
            p_msg->p_cmd_params     = (UINT8 *)(p_msg + 1);
            memcpy (p_msg->p_cmd_params, p_cmd_params, cmd_params_len);
        }
        else
        {
            p_msg->cmd_params_len   = 0;
            p_msg->p_cmd_params     = NULL;
        }

        nfa_sys_sendmsg (p_msg);

        return (NFA_STATUS_OK);
    }

    return (NFA_STATUS_FAILED);
}

/*******************************************************************************
**
** Function         NFA_SetTraceLevel
**
** Description      This function sets the trace level for NFA.  If called with
**                  a value of 0xFF, it simply returns the current trace level.
**
** Returns          The new or current trace level
**
*******************************************************************************/
UINT8 NFA_SetTraceLevel (UINT8 new_level)
{
    if (new_level != 0xFF)
        nfa_sys_set_trace_level (new_level);

    return (nfa_sys_cb.trace_level);
}
