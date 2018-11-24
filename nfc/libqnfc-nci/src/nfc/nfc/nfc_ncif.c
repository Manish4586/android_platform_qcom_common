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
 *  Copyright (C) 1999-2014 Broadcom Corporation
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
 *  This file contains functions that interface with the NFC NCI transport.
 *  On the receive side, it routes events to the appropriate handler
 *  (callback). On the transmit side, it manages the command transmission.
 *
 ******************************************************************************/
#include <string.h>
#include "nfc_target.h"

#if NFC_INCLUDED == TRUE
#include "nfc_hal_api.h"
#include "nfc_api.h"
#include "nci_defs.h"
#include "nci_hmsgs.h"
#include "nfc_int.h"
#include "rw_api.h"
#include "rw_int.h"
#include "hcidefs.h"
#include "nfc_hal_api.h"

#ifdef DTA // <DTA>
#include "dta_flag.h"
#include "config.h"
#include "nfa_sys.h"
#include "nfa_dm_int.h"
BOOLEAN is_deact_ntf = FALSE;
#endif // </DTA>

#if (NFC_RW_ONLY == FALSE)
static const UINT8 nfc_mpl_code_to_size[] =
{64, 128, 192, 254};

#endif /* NFC_RW_ONLY */


#define NFC_PB_ATTRIB_REQ_FIXED_BYTES   1
#define NFC_LB_ATTRIB_REQ_FIXED_BYTES   8


/*******************************************************************************
**
** Function         nfc_ncif_update_window
**
** Description      Update tx cmd window to indicate that NFCC can received
**
** Returns          void
**
*********************************************************************************/
void nfc_ncif_update_window (void)
{
    /* Sanity check - see if we were expecting a update_window */
    if (nfc_cb.nci_cmd_window == NCI_MAX_CMD_WINDOW)
    {
        if (nfc_cb.nfc_state != NFC_STATE_W4_HAL_CLOSE)
        {
            NFC_TRACE_ERROR0("nfc_ncif_update_window: Unexpected call");
        }
        return;
    }

    /* Stop command-pending timer */
    nfc_stop_timer (&nfc_cb.nci_wait_rsp_timer);

    nfc_cb.p_vsc_cback = NULL;
    nfc_cb.nci_cmd_window++;

    /* Check if there were any commands waiting to be sent */
    nfc_ncif_check_cmd_queue (NULL);
}

/*******************************************************************************
**
** Function         nfc_ncif_cmd_timeout
**
** Description      Handle a command timeout
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_cmd_timeout (void)
{
    NFC_TRACE_ERROR0 ("nfc_ncif_cmd_timeout");

    /* report an error */
    nfc_ncif_event_status(NFC_GEN_ERROR_REVT, NFC_STATUS_HW_TIMEOUT);
    nfc_ncif_event_status(NFC_NFCC_TIMEOUT_REVT, NFC_STATUS_HW_TIMEOUT);

    /* if enabling NFC, notify upper layer of failure */
    if (nfc_cb.nfc_state == NFC_STATE_CORE_INIT)
    {
        nfc_enabled (NFC_STATUS_FAILED, NULL);
    }

    /* terminate the process so we'll try again */
    NFC_TRACE_ERROR0 ("****NFCC NCI CMD Timeout*****");
    abort();
}

/*******************************************************************************
**
** Function         nfc_wait_2_deactivate_timeout
**
** Description      Handle a command timeout
**
** Returns          void
**
*******************************************************************************/
void nfc_wait_2_deactivate_timeout (void)
{
    NFC_TRACE_ERROR0 ("nfc_wait_2_deactivate_timeout");
    nfc_cb.flags  &= ~NFC_FL_DEACTIVATING;
    nci_snd_deactivate_cmd ((UINT8) ((TIMER_PARAM_TYPE) nfc_cb.deactivate_timer.param));
}


/*******************************************************************************
**
** Function         nfc_ncif_send_data
**
** Description      This function is called to add the NCI data header
**                  and send it to NCIT task for sending it to transport
**                  as credits are available.
**
** Returns          void
**
*******************************************************************************/
UINT8 nfc_ncif_send_data (tNFC_CONN_CB *p_cb, BT_HDR *p_data)
{
    UINT8 *pp;
    UINT8 *ps;
    UINT8   ulen = NCI_MAX_PAYLOAD_SIZE;
    BT_HDR *p;
    UINT8   pbf = 1;
    UINT8   buffer_size = p_cb->buff_size;
    UINT8   hdr0 = p_cb->conn_id;
    BOOLEAN fragmented = FALSE;

    NFC_TRACE_DEBUG3 ("nfc_ncif_send_data :%d, num_buff:%d qc:%d", p_cb->conn_id, p_cb->num_buff, p_cb->tx_q.count);
    if (p_cb->id == NFC_RF_CONN_ID)
    {
        if (nfc_cb.nfc_state != NFC_STATE_OPEN)
        {
            if (nfc_cb.nfc_state == NFC_STATE_CLOSING)
            {
                if ((p_data == NULL) && /* called because credit from NFCC */
                    (nfc_cb.flags  & NFC_FL_DEACTIVATING))
                {
                    if (p_cb->init_credits == p_cb->num_buff)
                    {
                        /* all the credits are back */
                        nfc_cb.flags  &= ~NFC_FL_DEACTIVATING;
                        NFC_TRACE_DEBUG2 ("deactivating NFC-DEP init_credits:%d, num_buff:%d", p_cb->init_credits, p_cb->num_buff);
                        nfc_stop_timer(&nfc_cb.deactivate_timer);
                        nci_snd_deactivate_cmd ((UINT8)((TIMER_PARAM_TYPE)nfc_cb.deactivate_timer.param));
                    }
                }
            }
            return NCI_STATUS_FAILED;
        }
    }

    if (p_data)
    {
        /* always enqueue the data to the tx queue */
        GKI_enqueue (&p_cb->tx_q, p_data);
    }

    /* try to send the first data packet in the tx queue  */
    p_data = (BT_HDR *)GKI_getfirst (&p_cb->tx_q);

    /* post data fragment to NCIT task as credits are available */
    while (p_data && (p_data->len >= 0) && (p_cb->num_buff > 0))
    {
        if (p_data->len <= buffer_size)
        {
            pbf         = 0;   /* last fragment */
            ulen        = (UINT8)(p_data->len);
            fragmented  = FALSE;
        }
        else
        {
            fragmented  = TRUE;
            ulen        = buffer_size;
        }

        if (!fragmented)
        {
            /* if data packet is not fragmented, use the original buffer */
            p         = p_data;
            p_data    = (BT_HDR *)GKI_dequeue (&p_cb->tx_q);
        }
        else
        {
            /* the data packet is too big and need to be fragmented
             * prepare a new GKI buffer
             * (even the last fragment to avoid issues) */
            if ((p = NCI_GET_CMD_BUF(ulen)) == NULL)
                return (NCI_STATUS_BUFFER_FULL);
            p->len    = ulen;
            p->offset = NCI_MSG_OFFSET_SIZE + NCI_DATA_HDR_SIZE + 1;
            if (p->len)
            {
            pp        = (UINT8 *)(p + 1) + p->offset;
            ps        = (UINT8 *)(p_data + 1) + p_data->offset;
            memcpy (pp, ps, ulen);
            }
            /* adjust the BT_HDR on the old fragment */
            p_data->len     -= ulen;
            p_data->offset  += ulen;
        }

        p->event             = BT_EVT_TO_NFC_NCI;
        p->layer_specific    = pbf;
        p->len              += NCI_DATA_HDR_SIZE;
        p->offset           -= NCI_DATA_HDR_SIZE;
        pp = (UINT8 *)(p + 1) + p->offset;
        /* build NCI Data packet header */
        NCI_DATA_PBLD_HDR(pp, pbf, hdr0, ulen);

        if (p_cb->num_buff != NFC_CONN_NO_FC)
            p_cb->num_buff--;

        /* send to HAL */
        HAL_WRITE(p);

        if (!fragmented)
        {
            /* check if there are more data to send */
            p_data = (BT_HDR *)GKI_getfirst (&p_cb->tx_q);
        }
    }

    return (NCI_STATUS_OK);
}

/*******************************************************************************
**
** Function         nfc_ncif_check_cmd_queue
**
** Description      Send NCI command to the transport
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_check_cmd_queue (BT_HDR *p_buf)
{
    UINT8   *ps = NULL;
    UINT8   *p_nci_hdr = NULL;
    BOOLEAN fragmented = FALSE;
#ifdef DTA // <DTA>
    static BOOLEAN is_rf_disable = FALSE;
    static UINT32 delay = 0;
    UINT8  current_dh_state = nfa_dm_cb.disc_cb.disc_state;
    UINT8 i = 0;
#endif // </DTA>
    /* If there are commands waiting in the xmit queue, or if the controller cannot accept any more commands, */
    /* then enqueue this command */
    if (p_buf)
    {
        if ((nfc_cb.nci_cmd_xmit_q.count) || (nfc_cb.nci_cmd_window == 0))
        {
            GKI_enqueue (&nfc_cb.nci_cmd_xmit_q, p_buf);
            p_buf = NULL;
        }
    }

    /* If controller can accept another command, then send the next command */
    if (nfc_cb.nci_cmd_window > 0)
    {
        /* If no command was provided, or if older commands were in the queue, then get cmd from the queue */
        if (!p_buf)
            p_buf = (BT_HDR *)GKI_dequeue (&nfc_cb.nci_cmd_xmit_q);

        if (p_buf)
        {
            /* save the message header to double check the response */
            ps   = (UINT8 *)(p_buf + 1) + p_buf->offset;
            memcpy(nfc_cb.last_hdr, ps, NFC_SAVED_HDR_SIZE);
            memcpy(nfc_cb.last_cmd, ps + NCI_MSG_HDR_SIZE, NFC_SAVED_CMD_SIZE);
            if (p_buf->layer_specific == NFC_WAIT_RSP_VSC)
            {
                /* save the callback for NCI VSCs)  */
                nfc_cb.p_vsc_cback = (void *)((tNFC_NCI_VS_MSG *)p_buf)->p_cback;
            }

#ifdef DTA // <DTA>
            if(in_dta_mode() )
            {
              if((current_dh_state != NFA_DM_RFST_W4_HOST_SELECT) && (current_dh_state != NFA_DM_RFST_DISCOVERY) && (current_dh_state != NFA_DM_RFST_LISTEN_SLEEP))
              {
                if((ps[0] == 0x21) && (ps[1] == NCI_MSG_RF_DEACTIVATE) && (ps[2] == 0x01) && (ps[3] == NCI_DEACTIVATE_TYPE_IDLE))
                { // 21 06 01 00
                  NFC_TRACE_DEBUG4 ("Deactivate to idle from DH : %x %x %x %x", ps[0], ps[1], ps[2], ps[3]);
                  is_rf_disable = TRUE;
                }
                else
                { // seen for 2nd time
                  if(is_rf_disable == TRUE)
                  {
                    NFC_TRACE_DEBUG4 ("Last cmd was deactivate to idle, insert delay: %x %x %x %x", ps[0], ps[1], ps[2], ps[3]);
                    is_rf_disable = FALSE;
                    GetNumValue("CMD_QUEUE_DELAY", &delay, sizeof(delay));
                    for(i=0;i < 10;i++)
                    {
                      NFC_TRACE_DEBUG1 ("is_deact_ntf : %u", is_deact_ntf);
                      if(is_deact_ntf)
                      {
                        NFC_TRACE_DEBUG0 ("RF_DEACTIVATE_NTF recieved...break loop");
                        is_deact_ntf = FALSE;
                        break;
                      }
                      NFC_TRACE_DEBUG1 ("Waiting for %d ms", delay);
                      GKI_delay(delay);
                    }
                  }
                  else
                  {
                    is_deact_ntf = FALSE;
                  }
                }
              }
            } /* in_dta_mode() */
#endif // </DTA>

            /* check if fragmentition is required */
            while(p_buf->len > (nfc_cb.nci_ctrl_size + NCI_MSG_HDR_SIZE))
            {
                NFC_TRACE_DEBUG1 ("Fragmenting, p_buf->len = %d",p_buf->len);
                fragmented = TRUE;

                p_nci_hdr = (UINT8 *) (p_buf + 1) + p_buf->offset;

                *p_nci_hdr |= NCI_PBF_ST_CONT;
                *(p_nci_hdr + 2)  = nfc_cb.nci_ctrl_size;

                /* send Fragmented payload to HAL */
                nfc_cb.p_hal->write((nfc_cb.nci_ctrl_size + NCI_MSG_HDR_SIZE), (UINT8 *)(p_buf + 1) + p_buf->offset);

                /* update len and offset */
                p_buf->len -= nfc_cb.nci_ctrl_size;
                p_buf->offset += nfc_cb.nci_ctrl_size;

                /* restore NCI Header 3 bytes ahead of payload */
                ps   = (UINT8 *) (p_buf + 1) + p_buf->offset;
                memcpy (ps, p_nci_hdr, NCI_MSG_HDR_SIZE);

                if(p_buf->len <= (nfc_cb.nci_ctrl_size + NCI_MSG_HDR_SIZE))
                {
                    *ps &= ~(UINT8)(NCI_PBF_ST_CONT);
                    *(ps + 2)  = (UINT8)(p_buf->len - NCI_MSG_HDR_SIZE);
                }
            }

            /* send to HAL */
            nfc_cb.p_hal->write(p_buf->len, (UINT8 *)(p_buf + 1) + p_buf->offset);

            /*  QNCI_FEATURE_UI_SCREEN_ERR_HANDLE */
            if (nfa_dm_cb.req_screen_off != 0xFF)
            {
                nfa_dm_cb.req_screen_off = 0xFF;
                /* screen off cmd was sent */
            }
            ps = (UINT8 *)(p_buf + 1) + p_buf->offset;
            /* Check only if No fragmentation done */
            if (!fragmented &&
                (ps[0] == (NCI_MTS_CMD | NCI_GID_PROP)) &&
                (ps[1] == NCI_MSG_PROP_GENERIC) &&
                (ps[2] == 0x03))
            {
                if (ps[3] == NCI_MSG_PROP_GENERIC_SUBCMD_UI_STATE)
                {
                    if ((nfa_dm_cb.poll_locked_mode == TRUE) &&
                        (ps[5] == NCI_PROP_NFCC_UI_STATE_OFF))
                    {
                        nfa_dm_cb.req_screen_off = ps[5];
                    }
                    else if ((nfa_dm_cb.poll_locked_mode == FALSE) &&
                        ((ps[5] == NCI_PROP_NFCC_UI_STATE_OFF) || (ps[5] == NCI_PROP_NFCC_UI_STATE_LOCKED)))
                    {
                       nfa_dm_cb.req_screen_off = ps[5];
                    }
                    NFC_TRACE_DEBUG0 ("UI_STATE, fake rsp from HAL");
                }
                else if (ps[3] == NCI_MSG_PROP_GENERIC_HAL_CMD)
                {
                    if (ps[4] == NCI_PROP_HAL_POLLING_IN_LOCKED)
                    {
                        if (ps[5] == NCI_PROP_HAL_ENABLE_POLL)
                        {
                            nfa_dm_cb.poll_locked_mode = TRUE;
                            NFC_TRACE_DEBUG0 ("enable poll locked");
                        }
                        else
                        {
                            nfa_dm_cb.poll_locked_mode = FALSE;
                            NFC_TRACE_DEBUG0 ("disable poll locked ");
                        }
                    }
                }
            }
            /* Indicate command is pending */
            nfc_cb.nci_cmd_window--;

            /* start NFC command-timeout timer */
            nfc_start_timer (&nfc_cb.nci_wait_rsp_timer, (UINT16)(NFC_TTYPE_NCI_WAIT_RSP), nfc_cb.nci_wait_rsp_tout);

            GKI_freebuf(p_buf);
        }
    }

    if (nfc_cb.nci_cmd_window == NCI_MAX_CMD_WINDOW)
    {
        /* the command queue must be empty now */
        if (nfc_cb.flags & NFC_FL_CONTROL_REQUESTED)
        {
            /* HAL requested control or stack needs to handle pre-discover */
            nfc_cb.flags &= ~NFC_FL_CONTROL_REQUESTED;
            if (nfc_cb.flags & NFC_FL_DISCOVER_PENDING)
            {
                if (nfc_cb.p_hal->prediscover ())
                {
                    /* HAL has the command window now */
                    nfc_cb.flags         |= NFC_FL_CONTROL_GRANTED;
                    nfc_cb.nci_cmd_window = 0;
                }
                else
                {
                    /* HAL does not need to send command,
                     * - restore the command window and issue the discovery command now */
                    nfc_cb.flags         &= ~NFC_FL_DISCOVER_PENDING;
                    ps                    = (UINT8 *)nfc_cb.p_disc_pending;
                    nci_snd_discover_cmd (*ps, (tNFC_DISCOVER_PARAMS *)(ps + 1));
                    GKI_freebuf (nfc_cb.p_disc_pending);
                    nfc_cb.p_disc_pending = NULL;
                }
            }
            else if (nfc_cb.flags & NFC_FL_HAL_REQUESTED)
            {
                /* grant the control to HAL */
                nfc_cb.flags         &= ~NFC_FL_HAL_REQUESTED;
                nfc_cb.flags         |= NFC_FL_CONTROL_GRANTED;
                nfc_cb.nci_cmd_window = 0;
                nfc_cb.p_hal->control_granted ();
            }
        }
    }
}


/*******************************************************************************
**
** Function         nfc_ncif_send_cmd
**
** Description      Send NCI command to the NCIT task
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_send_cmd (BT_HDR *p_buf)
{
    /* post the p_buf to NCIT task */
    p_buf->event            = BT_EVT_TO_NFC_NCI;
    p_buf->layer_specific   = 0;
    nfc_ncif_check_cmd_queue (p_buf);
}


/*******************************************************************************
**
** Function         nfc_ncif_process_event
**
** Description      This function is called to process the data/response/notification
**                  from NFCC
**
** Returns          TRUE if need to free buffer
**
*******************************************************************************/
BOOLEAN nfc_ncif_process_event (BT_HDR *p_msg)
{
    UINT8   mt, pbf, gid, *p, *pp;
    BOOLEAN free = TRUE;
    UINT8   oid;
    UINT8   *p_old, old_gid, old_oid, old_mt;

    p = (UINT8 *) (p_msg + 1) + p_msg->offset;

    pp = p;
    NCI_MSG_PRS_HDR0 (pp, mt, pbf, gid);

    switch (mt)
    {
    case NCI_MT_DATA:
        NFC_TRACE_DEBUG0 ("NFC received data");
        nfc_ncif_proc_data (p_msg);
        free = TRUE;
        break;

    case NCI_MT_RSP:
        NFC_TRACE_DEBUG1 ("NFC received RSP gid:0x%X", gid);
        oid = ((*pp) & NCI_OID_MASK);
        p_old   = nfc_cb.last_hdr;
        NCI_MSG_PRS_HDR0(p_old, old_mt, pbf, old_gid);
        old_oid = ((*p_old) & NCI_OID_MASK);
        /* make sure this is the RSP we are waiting for before updating the command window */
        if ((old_gid != gid) || (old_oid != oid))
        {
            NFC_TRACE_ERROR2 ("nfc_ncif_process_event(): **WARNING** Unexpected RSP: gid:0x%X, oid:0x%X > Stopping cmd pending timer", gid, oid);
            nfc_stop_timer (&nfc_cb.nci_wait_rsp_timer);
            return TRUE;
        }

        switch (gid)
        {
        case NCI_GID_CORE:      /* 0000b NCI Core group */
            free = nci_proc_core_rsp (p_msg);
            break;
        case NCI_GID_RF_MANAGE:   /* 0001b NCI Discovery group */
            nci_proc_rf_management_rsp (p_msg);
            break;
#if (NFC_NFCEE_INCLUDED == TRUE)
#if (NFC_RW_ONLY == FALSE)
        case NCI_GID_EE_MANAGE:  /* 0x02 0010b NFCEE Discovery group */
            nci_proc_ee_management_rsp (p_msg);
            break;
#endif
#endif
        case NCI_GID_PROP:      /* 1111b Proprietary */
                nci_proc_prop_rsp (p_msg);
            break;
        default:
            NFC_TRACE_ERROR1 ("nfc_ncif_process_event(): **WARNING** NFC: Unknown gid:0x%X", gid);
            break;
        }

        nfc_ncif_update_window ();
        break;

    case NCI_MT_NTF:
        NFC_TRACE_DEBUG1 ("NFC received ntf gid:0x%X", gid);
        switch (gid)
        {
        case NCI_GID_CORE:      /* 0000b NCI Core group */
            nci_proc_core_ntf (p_msg);
            break;
        case NCI_GID_RF_MANAGE:   /* 0001b NCI Discovery group */
            nci_proc_rf_management_ntf (p_msg);
            break;
#if (NFC_NFCEE_INCLUDED == TRUE)
#if (NFC_RW_ONLY == FALSE)
        case NCI_GID_EE_MANAGE:  /* 0x02 0010b NFCEE Discovery group */
            nci_proc_ee_management_ntf (p_msg);
            break;
#endif
#endif
        case NCI_GID_PROP:      /* 1111b Proprietary */
                nci_proc_prop_ntf (p_msg);
            break;
        default:
            NFC_TRACE_ERROR1 ("nfc_ncif_process_event(): **WARNING** NFC: Unknown gid:0x%X", gid);
            break;
        }
        break;

    default:
        NFC_TRACE_DEBUG2 ("nfc_ncif_process_event(): **WARNING** NFC received unknown mt:0x%X, gid:0x%X", mt, gid);
    }

    return (free);
}

/*******************************************************************************
**
** Function         nfc_ncif_rf_management_status
**
** Description      This function is called to report an event
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_rf_management_status (tNFC_DISCOVER_EVT event, UINT8 status)
{
    tNFC_DISCOVER   evt_data;
    if (nfc_cb.p_discv_cback)
    {
        evt_data.status = (tNFC_STATUS) status;
        (*nfc_cb.p_discv_cback) (event, &evt_data);
    }
}

/*******************************************************************************
**
** Function         nfc_ncif_set_config_status
**
** Description      This function is called to report NFC_SET_CONFIG_REVT
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_set_config_status (UINT8 *p, UINT8 len)
{
    tNFC_RESPONSE   evt_data;
    if (nfc_cb.p_resp_cback)
    {
        evt_data.set_config.status          = (tNFC_STATUS) *p++;
        evt_data.set_config.num_param_id    = NFC_STATUS_OK;
        if (evt_data.set_config.status != NFC_STATUS_OK)
        {
            evt_data.set_config.num_param_id    = *p++;
            STREAM_TO_ARRAY (evt_data.set_config.param_ids, p, evt_data.set_config.num_param_id);
        }

        (*nfc_cb.p_resp_cback) (NFC_SET_CONFIG_REVT, &evt_data);
    }
}

/*******************************************************************************
**
** Function         nfc_ncif_event_status
**
** Description      This function is called to report an event
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_event_status (tNFC_RESPONSE_EVT event, UINT8 status)
{
    tNFC_RESPONSE   evt_data;
    if (nfc_cb.p_resp_cback)
    {
        evt_data.status = (tNFC_STATUS) status;
        (*nfc_cb.p_resp_cback) (event, &evt_data);
    }
}

/*******************************************************************************
**
** Function         nfc_ncif_error_status
**
** Description      This function is called to report an error event to data cback
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_error_status (UINT8 conn_id, UINT8 status)
{
    tNFC_CONN_CB * p_cb;
    p_cb = nfc_find_conn_cb_by_conn_id (conn_id);
    if (p_cb && p_cb->p_cback)
    {
        (*p_cb->p_cback) (conn_id, NFC_ERROR_CEVT, (tNFC_CONN *) &status);
    }
}

/* QNCI_FEATURE_ISODEP_PRESENCE */
/*******************************************************************************
**
** Function         nfc_ncif_presence_check_status
**
** Description      This function is called to report an error event to data cback
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_presence_check_status (UINT8 status)
{
    tNFC_CONN_CB * p_cb;
    p_cb = nfc_find_conn_cb_by_conn_id (NFC_RF_CONN_ID);
    if (p_cb && p_cb->p_cback)
    {
        (*p_cb->p_cback) (NFC_RF_CONN_ID, NFC_PRESENCE_CHECK_CEVT, (tNFC_CONN *) &status);
    }
}
/* QNCI_FEATURE_ISODEP_PRESENCE */
/*******************************************************************************
**
** Function         nfc_ncif_proc_rf_field_ntf
**
** Description      This function is called to process RF field notification
**
** Returns          void
**
*******************************************************************************/
#if (NFC_RW_ONLY == FALSE)
void nfc_ncif_proc_rf_field_ntf (UINT8 rf_status)
{
    tNFC_RESPONSE   evt_data;
    if (nfc_cb.p_resp_cback)
    {
        evt_data.status            = (tNFC_STATUS) NFC_STATUS_OK;
        evt_data.rf_field.rf_field = rf_status;
        (*nfc_cb.p_resp_cback) (NFC_RF_FIELD_REVT, &evt_data);
    }
}
#endif

/*******************************************************************************
**
** Function         nfc_ncif_proc_credits
**
** Description      This function is called to process data credits
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_credits(UINT8 *p, UINT16 plen)
{
    UINT8   num, xx;
    tNFC_CONN_CB * p_cb;

    num = *p++;
    for (xx = 0; xx < num; xx++)
    {
        p_cb = nfc_find_conn_cb_by_conn_id(*p++);
        if (p_cb && p_cb->num_buff != NFC_CONN_NO_FC)
        {
            p_cb->num_buff += (*p);
#if (BT_USE_TRACES == TRUE)
            if (p_cb->num_buff > p_cb->init_credits)
            {
                if (nfc_cb.nfc_state == NFC_STATE_OPEN)
                {
                    /* if this happens in activated state, it's very likely that our NFCC has issues */
                    /* However, credit may be returned after deactivation */
                    NFC_TRACE_ERROR2( "num_buff:0x%x, init_credits:0x%x", p_cb->num_buff, p_cb->init_credits);
                }
                p_cb->num_buff = p_cb->init_credits;
            }
#endif
            /* check if there's nay data in tx q to be sent */
            nfc_ncif_send_data (p_cb, NULL);
        }
        p++;
    }
}
/*******************************************************************************
**
** Function         nfc_ncif_decode_rf_params
**
** Description      This function is called to process the detected technology
**                  and mode and the associated parameters for DISCOVER_NTF and
**                  ACTIVATE_NTF
**
** Returns          void
**
*******************************************************************************/
UINT8 * nfc_ncif_decode_rf_params (tNFC_RF_TECH_PARAMS *p_param, UINT8 *p)
{
    tNFC_RF_PA_PARAMS   *p_pa;
    UINT8               len, *p_start, u8;
    tNFC_RF_PB_PARAMS   *p_pb;
    tNFC_RF_LF_PARAMS   *p_lf;
    tNFC_RF_PF_PARAMS   *p_pf;
    tNFC_RF_PISO15693_PARAMS *p_i93;

    len             = *p++;
    p_start         = p;
    memset ( &p_param->param, 0, sizeof (tNFC_RF_TECH_PARAMU));
    switch (p_param->mode)
    {
    case NCI_DISCOVERY_TYPE_POLL_A:
    case NCI_DISCOVERY_TYPE_POLL_A_ACTIVE:
        p_pa        = &p_param->param.pa;
        /*
SENS_RES Response   2 bytes Defined in [DIGPROT] Available after Technology Detection
NFCID1 length   1 byte  Length of NFCID1 Available after Collision Resolution
NFCID1  4, 7, or 10 bytes   Defined in [DIGPROT]Available after Collision Resolution
SEL_RES Response    1 byte  Defined in [DIGPROT]Available after Collision Resolution
HRx Length  1 Octets    Length of HRx Parameters collected from the response to the T1T RID command.
HRx 0 or 2 Octets   If present, the first byte SHALL contain HR0 and the second byte SHALL contain HR1 as defined in [DIGITAL].
        */
        STREAM_TO_ARRAY (p_pa->sens_res, p, 2);
        p_pa->nfcid1_len     = *p++;
        if (p_pa->nfcid1_len > NCI_NFCID1_MAX_LEN)
            p_pa->nfcid1_len = NCI_NFCID1_MAX_LEN;
        STREAM_TO_ARRAY (p_pa->nfcid1, p, p_pa->nfcid1_len);
        u8                   = *p++;
        if (u8)
            p_pa->sel_rsp    = *p++;
        if (len == (7 + p_pa->nfcid1_len + u8)) /* 2(sens_res) + 1(len) + p_pa->nfcid1_len + 1(len) + u8 + hr (1:len + 2) */
        {
            p_pa->hr_len     = *p++;
            if (p_pa->hr_len == NCI_T1T_HR_LEN)
            {
                p_pa->hr[0]  = *p++;
                p_pa->hr[1]  = *p;
            }
        }
        break;

    case NCI_DISCOVERY_TYPE_POLL_B:
        /*
SENSB_RES Response length (n)   1 byte  Length of SENSB_RES Response (Byte 2 - Byte 12 or 13)Available after Technology Detection
SENSB_RES Response Byte 2 - Byte 12 or 13   11 or 12 bytes  Defined in [DIGPROT] Available after Technology Detection
        */
        p_pb                = &p_param->param.pb;
        p_pb->sensb_res_len = *p++;
        if (p_pb->sensb_res_len > NCI_MAX_SENSB_RES_LEN)
            p_pb->sensb_res_len = NCI_MAX_SENSB_RES_LEN;
        STREAM_TO_ARRAY (p_pb->sensb_res, p, p_pb->sensb_res_len);
        memcpy (p_pb->nfcid0, p_pb->sensb_res, NFC_NFCID0_MAX_LEN);
        break;

    case NCI_DISCOVERY_TYPE_POLL_F:
    case NCI_DISCOVERY_TYPE_POLL_F_ACTIVE:
        /*
Bit Rate    1 byte  1   212 kbps/2   424 kbps/0 and 3 to 255  RFU
SENSF_RES Response length.(n) 1 byte  Length of SENSF_RES (Byte 2 - Byte 17 or 19).Available after Technology Detection
SENSF_RES Response Byte 2 - Byte 17 or 19  n bytes Defined in [DIGPROT] Available after Technology Detection
        */
        p_pf                = &p_param->param.pf;
        p_pf->bit_rate      = *p++;
        p_pf->sensf_res_len = *p++;
        if (p_pf->sensf_res_len > NCI_MAX_SENSF_RES_LEN)
            p_pf->sensf_res_len = NCI_MAX_SENSF_RES_LEN;
        STREAM_TO_ARRAY (p_pf->sensf_res, p, p_pf->sensf_res_len);
        memcpy (p_pf->nfcid2, p_pf->sensf_res, NCI_NFCID2_LEN);
        p_pf->mrti_check    = p_pf->sensf_res[NCI_MRTI_CHECK_INDEX];
        p_pf->mrti_update   = p_pf->sensf_res[NCI_MRTI_UPDATE_INDEX];
        break;

    case NCI_DISCOVERY_TYPE_LISTEN_F:
    case NCI_DISCOVERY_TYPE_LISTEN_F_ACTIVE:
        p_lf                = &p_param->param.lf;
        u8                  = *p++;
        if (u8)
        {
            STREAM_TO_ARRAY (p_lf->nfcid2, p, NCI_NFCID2_LEN);
        }
        break;

    case NCI_DISCOVERY_TYPE_POLL_ISO15693:
        p_i93               = &p_param->param.pi93;
        /*Bypass Inventory Rsp Length*/
        *p++;
        p_i93->flag         = *p++;
        p_i93->dsfid        = *p++;
        STREAM_TO_ARRAY (p_i93->uid, p, NFC_ISO15693_UID_LEN);
        break;

    case NCI_DISCOVERY_TYPE_POLL_KOVIO:
        p_param->param.pk.uid_len = *p++;
        if (p_param->param.pk.uid_len > NFC_KOVIO_MAX_LEN)
        {
            NFC_TRACE_ERROR2( "Kovio UID len:0x%x exceeds max(0x%x)", p_param->param.pk.uid_len, NFC_KOVIO_MAX_LEN);
            p_param->param.pk.uid_len = NFC_KOVIO_MAX_LEN;
        }
        STREAM_TO_ARRAY (p_param->param.pk.uid, p, p_param->param.pk.uid_len);
        break;
    }

    return (p_start + len);
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_discover_ntf
**
** Description      This function is called to process discover notification
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_discover_ntf (UINT8 *p, UINT16 plen)
{
    tNFC_DISCOVER   evt_data;

    if (nfc_cb.p_discv_cback)
    {
        p                              += NCI_MSG_HDR_SIZE;
        evt_data.status                 = NCI_STATUS_OK;
        evt_data.result.rf_disc_id      = *p++;
        evt_data.result.protocol        = *p++;

        /* fill in tNFC_RESULT_DEVT */
        evt_data.result.rf_tech_param.mode  = *p++;
        p = nfc_ncif_decode_rf_params (&evt_data.result.rf_tech_param, p);

        evt_data.result.more            = *p++;
        (*nfc_cb.p_discv_cback) (NFC_RESULT_DEVT, &evt_data);
    }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_activate
**
** Description      This function is called to process de-activate
**                  response and notification
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_activate (UINT8 *p, UINT8 len)
{
    tNFC_DISCOVER   evt_data;
    tNFC_INTF_PARAMS        *p_intf = &evt_data.activate.intf_param;
    tNFC_INTF_PA_ISO_DEP    *p_pa_iso;
    tNFC_INTF_LB_ISO_DEP    *p_lb_iso;
    tNFC_INTF_PB_ISO_DEP    *p_pb_iso;
#if (NFC_RW_ONLY == FALSE)
    tNFC_INTF_PA_NFC_DEP    *p_pa_nfc;
    int                     mpl_idx = 0;
    UINT8                   gb_idx = 0, mpl;
#endif
    UINT8                   t0;
    tNCI_DISCOVERY_TYPE     mode;
    tNFC_CONN_CB * p_cb = &nfc_cb.conn_cb[NFC_RF_CONN_ID];
    UINT8                   *pp, len_act;
    UINT8                   buff_size, num_buff;
    tNFC_RF_PA_PARAMS       *p_pa;

    nfc_set_state (NFC_STATE_OPEN);

    memset (p_intf, 0, sizeof (tNFC_INTF_PARAMS));
    evt_data.activate.rf_disc_id    = *p++;
    p_intf->type                    = *p++;
    evt_data.activate.protocol      = *p++;

    if (evt_data.activate.protocol == NCI_PROTOCOL_18092_ACTIVE)
        evt_data.activate.protocol = NCI_PROTOCOL_NFC_DEP;

    evt_data.activate.rf_tech_param.mode    = *p++;
    buff_size                               = *p++;
    num_buff                                = *p++;
    /* fill in tNFC_activate_DEVT */
    p = nfc_ncif_decode_rf_params (&evt_data.activate.rf_tech_param, p);

    evt_data.activate.data_mode             = *p++;
    evt_data.activate.tx_bitrate            = *p++;
    evt_data.activate.rx_bitrate            = *p++;
    mode         = evt_data.activate.rf_tech_param.mode;
    len_act      = *p++;
    NFC_TRACE_DEBUG3 ("nfc_ncif_proc_activate:%d %d, mode:0x%02x", len, len_act, mode);
    /* just in case the interface reports activation parameters not defined in the NCI spec */
    p_intf->intf_param.frame.param_len      = len_act;
    if (p_intf->intf_param.frame.param_len > NFC_MAX_RAW_PARAMS)
        p_intf->intf_param.frame.param_len = NFC_MAX_RAW_PARAMS;
    pp = p;
    STREAM_TO_ARRAY (p_intf->intf_param.frame.param, pp, p_intf->intf_param.frame.param_len);
    if (evt_data.activate.intf_param.type == NCI_INTERFACE_ISO_DEP)
    {
        /* Make max payload of NCI aligned to max payload of ISO-DEP for better performance */
        if (buff_size > NCI_ISO_DEP_MAX_INFO)
            buff_size = NCI_ISO_DEP_MAX_INFO;

        switch (mode)
        {
        case NCI_DISCOVERY_TYPE_POLL_A:
            p_pa_iso                  = &p_intf->intf_param.pa_iso;
            p_pa_iso->ats_res_len     = *p++;

            if (p_pa_iso->ats_res_len == 0)
                break;

            if (p_pa_iso->ats_res_len > NFC_MAX_ATS_LEN)
                p_pa_iso->ats_res_len = NFC_MAX_ATS_LEN;
            STREAM_TO_ARRAY (p_pa_iso->ats_res, p, p_pa_iso->ats_res_len);
            pp = &p_pa_iso->ats_res[NCI_ATS_T0_INDEX];
            t0 = p_pa_iso->ats_res[NCI_ATS_T0_INDEX];
            pp++;       /* T0 */
            if (t0 & NCI_ATS_TA_MASK)
                pp++;   /* TA */
            if (t0 & NCI_ATS_TB_MASK)
            {
                /* FWI (Frame Waiting time Integer) & SPGI (Start-up Frame Guard time Integer) */
                p_pa_iso->fwi       = (((*pp) >> 4) & 0x0F);
                p_pa_iso->sfgi      = ((*pp) & 0x0F);
                pp++;   /* TB */
            }
            if (t0 & NCI_ATS_TC_MASK)
            {
                p_pa_iso->nad_used  = ((*pp) & 0x01);
                pp++;   /* TC */
            }
            p_pa_iso->his_byte_len  = (UINT8) (p_pa_iso->ats_res_len - (pp - p_pa_iso->ats_res));
            memcpy (p_pa_iso->his_byte,  pp, p_pa_iso->his_byte_len);
            break;

        case NCI_DISCOVERY_TYPE_LISTEN_A:
            p_intf->intf_param.la_iso.rats = *p++;
            break;

        case NCI_DISCOVERY_TYPE_POLL_B:
            /* ATTRIB RSP
            Byte 1   Byte 2 ~ 2+n-1
            MBLI/DID Higher layer - Response
            */
            p_pb_iso                     = &p_intf->intf_param.pb_iso;
            p_pb_iso->attrib_res_len     = *p++;

            if (p_pb_iso->attrib_res_len == 0)
                break;

            if (p_pb_iso->attrib_res_len > NFC_MAX_ATTRIB_LEN)
                p_pb_iso->attrib_res_len = NFC_MAX_ATTRIB_LEN;
            STREAM_TO_ARRAY (p_pb_iso->attrib_res, p, p_pb_iso->attrib_res_len);
            p_pb_iso->mbli = (p_pb_iso->attrib_res[0]) >> 4;
            if (p_pb_iso->attrib_res_len > NFC_PB_ATTRIB_REQ_FIXED_BYTES)
            {
                p_pb_iso->hi_info_len    = p_pb_iso->attrib_res_len - NFC_PB_ATTRIB_REQ_FIXED_BYTES;
                if (p_pb_iso->hi_info_len > NFC_MAX_GEN_BYTES_LEN)
                    p_pb_iso->hi_info_len = NFC_MAX_GEN_BYTES_LEN;
                memcpy (p_pb_iso->hi_info, &p_pb_iso->attrib_res[NFC_PB_ATTRIB_REQ_FIXED_BYTES], p_pb_iso->hi_info_len);
            }
            break;

        case NCI_DISCOVERY_TYPE_LISTEN_B:
            /* ATTRIB CMD
            Byte 2~5 Byte 6  Byte 7  Byte 8  Byte 9  Byte 10 ~ 10+k-1
            NFCID0   Param 1 Param 2 Param 3 Param 4 Higher layer - INF
            */
            p_lb_iso                     = &p_intf->intf_param.lb_iso;
            p_lb_iso->attrib_req_len     = *p++;

            if (p_lb_iso->attrib_req_len == 0)
                break;

            if (p_lb_iso->attrib_req_len > NFC_MAX_ATTRIB_LEN)
                p_lb_iso->attrib_req_len = NFC_MAX_ATTRIB_LEN;
            STREAM_TO_ARRAY (p_lb_iso->attrib_req, p, p_lb_iso->attrib_req_len);
            memcpy (p_lb_iso->nfcid0, p_lb_iso->attrib_req, NFC_NFCID0_MAX_LEN);
            if (p_lb_iso->attrib_req_len > NFC_LB_ATTRIB_REQ_FIXED_BYTES)
            {
                p_lb_iso->hi_info_len    = p_lb_iso->attrib_req_len - NFC_LB_ATTRIB_REQ_FIXED_BYTES;
                if (p_lb_iso->hi_info_len > NFC_MAX_GEN_BYTES_LEN)
                    p_lb_iso->hi_info_len = NFC_MAX_GEN_BYTES_LEN;
                memcpy (p_lb_iso->hi_info, &p_lb_iso->attrib_req[NFC_LB_ATTRIB_REQ_FIXED_BYTES], p_lb_iso->hi_info_len);
            }
            break;
        }

    }
#if (NFC_RW_ONLY == FALSE)
    else if (evt_data.activate.intf_param.type == NCI_INTERFACE_NFC_DEP)
    {
        /* Make max payload of NCI aligned to max payload of NFC-DEP for better performance */
        if (buff_size > NCI_NFC_DEP_MAX_DATA)
            buff_size = NCI_NFC_DEP_MAX_DATA;

        p_pa_nfc                  = &p_intf->intf_param.pa_nfc;
        p_pa_nfc->atr_res_len     = *p++;

        if (p_pa_nfc->atr_res_len > 0)
        {
            if (p_pa_nfc->atr_res_len > NFC_MAX_ATS_LEN)
                p_pa_nfc->atr_res_len = NFC_MAX_ATS_LEN;
            STREAM_TO_ARRAY (p_pa_nfc->atr_res, p, p_pa_nfc->atr_res_len);
            if (  (mode == NCI_DISCOVERY_TYPE_POLL_A)
                ||(mode == NCI_DISCOVERY_TYPE_POLL_F)
                ||(mode == NCI_DISCOVERY_TYPE_POLL_A_ACTIVE)
                ||(mode == NCI_DISCOVERY_TYPE_POLL_F_ACTIVE)  )
            {
                /* ATR_RES
                Byte 3~12 Byte 13 Byte 14 Byte 15 Byte 16 Byte 17 Byte 18~18+n
                NFCID3T   DIDT    BST     BRT     TO      PPT     [GT0 ... GTn] */
                mpl_idx                 = 14;
                gb_idx                  = NCI_P_GEN_BYTE_INDEX;
                p_pa_nfc->waiting_time  = p_pa_nfc->atr_res[NCI_L_NFC_DEP_TO_INDEX] & 0x0F;
            }
            else if (  (mode == NCI_DISCOVERY_TYPE_LISTEN_A)
                     ||(mode == NCI_DISCOVERY_TYPE_LISTEN_F)
                     ||(mode == NCI_DISCOVERY_TYPE_LISTEN_A_ACTIVE)
                     ||(mode == NCI_DISCOVERY_TYPE_LISTEN_F_ACTIVE)  )
            {
                /* ATR_REQ
                Byte 3~12 Byte 13 Byte 14 Byte 15 Byte 16 Byte 17~17+n
                NFCID3I   DIDI    BSI     BRI     PPI     [GI0 ... GIn] */
                mpl_idx = 13;
                gb_idx  = NCI_L_GEN_BYTE_INDEX;
            }

            mpl                         = ((p_pa_nfc->atr_res[mpl_idx]) >> 4) & 0x03;
            p_pa_nfc->max_payload_size  = nfc_mpl_code_to_size[mpl];
            if (p_pa_nfc->atr_res_len > gb_idx)
            {
                p_pa_nfc->gen_bytes_len = p_pa_nfc->atr_res_len - gb_idx;
                if (p_pa_nfc->gen_bytes_len > NFC_MAX_GEN_BYTES_LEN)
                    p_pa_nfc->gen_bytes_len = NFC_MAX_GEN_BYTES_LEN;
                memcpy (p_pa_nfc->gen_bytes, &p_pa_nfc->atr_res[gb_idx], p_pa_nfc->gen_bytes_len);
            }
        }
    }
#endif
    else if ((evt_data.activate.intf_param.type == NCI_INTERFACE_FRAME) && (evt_data.activate.protocol == NCI_PROTOCOL_T1T) )
    {
        p_pa = &evt_data.activate.rf_tech_param.param.pa;
        if ((len_act == NCI_T1T_HR_LEN) && (p_pa->hr_len == 0))
        {
            p_pa->hr_len    = NCI_T1T_HR_LEN;
            p_pa->hr[0]     = *p++;
            p_pa->hr[1]     = *p++;
        }
    }

    p_cb->act_protocol  = evt_data.activate.protocol;
    p_cb->buff_size     = buff_size;
    p_cb->num_buff      = num_buff;
    p_cb->init_credits  = num_buff;

    if (nfc_cb.p_discv_cback)
    {
        (*nfc_cb.p_discv_cback) (NFC_ACTIVATE_DEVT, &evt_data);
    }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_deactivate
**
** Description      This function is called to process de-activate
**                  response and notification
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_deactivate (UINT8 status, UINT8 deact_type, BOOLEAN is_ntf)
{
    tNFC_DISCOVER   evt_data;
    tNFC_DEACTIVATE_DEVT    *p_deact;
    tNFC_CONN_CB * p_cb = &nfc_cb.conn_cb[NFC_RF_CONN_ID];
    void    *p_data;

    nfc_set_state (NFC_STATE_IDLE);
    p_deact             = &evt_data.deactivate;
    p_deact->status     = status;
    p_deact->type       = deact_type;
    p_deact->is_ntf     = is_ntf;

    while ((p_data = GKI_dequeue (&p_cb->rx_q)) != NULL)
    {
        GKI_freebuf (p_data);
    }

    while ((p_data = GKI_dequeue (&p_cb->tx_q)) != NULL)
    {
        GKI_freebuf (p_data);
    }

    if (p_cb->p_cback)
        (*p_cb->p_cback) (NFC_RF_CONN_ID, NFC_DEACTIVATE_CEVT, (tNFC_CONN *) p_deact);

    if (nfc_cb.p_discv_cback)
    {
        (*nfc_cb.p_discv_cback) (NFC_DEACTIVATE_DEVT, &evt_data);
    }
}
/*******************************************************************************
**
** Function         nfc_ncif_proc_ee_action
**
** Description      This function is called to process NFCEE ACTION NTF
**
** Returns          void
**
*******************************************************************************/
#if ((NFC_NFCEE_INCLUDED == TRUE) && (NFC_RW_ONLY == FALSE))
void nfc_ncif_proc_ee_action (UINT8 *p, UINT16 plen)
{
    tNFC_EE_ACTION_REVT evt_data;
    tNFC_RESPONSE_CBACK *p_cback = nfc_cb.p_resp_cback;
    UINT8   data_len, ulen, tag, *p_data;
    UINT8   max_len;

    if (p_cback)
    {
        memset (&evt_data.act_data, 0, sizeof (tNFC_ACTION_DATA));
        evt_data.status             = NFC_STATUS_OK;
        evt_data.nfcee_id           = *p++;
        evt_data.act_data.trigger   = *p++;
        data_len                    = *p++;
        if (plen >= 3)
            plen -= 3;
        if (data_len > plen)
            data_len = (UINT8) plen;

        switch (evt_data.act_data.trigger)
        {
        case NCI_EE_TRIG_7816_SELECT:
            if (data_len > NFC_MAX_AID_LEN)
                data_len = NFC_MAX_AID_LEN;
            evt_data.act_data.param.aid.len_aid = data_len;
            STREAM_TO_ARRAY (evt_data.act_data.param.aid.aid, p, data_len);
            break;
        case NCI_EE_TRIG_RF_PROTOCOL:
            evt_data.act_data.param.protocol    = *p++;
            break;
        case NCI_EE_TRIG_RF_TECHNOLOGY:
            evt_data.act_data.param.technology  = *p++;
            break;
        case NCI_EE_TRIG_APP_INIT:
            while (data_len > NFC_TL_SIZE)
            {
                data_len    -= NFC_TL_SIZE;
                tag         = *p++;
                ulen        = *p++;
                if (ulen == 0x81) // 2bytes BER-TLV, length between 0x80 and 0xFF at next byte
                {
                    ulen = *p++;
                    data_len--;
                }
                if (ulen > data_len)
                    ulen = data_len;
                p_data      = NULL;
                max_len     = ulen;
                switch (tag)
                {
                case NCI_EE_ACT_TAG_AID:    /* AID                 */
                    if (max_len > NFC_MAX_AID_LEN)
                        max_len = NFC_MAX_AID_LEN;
                    evt_data.act_data.param.app_init.len_aid = max_len;
                    p_data = evt_data.act_data.param.app_init.aid;
                    break;
                case NCI_EE_ACT_TAG_DATA:   /* hex data for app    */
                    if (max_len > NFC_MAX_APP_DATA_LEN)
                        max_len = NFC_MAX_APP_DATA_LEN;
                    evt_data.act_data.param.app_init.len_data   = max_len;
                    p_data                                      = evt_data.act_data.param.app_init.data;
                    break;
                }
                if (p_data)
                {
                    STREAM_TO_ARRAY (p_data, p, max_len);
                }
                data_len -= ulen;
            }
            break;
        }
        (*p_cback) (NFC_EE_ACTION_REVT, (tNFC_RESPONSE *) &evt_data);
    }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_ee_discover_req
**
** Description      This function is called to process NFCEE DISCOVER REQ NTF
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_ee_discover_req (UINT8 *p, UINT16 plen)
{
    tNFC_RESPONSE_CBACK *p_cback = nfc_cb.p_resp_cback;
    tNFC_EE_DISCOVER_REQ_REVT   ee_disc_req;
    tNFC_EE_DISCOVER_INFO       *p_info;
#ifdef DTA // <DTA>
    tNFC_EE_DISCOVER_INFO_DTA   *p_info_dta;
#endif // </DTA>
    UINT8                       u8;

    NFC_TRACE_DEBUG2 ("nfc_ncif_proc_ee_discover_req %d len:%d", *p, plen);
    if (p_cback)
    {
        u8  = *p;
        ee_disc_req.status      = NFC_STATUS_OK;
        ee_disc_req.num_info    = *p++;
        p_info                  = ee_disc_req.info;
#ifdef DTA // <DTA>
        p_info_dta = ee_disc_req.info;
#endif // </DTA>
        if (plen)
            plen--;
#ifdef DTA // <DTA>
        if(in_dta_mode() ) {
            while ((u8 > 0) && (plen >= NFC_EE_DISCOVER_ENTRY_LEN))
            {
                p_info_dta->op  = *p++;                  /* T */
                if (*p != NFC_EE_DISCOVER_INFO_LEN)/* L */
                {
                    NFC_TRACE_DEBUG1 ("bad entry len:%d", *p );
                    return;
                }
                p++;
                /* V */
                p_info_dta->nfcee_id    = *p++;
                p_info_dta->tech_n_mode = *p++;
                p_info_dta->protocol    = *p++;
                u8--;
                plen    -=NFC_EE_DISCOVER_ENTRY_LEN;
                p_info_dta++;
            }
        } else {
#endif // </DTA>
        while ((u8 > 0) && (plen > 0))
        {
            p_info->op = *p++;                  /* T */
            switch (p_info->op)
            {
                case NFC_EE_DISC_OP_ADD:
                case NFC_EE_DISC_OP_REMOVE:
                    if (  (*p != NFC_EE_DISCOVER_REQ_INFO_LEN)    /* L */
                        ||(plen < NFC_EE_DISCOVER_REQ_ENTRY_LEN)  )
                    {
                        NFC_TRACE_DEBUG1 ("bad len for DISC_REQ:%d", *p );
                        return;
                    }
                    p++;
                    /* V */
                    p_info->nfcee_id                  = *p++;
                    p_info->info.req_info.tech_n_mode = *p++;
                    p_info->info.req_info.protocol    = *p++;
                    u8--;
                    plen -= NFC_EE_DISCOVER_REQ_ENTRY_LEN;
                    p_info++;
                    break;

                case NFC_EE_DISC_OP_SAK_INFO:
                    if (  (*p != NFC_EE_SAK_INFO_LEN)        /* L */
                        ||(plen < NFC_EE_SAK_ENTRY_LEN)  )
                    {
                        NFC_TRACE_DEBUG1 ("bad len for SAK:%d", *p );
                        return;
                    }
                    p++;
                    /* V */
                    p_info->nfcee_id          = *p++;
                    p_info->info.sak_info.sak = *p++;
                    u8--;
                    plen -= NFC_EE_SAK_ENTRY_LEN;
                    p_info++;
                    break;

                default:
                    NFC_TRACE_DEBUG1 ("Unknown type:0x%x", *p );
                    p++;
                    if (plen < (*p + 2))
                        return;
                    plen -= (*p + 2);
                    p += (*p + 1);     /* move to next TLV */
                    u8--;
                    ee_disc_req.num_info--;
                    break;
            }
        }
#ifdef DTA // <DTA>
        }
#endif // </DTA>
        (*p_cback) (NFC_EE_DISCOVER_REQ_REVT, (tNFC_RESPONSE *) &ee_disc_req);
    }

}

#define NFC_LMRT_NTF_LAST_MESSAGE 0x00

/*******************************************************************************
**
** Function         nfc_ncif_proc_get_routing
**
** Description      This function is called to process get routing notification
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_get_routing (UINT8 *p, UINT8 len)
{
    tNFC_GET_ROUTING_REVT evt_data;
    UINT8       more;

    evt_data.status = NFC_STATUS_CONTINUE;

    if (nfc_cb.p_resp_cback)
    {
        more = *p++;
        evt_data.numEntries = *p++;

        //The TLV Data size is totalBufferlen - 1 byte(More) - 1 byte(NumEntries)
        evt_data.tlv_size = len - 2;


        //Copy TLV Data from buffer
        memcpy(&evt_data.param_tlvs, p, evt_data.tlv_size);

        //Copy pEntries
        //The status is OK only if the last NTF.
        if (more == NFC_LMRT_NTF_LAST_MESSAGE)
            evt_data.status = NFC_STATUS_OK;

        (*nfc_cb.p_resp_cback) (NFC_GET_ROUTING_REVT, (tNFC_RESPONSE *)&evt_data);
    }
}
#endif

/*******************************************************************************
**
** Function         nfc_ncif_proc_conn_create_rsp
**
** Description      This function is called to process connection create
**                  response
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_conn_create_rsp (UINT8 *p, UINT16 plen, UINT8 dest_type)
{
    tNFC_CONN_CB * p_cb;
    tNFC_STATUS    status;
    tNFC_CONN_CBACK *p_cback;
    tNFC_CONN   evt_data;
    UINT8           conn_id;

    /* find the pending connection control block */
    p_cb                = nfc_find_conn_cb_by_conn_id (NFC_PEND_CONN_ID);
    if (p_cb)
    {
        p                                  += NCI_MSG_HDR_SIZE;
        status                              = *p++;
        p_cb->buff_size                     = *p++;
        p_cb->num_buff = p_cb->init_credits = *p++;
        conn_id                             = *p++;
        evt_data.conn_create.status         = status;
        evt_data.conn_create.dest_type      = dest_type;
        evt_data.conn_create.id             = p_cb->id;
        evt_data.conn_create.buff_size      = p_cb->buff_size;
        evt_data.conn_create.num_buffs      = p_cb->num_buff;
        p_cback = p_cb->p_cback;
        if (status == NCI_STATUS_OK)
        {
            nfc_set_conn_id (p_cb, conn_id);
        }
        else
        {
            nfc_free_conn_cb (p_cb);
        }


        if (p_cback)
            (*p_cback) (conn_id, NFC_CONN_CREATE_CEVT, &evt_data);
    }
}

/*******************************************************************************
**
** Function         nfc_ncif_report_conn_close_evt
**
** Description      This function is called to report connection close event
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_report_conn_close_evt (UINT8 conn_id, tNFC_STATUS status)
{
    tNFC_CONN       evt_data;
    tNFC_CONN_CBACK *p_cback;
    tNFC_CONN_CB    *p_cb;

    p_cb = nfc_find_conn_cb_by_conn_id (conn_id);
    if (p_cb)
    {
        p_cback         = p_cb->p_cback;
        nfc_free_conn_cb (p_cb);
        evt_data.status = status;
        if (p_cback)
            (*p_cback) (conn_id, NFC_CONN_CLOSE_CEVT, &evt_data);
    }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_reset_rsp
**
** Description      This function is called to process reset response/notification
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_reset_rsp (UINT8 *p, BOOLEAN is_ntf)
{
    UINT8 status = *p++;
    static UINT32 initdelay = 0;
    if (is_ntf)
    {
        NFC_TRACE_ERROR1 ("reset notification!!:0x%x ", status);
        /* clean up, if the state is OPEN
         * FW does not report reset ntf right now */
        if (nfc_cb.nfc_state == NFC_STATE_OPEN)
        {
            /*if any conn_cb is connected, close it.
              if any pending outgoing packets are dropped.*/
            nfc_reset_all_conn_cbs ();
        }
        status = NCI_STATUS_OK;
    }

    if (nfc_cb.flags & (NFC_FL_RESTARTING|NFC_FL_POWER_CYCLE_NFCC))
    {
        nfc_reset_all_conn_cbs ();
    }

    /* NCI specification version comes in a CORE_RESET_RSP, check it. */
    if (!is_ntf && (status == NCI_STATUS_OK) && ((*p) != NCI_VERSION))
    {
        NFC_TRACE_ERROR2 ("nfc_ncif_proc_reset_rsp: **ERROR** NCI version mismatch!!:0x%02x != 0x%02x ", NCI_VERSION, *p);
        if ((*p) < NCI_VERSION_0_F)
        {
            NFC_TRACE_ERROR0 ("NFCC version is too old");
            status = NCI_STATUS_FAILED;
        }
    }

    if (status == NCI_STATUS_OK)
    {
        nci_snd_core_init ();
    }
    else
    {
        NFC_TRACE_ERROR0 ("Failed to reset NFCC");
        nfc_enabled (status, NULL);
    }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_init_rsp
**
** Description      This function is called to process init response
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_init_rsp (BT_HDR *p_msg)
{
    UINT8 *p, status, nfcc_feature_octet_fourth, orange_route_bit;
    tNFC_CONN_CB * p_cb = &nfc_cb.conn_cb[NFC_RF_CONN_ID];

    /* p_msg is already checked for not NULL in nfc_task */
    p = (UINT8 *) (p_msg + 1) + p_msg->offset;

    /* handle init params in nfc_enabled */
    status   = *(p + NCI_MSG_HDR_SIZE);
    if (status == NCI_STATUS_OK)
    {
        p_cb->id            = NFC_RF_CONN_ID;
        p_cb->act_protocol  = NCI_PROTOCOL_UNKNOWN;

        nfc_set_state (NFC_STATE_W4_POST_INIT_CPLT);

        nfc_cb.p_nci_init_rsp = p_msg;
        /* Get the 4th Byte of the NFCC feature in the core int response */
        nfcc_feature_octet_fourth = *(p + NCI_MSG_HDR_SIZE + NFCC_FEATURES_OCTET_FOURTH);
        /* check if the 2nd bit is set or not */
        /* this bit represents if NFCC supports Orange routing */
        orange_route_bit = nfcc_feature_octet_fourth & (1 << ORANGE_ROUTE_BIT);
        /* orange_route_bit value is greater than 0 if bit is set */
        if (orange_route_bit > 0)
        {
            //true if 2nd bit in 4th Byte of the NFCC feature is set
            nfa_dm_cb.orange_route = TRUE;
        }
        else
        {
            //false if 2nd bit in 4th Byte of the NFCC feature is set
            nfa_dm_cb.orange_route = FALSE;
        }
        NFC_TRACE_DEBUG0 ("nfc_ncif_proc_init_rsp(): HAL_SYNC: HAL Core initialised completing.");
        nfc_cb.p_hal->core_initialized (p);
    }
    else
    {
        nfc_enabled (status, NULL);
        GKI_freebuf (p_msg);
    }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_get_config_rsp
**
** Description      This function is called to process get config response
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_get_config_rsp (BT_HDR *p_evt)
{
    UINT8   *p;
    tNFC_RESPONSE_CBACK *p_cback = nfc_cb.p_resp_cback;
    tNFC_RESPONSE  evt_data;

    p_evt->offset += NCI_MSG_HDR_SIZE;
    p_evt->len    -= NCI_MSG_HDR_SIZE;
    if (p_cback)
    {
        p                                = (UINT8 *) (p_evt + 1) + p_evt->offset;
        evt_data.get_config.status       = *p++;
        evt_data.get_config.tlv_size     = p_evt->len;
        evt_data.get_config.p_param_tlvs = p;
        (*p_cback) (NFC_GET_CONFIG_REVT, &evt_data);
    }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_t3t_polling_ntf
**
** Description      Handle NCI_MSG_RF_T3T_POLLING NTF
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_t3t_polling_ntf (UINT8 *p, UINT16 plen)
{
    UINT8 status;
    UINT8 num_responses;

    /* Pass result to RW_T3T for processing */
    STREAM_TO_UINT8 (status, p);
    STREAM_TO_UINT8 (num_responses, p);
    plen-=NFC_TL_SIZE;
    rw_t3t_handle_nci_poll_ntf (status, num_responses, (UINT8) plen, p);
}

/*******************************************************************************
**
** Function         nfc_data_event
**
** Description      Report Data event on the given connection control block
**
** Returns          void
**
*******************************************************************************/
void nfc_data_event (tNFC_CONN_CB * p_cb)
{
    BT_HDR      *p_evt;
    tNFC_DATA_CEVT data_cevt;
    UINT8       *p;

    if (p_cb->p_cback)
    {
        while ((p_evt = (BT_HDR *)GKI_getfirst (&p_cb->rx_q)) != NULL)
        {
            if (p_evt->layer_specific & NFC_RAS_FRAGMENTED)
            {
#ifdef DTA // <DTA>
                if(!in_dta_mode() ) {
#endif // </DTA>
                /* Not the last fragment */
                if (!(p_evt->layer_specific & NFC_RAS_TOO_BIG))
                {
                    /* buffer can hold more */
                    if (  (p_cb->conn_id != NFC_RF_CONN_ID)
                        ||(nfc_cb.reassembly)  )
                    {
                        /* If not rf connection or If rf connection and reassembly requested,
                         * try to Reassemble next packet */
                        break;
                    }
                }
#ifdef DTA // <DTA>
                } else {
                    /* If in dta mode don't call callback, as in previous release.
                     * This is a hack, and we should make the code above work for
                     * both cases */
                    break;
                }
#endif // </DTA>

            }
            if(( p_evt = (BT_HDR *) GKI_dequeue (&p_cb->rx_q)) != NULL)
            {
                /* report data event */
                p_evt->offset   += NCI_MSG_HDR_SIZE;
                p_evt->len      -= NCI_MSG_HDR_SIZE;

            if (p_evt->layer_specific)
#ifdef DTA // <DTA>
                if(!in_dta_mode() )
#endif // </DTA>
                    data_cevt.status = NFC_STATUS_CONTINUE;
#ifdef DTA // <DTA>
                else
                    data_cevt.status = NFC_STATUS_BAD_LENGTH;
#endif // </DTA>
            else
            {
                nfc_cb.reassembly = TRUE;
                    data_cevt.status = NFC_STATUS_OK;
            }

                data_cevt.p_data = p_evt;
                /* adjust payload, if needed */
                if (p_cb->conn_id == NFC_RF_CONN_ID)
                {
                     /* if NCI_PROTOCOL_T1T/NCI_PROTOCOL_T2T/NCI_PROTOCOL_T3T, the status byte needs to be removed
                     */
                     if (((p_cb->act_protocol >= NCI_PROTOCOL_T1T) && (p_cb->act_protocol <= NCI_PROTOCOL_T3T))||
                         (p_cb->act_protocol == NCI_PROTOCOL_15693)
                        )
                     {
                         p_evt->len--;
                         p                = (UINT8 *) (p_evt + 1);
                        data_cevt.status = *(p + p_evt->offset + p_evt->len);
                     }
                }
                (*p_cb->p_cback) (p_cb->conn_id, NFC_DATA_CEVT, (tNFC_CONN *) &data_cevt);
                p_evt = NULL;
           }
        }
    }
}

/*******************************************************************************
**
** Function         nfc_ncif_proc_data
**
** Description      Find the connection control block associated with the data
**                  packet. Assemble the data packet, if needed.
**                  Report the Data event.
**
** Returns          void
**
*******************************************************************************/
void nfc_ncif_proc_data (BT_HDR *p_msg)
{
    UINT8   *pp, cid;
    tNFC_CONN_CB * p_cb;
    UINT8   pbf;
    BT_HDR  *p_last;
    UINT8   *ps, *pd;
    UINT16  size;
    BT_HDR  *p_max = NULL;
    UINT16  len;
    UINT16  error_mask = 0;

    pp   = (UINT8 *) (p_msg+1) + p_msg->offset;
    NFC_TRACE_DEBUG3 ("nfc_ncif_proc_data 0x%02x%02x%02x", pp[0], pp[1], pp[2]);
    NCI_DATA_PRS_HDR (pp, pbf, cid, len);
    p_cb = nfc_find_conn_cb_by_conn_id (cid);
    if (p_cb && (p_msg->len >= NCI_DATA_HDR_SIZE))
    {
        NFC_TRACE_DEBUG1 ("nfc_ncif_proc_data len:%d", len);
        if (len > 0)
        {
            p_msg->layer_specific       = 0;
            if (pbf)
                p_msg->layer_specific   = NFC_RAS_FRAGMENTED;
            p_last = (BT_HDR *)GKI_getlast (&p_cb->rx_q);
            if (p_last && (p_last->layer_specific & NFC_RAS_FRAGMENTED))
            {
                /* last data buffer is not last fragment, append this new packet to the last */
                size = GKI_get_buf_size(p_last);
                if (size < (BT_HDR_SIZE + p_last->len + p_last->offset + len))
                {
                    /* the current size of p_last is not big enough to hold the new fragment, p_msg */
                    if (size != GKI_MAX_BUF_SIZE)
                    {
                        /* try the biggest GKI pool */
                        p_max = (BT_HDR *)GKI_getpoolbuf (GKI_MAX_BUF_SIZE_POOL_ID);
                        if (p_max)
                        {
                            /* copy the content of last buffer to the new buffer */
                            memcpy(p_max, p_last, BT_HDR_SIZE);
                            pd  = (UINT8 *)(p_max + 1) + p_max->offset;
                            ps  = (UINT8 *)(p_last + 1) + p_last->offset;
                            memcpy(pd, ps, p_last->len);

                            /* place the new buffer in the queue instead */
                            GKI_remove_from_queue (&p_cb->rx_q, p_last);
                            GKI_freebuf (p_last);
                            GKI_enqueue (&p_cb->rx_q, p_max);
                            p_last  = p_max;
                        }
                    }
                    if (p_max == NULL)
                    {
                    /* Biggest GKI Pool not available (or)
                     * Biggest available GKI Pool is not big enough to hold the new fragment, p_msg */
                        p_last->layer_specific  |= NFC_RAS_TOO_BIG;
                        NFC_TRACE_ERROR1 ("nci_reassemble_msg buffer overrun(%d)!!", len);
                    }
                }

                ps   = (UINT8 *)(p_msg + 1) + p_msg->offset + NCI_MSG_HDR_SIZE;
                len  = p_msg->len - NCI_MSG_HDR_SIZE;
                if ((p_last->layer_specific & NFC_RAS_TOO_BIG) == 0)
                {
                    pd   = (UINT8 *)(p_last + 1) + p_last->offset + p_last->len;
                    memcpy(pd, ps, len);
                    p_last->len  += len;
                    /* do not need to update pbf and len in NCI header.
                     * They are stripped off at NFC_DATA_CEVT and len may exceed 255 */
                    NFC_TRACE_DEBUG1 ("nfc_ncif_proc_data len:%d", p_last->len);
                }

                error_mask              = (p_last->layer_specific & NFC_RAS_TOO_BIG);
                p_last->layer_specific  = (p_msg->layer_specific | error_mask);
                GKI_freebuf (p_msg);
#ifdef DISP_NCI
                if ((p_last->layer_specific & NFC_RAS_FRAGMENTED) == 0)
                {
                    /* this packet was reassembled. display the complete packet */
                    DISP_NCI ((UINT8 *)(p_last + 1) + p_last->offset, p_last->len, TRUE);
                }
#endif
            }
            else
            {
                /* if this is the first fragment on RF link */
                if (  (p_msg->layer_specific & NFC_RAS_FRAGMENTED)
                    &&(p_cb->conn_id == NFC_RF_CONN_ID)
                    &&(p_cb->p_cback)  )
                {
                    /* Indicate upper layer that local device started receiving data */
                    (*p_cb->p_cback) (p_cb->conn_id, NFC_DATA_START_CEVT, NULL);
                }
                /* enqueue the new buffer to the rx queue */
                GKI_enqueue (&p_cb->rx_q, p_msg);
            }
            nfc_data_event (p_cb);
            return;
        }
        /* else an empty data packet*/
    }
    GKI_freebuf (p_msg);
}

#endif /* NFC_INCLUDED == TRUE*/
