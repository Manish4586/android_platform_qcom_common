/*============================================================================

  Copyright (c) 2013 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/
#ifndef __DEMOSAIC_BCC32_H__
#define __DEMOSAIC_BCC32_H__

#include "camera_dbg.h"
#include "isp_event.h"
#include "bcc32_reg.h"
#include "isp_hw_module_ops.h"
#include "isp_pipeline.h"
#include "isp_pipeline_util.h"
#include "chromatix.h"


typedef struct {
  bpc_4_offset_type *p_input_offset;
  uint8_t Fmin;
  uint8_t Fmax;
} header_params_t;

typedef struct {
  /* ISP Related*/
  int fd; //handle for module in session
  isp_ops_t ops;
  isp_notify_ops_t *notify_ops;
  cam_streaming_mode_t old_streaming_mode;

  /* Module Params*/
  ISP_DemosaicDBCCCfg_CmdType RegCfgCmd;
  ISP_DemosaicDBCC_CmdType RegCmd;
  ISP_DemosaicDBCC_CmdType applied_RegCmd;
  float aec_ratio;
  header_params_t p_params;

  /* Module Control */
  uint8_t hw_update_pending;
  uint8_t trigger_enable; /* enable trigger update feature flag from PIX */
  uint8_t skip_trigger;
  uint8_t enable;         /* enable flag from PIX */
} isp_bcc_mod_t;

#endif //__DEMOSAIC_BCC32_H__
