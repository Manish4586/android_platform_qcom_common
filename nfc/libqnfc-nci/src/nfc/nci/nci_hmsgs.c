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
 *  This file contains function of the NCI unit to format and send NCI
 *  commands (for DH).
 *
 ******************************************************************************/
#include <string.h>
#include "nfc_target.h"

#if NFC_INCLUDED == TRUE
#include "nci_defs.h"
#include "nci_hmsgs.h"
#include "nfc_api.h"
#include "nfc_int.h"

#ifdef DTA // <DTA>
#include "dta_flag.h"
#endif // </DTA>

/*******************************************************************************
**
** Function         nci_snd_core_reset
**
** Description      compose and send CORE RESET command to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_core_reset (UINT8 reset_type)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = NCI_GET_CMD_BUF (NCI_CORE_PARAM_SIZE_RESET)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->len              = NCI_MSG_HDR_SIZE + NCI_CORE_PARAM_SIZE_RESET;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_CORE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_CORE_RESET);
    UINT8_TO_STREAM (pp, NCI_CORE_PARAM_SIZE_RESET);
    UINT8_TO_STREAM (pp, reset_type);

    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}

/*******************************************************************************
**
** Function         nci_snd_core_init
**
** Description      compose and send CORE INIT command to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_core_init (void)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = NCI_GET_CMD_BUF (NCI_CORE_PARAM_SIZE_INIT)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->len              = NCI_MSG_HDR_SIZE + NCI_CORE_PARAM_SIZE_INIT;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_CORE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_CORE_INIT);
    UINT8_TO_STREAM (pp, NCI_CORE_PARAM_SIZE_INIT);

    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}

/*******************************************************************************
**
** Function         nci_snd_core_get_config
**
** Description      compose and send CORE GET_CONFIG command to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_core_get_config (UINT8 *param_ids, UINT8 num_ids)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = NCI_GET_CMD_BUF (num_ids)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->len              = NCI_MSG_HDR_SIZE + num_ids + 1;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_CORE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_CORE_GET_CONFIG);
    UINT8_TO_STREAM (pp, (UINT8) (num_ids + 1));
    UINT8_TO_STREAM (pp, num_ids);
    ARRAY_TO_STREAM (pp, param_ids, num_ids);

    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}

/*******************************************************************************
**
** Function         nci_snd_core_set_config
**
** Description      compose and send CORE SET_CONFIG command to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_core_set_config (UINT8 *p_param_tlvs, UINT8 tlv_size)
{
    BT_HDR *p;
    UINT8 *pp;
    UINT8  num = 0, ulen, len, *pt;

    if ((p = NCI_GET_CMD_BUF (tlv_size + 1)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event    = BT_EVT_TO_NFC_NCI;
    p->len      = NCI_MSG_HDR_SIZE + tlv_size + 1;
    p->offset   = NCI_MSG_OFFSET_SIZE;
    pp          = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_CORE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_CORE_SET_CONFIG);
    UINT8_TO_STREAM (pp, (UINT8) (tlv_size + 1));
    len         = tlv_size;
    pt          = p_param_tlvs;
    while (len > 1)
    {
        len     -= 2;
        pt++;
        num++;
        ulen     = *pt++;
        pt      += ulen;
        if (len >= ulen)
        {
            len -= ulen;
        }
        else
        {
            GKI_freebuf (p);
            return NCI_STATUS_FAILED;
        }
    }

    UINT8_TO_STREAM (pp, num);
    ARRAY_TO_STREAM (pp, p_param_tlvs, tlv_size);
    nfc_ncif_send_cmd (p);

    return (NCI_STATUS_OK);
}

/*******************************************************************************
**
** Function         nci_snd_core_conn_create
**
** Description      compose and send CORE CONN_CREATE command to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_core_conn_create (UINT8 dest_type, UINT8 num_tlv, UINT8 tlv_size, UINT8 *p_param_tlvs)
{
    BT_HDR *p;
    UINT8 *pp;
    UINT8 size = NCI_CORE_PARAM_SIZE_CON_CREATE+tlv_size;

    if ((p = NCI_GET_CMD_BUF (size)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->len              = NCI_MSG_HDR_SIZE + NCI_CORE_PARAM_SIZE_CON_CREATE;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_CORE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_CORE_CONN_CREATE);
    UINT8_TO_STREAM (pp, size);
    UINT8_TO_STREAM (pp, dest_type);
    UINT8_TO_STREAM (pp, num_tlv);
    if (tlv_size)
    {
        ARRAY_TO_STREAM (pp, p_param_tlvs, tlv_size);
        p->len         += tlv_size;
    }

    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}

/*******************************************************************************
**
** Function         nci_snd_core_conn_close
**
** Description      compose and send CORE CONN_CLOSE command to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_core_conn_close (UINT8 conn_id)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = NCI_GET_CMD_BUF (NCI_CORE_PARAM_SIZE_CON_CLOSE)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->len              = NCI_MSG_HDR_SIZE + NCI_CORE_PARAM_SIZE_CON_CLOSE;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_CORE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_CORE_CONN_CLOSE);
    UINT8_TO_STREAM (pp, NCI_CORE_PARAM_SIZE_CON_CLOSE);
    UINT8_TO_STREAM (pp, conn_id);

    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}


#if (NFC_NFCEE_INCLUDED == TRUE)
#if (NFC_RW_ONLY == FALSE)
/*******************************************************************************
**
** Function         nci_snd_nfcee_discover
**
** Description      compose and send NFCEE Management NFCEE_DISCOVER command
**                  to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_nfcee_discover (UINT8 discover_action)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = NCI_GET_CMD_BUF (NCI_PARAM_SIZE_DISCOVER_NFCEE)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->len              = NCI_MSG_HDR_SIZE + NCI_PARAM_SIZE_DISCOVER_NFCEE;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_EE_MANAGE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_NFCEE_DISCOVER);
    UINT8_TO_STREAM (pp, NCI_PARAM_SIZE_DISCOVER_NFCEE);
    UINT8_TO_STREAM (pp, discover_action);

    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}

/*******************************************************************************
**
** Function         nci_snd_nfcee_mode_set
**
** Description      compose and send NFCEE Management NFCEE MODE SET command
**                  to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_nfcee_mode_set (UINT8 nfcee_id, UINT8 nfcee_mode)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = NCI_GET_CMD_BUF (NCI_CORE_PARAM_SIZE_NFCEE_MODE_SET)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->len              = NCI_MSG_HDR_SIZE + NCI_CORE_PARAM_SIZE_NFCEE_MODE_SET;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_EE_MANAGE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_NFCEE_MODE_SET);
    UINT8_TO_STREAM (pp, NCI_CORE_PARAM_SIZE_NFCEE_MODE_SET);
    UINT8_TO_STREAM (pp, nfcee_id);
    UINT8_TO_STREAM (pp, nfcee_mode);

    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}
#endif
#endif

/*******************************************************************************
**
** Function         nci_snd_discover_cmd
**
** Description      compose and send RF Management DISCOVER command to command queue
**
** Returns          status
**
*******************************************************************************/
#ifdef DTA // <DTA>
tNCI_DISCOVER_PARAMS p_paramListen[] ={
                                          {0x80,0x01},
                                          {0x81,0x01},
                                          {0x82,0x01}
                                      };

tNCI_DISCOVER_PARAMS p_paramPoll[] ={
                                          {0x00,0x01},
                                          {0x01,0x01},
                                          {0x02,0x01}
                                    };
#endif // </DTA>
UINT8 nci_snd_discover_cmd (UINT8 num, tNCI_DISCOVER_PARAMS *p_param)
{
    BT_HDR *p;
    UINT8 *pp, *p_size, *p_start;
#ifdef DTA // <DTA>
    UINT32 lp_update_flag = 0;
#endif // </DTA>
    int xx;
    int size;
#ifdef DTA // <DTA>
    if(in_dta_mode() ) {
        GetNumValue("DISC_LP_ENABLE_TEST", &lp_update_flag, sizeof(lp_update_flag));
        if (lp_update_flag == 1 || lp_update_flag ==2 ) // LISTEN or POLL ONLY
        {
            num = 0x03;
        }
    }
#endif // </DTA>

    size   = num * sizeof (tNCI_DISCOVER_PARAMS) + 1;
    if ((p = NCI_GET_CMD_BUF (size)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_RF_MANAGE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_RF_DISCOVER);
    p_size  = pp;
    pp++;
    p_start = pp;
    UINT8_TO_STREAM (pp, num);
    for (xx=0; xx<num; xx++)
    {
#ifdef DTA // <DTA>
        if(in_dta_mode() ) {
            if (lp_update_flag == 1) //LISTEN ONLY
            {
                NFC_TRACE_DEBUG0("DISC_LP_ENABLE_TEST : LISTEN ONLY MODE");
                UINT8_TO_STREAM (pp, p_paramListen[xx].type);
                UINT8_TO_STREAM (pp, p_paramListen[xx].frequency);
            }
            else if(lp_update_flag == 2) //POLL ONLY
            {
                NFC_TRACE_DEBUG0("DISC_LP_ENABLE_TEST : POLL ONLY MODE");
                UINT8_TO_STREAM (pp, p_paramPoll[xx].type);
                UINT8_TO_STREAM (pp, p_paramPoll[xx].frequency);
            } else {
                UINT8_TO_STREAM (pp, p_param[xx].type);
                UINT8_TO_STREAM (pp, p_param[xx].frequency);
            }
        } else { // !(in_dta_mode() )
#endif // </DTA>
        UINT8_TO_STREAM (pp, p_param[xx].type);
        UINT8_TO_STREAM (pp, p_param[xx].frequency);
#ifdef DTA // <DTA>
        }
#endif // </DTA>
    }
    *p_size = (UINT8) (pp - p_start);
    p->len  = NCI_MSG_HDR_SIZE + *p_size;

    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}

/*******************************************************************************
**
** Function         nci_snd_discover_select_cmd
**
** Description      compose and send RF Management DISCOVER SELECT command
**                  to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_discover_select_cmd (UINT8 rf_disc_id, UINT8 protocol, UINT8 rf_interface)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = NCI_GET_CMD_BUF (NCI_DISCOVER_PARAM_SIZE_SELECT)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->len              = NCI_MSG_HDR_SIZE + NCI_DISCOVER_PARAM_SIZE_SELECT;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_RF_MANAGE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_RF_DISCOVER_SELECT);
    UINT8_TO_STREAM (pp, NCI_DISCOVER_PARAM_SIZE_SELECT);
    UINT8_TO_STREAM (pp, rf_disc_id);
    UINT8_TO_STREAM (pp, protocol);
    UINT8_TO_STREAM (pp, rf_interface);

    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}

/*******************************************************************************
**
** Function         nci_snd_deactivate_cmd
**
** Description      compose and send RF Management DEACTIVATE command
**                  to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_deactivate_cmd (UINT8 de_act_type )
{
    BT_HDR *p;
    UINT8 *pp;

    nfc_cb.reassembly = TRUE;

    if ((p = NCI_GET_CMD_BUF (NCI_DISCOVER_PARAM_SIZE_DEACT)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->len              = NCI_MSG_HDR_SIZE + NCI_DISCOVER_PARAM_SIZE_DEACT;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_RF_MANAGE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_RF_DEACTIVATE);
    UINT8_TO_STREAM (pp, NCI_DISCOVER_PARAM_SIZE_DEACT);
    UINT8_TO_STREAM (pp, de_act_type);

    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}

/*******************************************************************************
**
** Function         nci_snd_discover_map_cmd
**
** Description      compose and send RF Management DISCOVER MAP command
**                  to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_discover_map_cmd (UINT8 num, tNCI_DISCOVER_MAPS *p_maps)
{
    BT_HDR *p;
    UINT8 *pp, *p_size, *p_start;
    int xx;
    int size;

    size = num * sizeof (tNCI_DISCOVER_MAPS) + 1;

    if ((p = NCI_GET_CMD_BUF (size)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_RF_MANAGE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_RF_DISCOVER_MAP);
    p_size  = pp;
    pp++;
    p_start = pp;
    UINT8_TO_STREAM (pp, num);
    for (xx = 0; xx < num; xx++)
    {
        UINT8_TO_STREAM (pp, p_maps[xx].protocol);
        UINT8_TO_STREAM (pp, p_maps[xx].mode);
        UINT8_TO_STREAM (pp, p_maps[xx].intf_type);
    }
    *p_size = (UINT8) (pp - p_start);
    p->len  = NCI_MSG_HDR_SIZE + *p_size;
    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}
/*******************************************************************************
**
** Function         nci_snd_t3t_polling
**
** Description      compose and send RF Management T3T POLLING command
**                  to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_t3t_polling (UINT16 system_code, UINT8 rc, UINT8 tsn)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = NCI_GET_CMD_BUF (NCI_RF_PARAM_SIZE_T3T_POLLING)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->len              = NCI_MSG_HDR_SIZE + NCI_RF_PARAM_SIZE_T3T_POLLING;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_RF_MANAGE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_RF_T3T_POLLING);
    UINT8_TO_STREAM (pp, NCI_RF_PARAM_SIZE_T3T_POLLING);
    UINT16_TO_BE_STREAM (pp, system_code);
    UINT8_TO_STREAM (pp, rc);
    UINT8_TO_STREAM (pp, tsn);

    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}

/*******************************************************************************
**
** Function         nci_snd_parameter_update_cmd
**
** Description      compose and send RF Management RF Communication Parameter
**                  Update commandto command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_parameter_update_cmd (UINT8 *p_param_tlvs, UINT8 tlv_size)
{
    BT_HDR *p;
    UINT8 *pp;
    UINT8  num = 0, ulen, len, *pt;

    if (MAX_RF_TLV < tlv_size)
        return (NCI_STATUS_FAILED);

    if ((p = NCI_GET_CMD_BUF (tlv_size + 1)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event    = BT_EVT_TO_NFC_NCI;
    p->len      = NCI_MSG_HDR_SIZE + tlv_size + 1;
    p->offset   = NCI_MSG_OFFSET_SIZE;
    pp          = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_RF_MANAGE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_RF_PARAMETER_UPDATE);
    UINT8_TO_STREAM (pp, (UINT8) (tlv_size + 1));
    len         = tlv_size;
    pt          = p_param_tlvs;
    while (len > 1)
    {
        len     -= 2;
        pt++;
        num++;
        ulen     = *pt++;
        pt      += ulen;
        if (len >= ulen)
        {
            len -= ulen;
        }
        else
        {
            GKI_freebuf (p);
            return NCI_STATUS_FAILED;
        }
    }

    UINT8_TO_STREAM (pp, num);
    ARRAY_TO_STREAM (pp, p_param_tlvs, tlv_size);
    nfc_ncif_send_cmd (p);

    return (NCI_STATUS_OK);
}

/* QNCI_FEATURE_ISODEP_PRESENCE */
/*******************************************************************************
**
** Function         nci_snd_presence_check_cmd
**
** Description      compose and send RF Management ISO_DEP_NAK_PRESENCE command
**                  to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_presence_check_cmd (void)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = NCI_GET_CMD_BUF (NCI_MSG_HDR_SIZE)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->len              = NCI_MSG_HDR_SIZE;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_RF_MANAGE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_RF_PRESENCE_CHECK);
    UINT8_TO_STREAM (pp, 0x00);

    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}
/* QNCI_FEATURE_ISODEP_PRESENCE */

#if (NFC_NFCEE_INCLUDED == TRUE)
#if (NFC_RW_ONLY == FALSE)
/*******************************************************************************
**
** Function         nci_snd_set_routing_cmd
**
** Description      compose and send RF Management SET_LISTEN_MODE_ROUTING command
**                  to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_set_routing_cmd (BOOLEAN more, UINT8 target_handle, UINT8 num_tlv, UINT8 tlv_size, UINT8 *p_param_tlvs)
{
    BT_HDR *p;
    UINT8 *pp;
    UINT8 size = tlv_size + 2;

    if (tlv_size == 0)
    {
        /* just to terminate routing table
         * 2 bytes (more=FALSE and num routing entries=0) */
        size = 2;
    }

    if ((p = NCI_GET_CMD_BUF(size)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->len              = NCI_MSG_HDR_SIZE + size;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_RF_MANAGE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_RF_SET_ROUTING);
    UINT8_TO_STREAM (pp, size);
    UINT8_TO_STREAM (pp, more);
    if (size == 2)
    {
        UINT8_TO_STREAM (pp, 0);
    }
    else
    {
        UINT8_TO_STREAM (pp, num_tlv);
        ARRAY_TO_STREAM (pp, p_param_tlvs, tlv_size);
    }
    nfc_ncif_send_cmd (p);

    return (NCI_STATUS_OK);
}

/*******************************************************************************
**
** Function         nci_snd_get_routing_cmd
**
** Description      compose and send RF Management GET_LISTEN_MODE_ROUTING command
**                  to command queue
**
** Returns          status
**
*******************************************************************************/
UINT8 nci_snd_get_routing_cmd (void)
{
    BT_HDR *p;
    UINT8 *pp;
    UINT8   param_size = 0;

    if ((p = NCI_GET_CMD_BUF (param_size)) == NULL)
        return (NCI_STATUS_FAILED);

    p->event            = BT_EVT_TO_NFC_NCI;
    p->len              = NCI_MSG_HDR_SIZE + param_size;
    p->offset           = NCI_MSG_OFFSET_SIZE;
    p->layer_specific   = 0;
    pp                  = (UINT8 *) (p + 1) + p->offset;

    NCI_MSG_BLD_HDR0 (pp, NCI_MT_CMD, NCI_GID_RF_MANAGE);
    NCI_MSG_BLD_HDR1 (pp, NCI_MSG_RF_GET_ROUTING);
    UINT8_TO_STREAM (pp, param_size);

    nfc_ncif_send_cmd (p);
    return (NCI_STATUS_OK);
}
#endif
#endif


#endif /* NFC_INCLUDED == TRUE*/
