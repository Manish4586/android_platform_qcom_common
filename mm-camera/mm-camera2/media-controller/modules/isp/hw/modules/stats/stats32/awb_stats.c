/*============================================================================

  Copyright (c) 2013 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/

#include <stddef.h>
#include "awb_stats.h"
#include "isp_log.h"

#define AWB_SHIFT_BITS(n) ({ \
  uint32_t s_bits; \
  s_bits = CEIL_LOG2(n); \
  s_bits = (s_bits > 8) ? (s_bits-8) : 0; \
  s_bits;})


/* Stats AWB Config Command */
/* 1296 * 972 camif size.  16x16 regions. */
const struct ISP_StatsAwb_CfgCmdType ISP_DefaultStats32Awb_ConfigCmd = {
  /* reserved */
  0,   /* rgnHOffset */
  0,   /* rgnVOffset */
  1,   /* shiftBits */
  79,  /* rgnWidth  */
  59,  /* rgnHeight */
  /* reserved */
  15,  /*  rgnHNum */
  15,  /*  rgnVNum */
  241, /*  yMax    */
  10,  /*  yMin    */
  /* reserved */
  114,  /* c1  */
  /* reserved */
  136, /* c2  */
  /* reserved */
  -34, /* c3  */
  /* reserved */
  257, /* c4  */
  /* reserved */
  2, /* m1 */
  -16, /* m2 */
  16, /* m3 */
  -16, /* m4 */
  61,   /* t1 */
  32,   /* t2 */
  33,   /* t3 */
  64,   /* t6 */
  130,  /* t4 */
  /* reserved */
  157,  /* mg */
  /* reserved */
  64,  /* t5  */
};

/** awb_stats_debug:
 *    @pcmd: Pointer to statistic configuration.
 *
 * Print statistic configuration.
 *
 * This function executes in ISP thread context
 *
 * Return 0 on success.
 **/
static void awb_stats_debug(ISP_StatsAwb_CfgCmdType *pcmd)
{
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig shiftBits  %d\n", pcmd->shiftBits);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig rgnWidth   %d\n", pcmd->rgnWidth);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig rgnHeight  %d\n", pcmd->rgnHeight);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig rgnHOffset %d\n", pcmd->rgnHOffset);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig rgnVOffset %d\n", pcmd->rgnVOffset);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig rgnHNum    %d\n", pcmd->rgnHNum);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig rgnVNum    %d\n", pcmd->rgnVNum);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig yMax       %d\n", pcmd->yMax);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig yMin       %d\n", pcmd->yMin);

  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig t1         %d\n", pcmd->t1);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig t2         %d\n", pcmd->t2);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig t3         %d\n", pcmd->t3);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig t4         %d\n", pcmd->t4);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig mg         %d\n", pcmd->mg);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig t5         %d\n", pcmd->t5);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig t6         %d\n", pcmd->t6);

  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig m1         %d\n", pcmd->m1);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig m2         %d\n", pcmd->m2);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig m3         %d\n", pcmd->m3);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig m4         %d\n", pcmd->m4);

  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig c1         %d\n", pcmd->c1);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig c2         %d\n", pcmd->c2);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig c3         %d\n", pcmd->c3);
  ISP_DBG(ISP_MOD_STATS, "AWB statsconfig c4         %d\n", pcmd->c4);
}

/** isp_awb_stats_get_shiftbits:
 *    @entry: pointer to instance private data
 *    @p_shiftbits: return shift bits
 *
 * Get shift bits.
 *
 * This function executes in ISP thread context
 *
 * Return 0 on success.
 **/
static int isp_awb_stats_get_shiftbits(isp_stats_entry_t *entry,
  uint32_t *p_shiftbits)
{
  ISP_StatsAwb_CfgCmdType *cmd = entry->reg_cmd;
  *p_shiftbits = cmd->shiftBits;
  return 0;
}

/** awb_stats_enable:
 *    @entry: pointer to instance private data
 *    @in_params: input data
 *
 * Set enable.
 *
 * This function executes in ISP thread context
 *
 * Return 0 on success.
 **/
static int awb_stats_enable(isp_stats_entry_t *entry,
  isp_mod_set_enable_t *in_params)
{
  entry->enable = in_params->enable;
  return 0;
}

/** awb_stats_triger_enable:
 *    @entry: pointer to instance private data
 *    @in_params: input data
 *
 * Set trigger enable.
 *
 * This function executes in ISP thread context
 *
 * Return 0 on success.
 **/
static int awb_stats_triger_enable(isp_stats_entry_t *entry,
  isp_mod_set_enable_t *in_params)
{
  entry->trigger_enable = in_params->enable;
  return 0;
}

/** awb_stats_config:
 *    @entry: pointer to instance private data
 *    @pix_settings: input data
 *    @in_param_size: size of input data
 *
 * Configure submodule.
 *
 * This function executes in ISP thread context
 *
 * Return 0 on success.
 **/
static int awb_stats_config(isp_stats_entry_t *entry,
  isp_hw_pix_setting_params_t *pix_settings, uint32_t in_param_size)
{
  ISP_StatsAwb_CfgCmdType *pcmd = entry->reg_cmd;
  uint32_t pix_per_region;
  int rc = 0;
  uint32_t width, height;

  if (!entry->enable) {
    ISP_DBG(ISP_MOD_STATS, "%s: AWB stats not enabled", __func__);
    return 0;
  }

  entry->session_id = pix_settings->outputs->stream_param.session_id;
  entry->ion_fd = pix_settings->ion_fd;
  entry->hfr_mode = pix_settings->camif_cfg.hfr_mode;
  entry->comp_flag = 1;
  width = pix_settings->demosaic_output.last_pixel
    - pix_settings->demosaic_output.first_pixel + 1;
  pcmd->rgnWidth = ((width / (pcmd->rgnHNum + 1)) - 1);

  height = pix_settings->demosaic_output.last_line
    - pix_settings->demosaic_output.first_line + 1;
  pcmd->rgnHeight = ((height / (pcmd->rgnVNum + 1)) - 1);
  pix_per_region = (pcmd->rgnWidth + 1) * (pcmd->rgnHeight + 1);
  pcmd->shiftBits = 0;
#if 0 /* TODO */
  pcmd->yMax = params->awb_params.bounding_box.y_max;
  pcmd->yMin = params->awb_params.bounding_box.y_min;
  pcmd->t1 = params->awb_params.exterme_col_param.t1;
  pcmd->t2 = params->awb_params.exterme_col_param.t2;
  pcmd->t3 = params->awb_params.exterme_col_param.t3;
  pcmd->t4 = params->awb_params.exterme_col_param.t4;
  pcmd->t5 = params->awb_params.exterme_col_param.t5;
  pcmd->t6 = params->awb_params.exterme_col_param.t6;
  pcmd->mg = params->awb_params.exterme_col_param.mg;

  pcmd->c1 = params->awb_params.bounding_box.c1;
  pcmd->c2 = params->awb_params.bounding_box.c2;
  pcmd->c3 = params->awb_params.bounding_box.c3;
  pcmd->c4 = params->awb_params.bounding_box.c4;
  pcmd->m1 = params->awb_params.bounding_box.m1;
  pcmd->m2 = params->awb_params.bounding_box.m2;
  pcmd->m3 = params->awb_params.bounding_box.m3;
  pcmd->m4 = params->awb_params.bounding_box.m4;
#endif
  awb_stats_debug(pcmd);
  entry->hw_update_pending = 1;
  return 0;
}

/** awb_stats_triger_update:
 *    @entry: pointer to instance private data
 *    @pix_settings: input data
 *    @in_param_size: input data size
 *
 * Set trigger update.
 *
 * This function executes in ISP thread context
 *
 * Return 0 on success.
 **/
static int awb_stats_triger_update(isp_stats_entry_t *entry,
  isp_hw_pix_setting_params_t *pix_settings, uint32_t in_param_size)
{
  if (entry->trigger_enable == 0)
    return 0;

  return awb_stats_config(entry, pix_settings, in_param_size);
}

/** awb_stats_set_params:
 *    @mod_ctrl: pointer to instance private data
 *    @params_id: parameter ID
 *    @in_params: input data
 *    @in_params_size: size of input data
 *
 * Set parameter function. It handle all input parameters.
 *
 * This function executes in ISP thread context
 *
 * Return 0 on success.
 **/
static int awb_stats_set_params(void *ctrl, uint32_t param_id, void *in_params,
  uint32_t in_param_size)
{
  isp_stats_entry_t *entry = ctrl;
  int rc = 0;

  switch (param_id) {
  case ISP_STATS_SET_ENABLE:
    rc = awb_stats_enable(entry, (isp_mod_set_enable_t *)in_params);
    break;
  case ISP_STATS_SET_CONFIG:
    rc = awb_stats_config(entry, (isp_hw_pix_setting_params_t *)in_params,
      in_param_size);
    break;
  case ISP_STATS_SET_TRIGGER_ENABLE:
    rc = awb_stats_triger_enable(entry, (isp_mod_set_enable_t *)in_params);
    break;
  case ISP_STATS_SET_TRIGGER_UPDATE:
    rc = awb_stats_triger_update(entry,
      (isp_hw_pix_setting_params_t *)in_params, in_param_size);
    break;
  case ISP_STATS_SET_STREAM_CFG:
    break;
  case ISP_STATS_SET_STREAM_UNCFG:
    break;
  default:
    break;
  }

  return rc;
}

/** awb_stats_get_params:
 *    @mod_ctrl: pointer to instance private data
 *    @params_id: parameter ID
 *    @in_params: input data
 *    @in_params_size: size of input data
 *    @out_params: output data
 *    @out_params_size: size of output data
 *
 * Get parameter function. It handle all parameters.
 *
 * This function executes in ISP thread context
 *
 * Return 0 on success.
 **/
static int awb_stats_get_params(void *ctrl, uint32_t param_id, void *in_params,
  uint32_t in_param_size, void *out_params, uint32_t out_param_size)
{
  isp_stats_entry_t *entry = ctrl;
  int rc = 0;

  switch (param_id) {
  case ISP_STATS_GET_ENABLE:
    break;
  case ISP_STATS_GET_STREAM_STATE:
    break;
  case ISP_STATS_GET_PARSED_STATS:
    break;
  case ISP_STATS_GET_STREAM_HANDLE: {
    uint32_t *handle = (uint32_t *)(out_params);
    *handle = entry->stream_handle;
    break;
  }
  default:
    break;
  }

  return rc;
}

/** awb_stats_do_hw_update:
 *    @entry: pointer to instance private data
 *
 * Update hardware configuration.
 *
 * This function executes in ISP thread context
 *
 * Return 0 on success.
 **/
static int awb_stats_do_hw_update(isp_stats_entry_t *entry)
{
  int rc = 0;
  struct msm_vfe_cfg_cmd2 cfg_cmd;
  struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[1];

  if (entry->hw_update_pending) {
    cfg_cmd.cfg_data = (void *)entry->reg_cmd;
    cfg_cmd.cmd_len = sizeof(ISP_StatsAwb_CfgCmdType);
    cfg_cmd.cfg_cmd = (void *)reg_cfg_cmd;
    cfg_cmd.num_cfg = 1;

    reg_cfg_cmd[0].u.rw_info.cmd_data_offset = 0;
    reg_cfg_cmd[0].cmd_type = VFE_WRITE;
    reg_cfg_cmd[0].u.rw_info.reg_offset = AWB_STATS_OFF;
    reg_cfg_cmd[0].u.rw_info.len = AWB_STATS_LEN * sizeof(uint32_t);

    rc = ioctl(entry->fd, VIDIOC_MSM_VFE_REG_CFG, &cfg_cmd);
    if (rc < 0) {
      CDBG_ERROR("%s: HW update error, rc = %d", __func__, rc);
      return rc;
    }
    entry->hw_update_pending = 0;
  }

  return rc;
}

/** awb_stats_parse:
 *    @entry: pointer to instance private data
 *    @raw_buf: buffer with data for stats hw
 *    @bf_stats: output buffer to 3A module
 *
 * Parse BG statistics.
 *
 * This function executes in ISP thread context
 *
 * Return 0 on success.
 **/
static int awb_stats_parse(isp_stats_entry_t *entry, void *raw_buf,
  q3a_awb_stats_t *awb_stats)
{
  int32_t numRegions, i;
  uint32_t *current_region;
  uint32_t high_shift_bits;
  uint8_t inputNumReg;
  unsigned long *SCb, *SCr, *SY1, *NSCb;
  int rc = 0;

  SCb = awb_stats->SCb;
  SCr = awb_stats->SCr;
  SY1 = awb_stats->SY1;
  NSCb = awb_stats->NSCb;
  awb_stats->wb_region_h_num = 16;
  awb_stats->wb_region_v_num = 16;
  numRegions = awb_stats->wb_region_h_num * awb_stats->wb_region_v_num;

  /* Translate packed 4 32 bit word per region struct comming from the VFE
   * into more usable struct for microprocessor algorithms,
   * vfeStatDspOutput - up to 4k output of DSP from VFE block for AEC and
   AWB control */

  /* copy pointer to VFE stat 2 output region, plus 1 for header */
  current_region = raw_buf;
  for (i = 0; i < numRegions; i++) {
    /* Either 64 or 256 regions processed here */
    /* 16 bits sum of Y. */
    *SY1 = ((*(current_region)) & 0x01FFFFFF);
    SY1++;
    current_region++; /* each step is a 32 bit words or 32 bytes long */
    /* which is 2 of 16 bit words */
    *SCb = ((*(current_region)) & 0x01FFFFFF);
    SCb++;
    current_region++;
    *SCr = ((*(current_region)) & 0x01FFFFFF);
    SCr++;
    current_region++;
    /* NSCb counter should not left shifted by bitshift */
    *NSCb = ((*(current_region)) & 0x0001FFFF);
    NSCb++;
    current_region++;
  }
  awb_stats->awb_extra_stats.GLB_Y = *(current_region);
  current_region++;
  awb_stats->awb_extra_stats.GLB_Cb = *(current_region);
  current_region++;
  awb_stats->awb_extra_stats.GLB_Cr = *(current_region);
  current_region++;
  awb_stats->awb_extra_stats.GLB_N = ((*(current_region)) & 0x1FFFFFF);
  current_region++;
  awb_stats->awb_extra_stats.Green_r = *(current_region);
  current_region++;
  awb_stats->awb_extra_stats.Green_g = *(current_region);
  current_region++;
  awb_stats->awb_extra_stats.Green_b = *(current_region);
  current_region++;
  awb_stats->awb_extra_stats.Green_N = ((*current_region) & 0x1FFFFFF);
  current_region++;
  awb_stats->awb_extra_stats.ExtBlue_r = *(current_region);
  current_region++;
  awb_stats->awb_extra_stats.ExtBlue_g = *(current_region);
  current_region++;
  awb_stats->awb_extra_stats.ExtBlue_b = *(current_region);
  current_region++;
  awb_stats->awb_extra_stats.ExtBlue_N = ((*current_region) & 0x1FFFFFF);
  current_region++;
  awb_stats->awb_extra_stats.ExtRed_r = *(current_region);
  current_region++;
  awb_stats->awb_extra_stats.ExtRed_g = *(current_region);
  current_region++;
  awb_stats->awb_extra_stats.ExtRed_b = *(current_region);
  current_region++;
  awb_stats->awb_extra_stats.ExtRed_N = ((*current_region) & 0x1FFFFFF);
  return 0;
}

/** awb_stats_action:
 *    @mod_ctrl: pointer to instance private data
 *    @action_code: action id
 *    @data: action data
 *    @data_size: action data size
 *
 * Handle all actions.
 *
 * This function executes in ISP thread context
 *
 * Return 0 on success.
 **/
static int awb_stats_action(void *ctrl, uint32_t action_code, void *data,
  uint32_t data_size)
{
  int rc = 0;
  isp_stats_entry_t *entry = ctrl;

  switch ((isp_stats_action_code_t)action_code) {
  case ISP_STATS_ACTION_STREAM_START:
    break;
  case ISP_STATS_ACTION_STREAM_STOP:
    break;
  case ISP_STATS_ACTION_HW_CFG_UPDATE:
    rc = awb_stats_do_hw_update(entry);
    break;
  case ISP_STATS_ACTION_STATS_PARSE: {
    isp_pipeline_stats_parse_t *action_data = data;
    mct_event_stats_isp_t *isp_stats_event = action_data->parsed_stats_event;
    mct_event_stats_isp_data_t *stats_data = isp_stats_event->stats_data;
    int buf_idx =
      action_data->raw_stats_event->u.stats.stats_buf_idxs[MSM_ISP_STATS_AWB];
    void *raw_buf = isp_get_buf_addr(entry->buf_mgr,
      entry->buf_handle, buf_idx);
    if(!raw_buf){
      CDBG_ERROR("%s: isp_get_buf_addr failed!\n", __func__);
      return -1;
    }
    q3a_awb_stats_t *awb_stats = entry->parsed_stats_buf;
    isp_stats_event->stats_mask |= (1 << MSM_ISP_STATS_AWB);
    rc = awb_stats_parse(entry, raw_buf, awb_stats);
    if (entry->num_bufs != 0) {
      rc |= isp_stats_enqueue_buf(entry, buf_idx);
    }
    if (rc == 0) {
      stats_data[MSM_ISP_STATS_AWB].stats_buf = awb_stats;
      stats_data[MSM_ISP_STATS_AWB].stats_type = MSM_ISP_STATS_AWB;
      stats_data[MSM_ISP_STATS_AWB].buf_size = sizeof(q3a_awb_stats_t);
      stats_data[MSM_ISP_STATS_AWB].used_size = sizeof(q3a_awb_stats_t);
    } else {
      stats_data[MSM_ISP_STATS_AWB].stats_buf = NULL;
      stats_data[MSM_ISP_STATS_AWB].buf_size = 0;
      stats_data[MSM_ISP_STATS_AWB].used_size = 0;
    }
    break;
  }
  case ISP_STATS_ACTION_STREAM_BUF_CFG:
    rc = isp_stats_config_stats_stream(entry, ISP_STATS_AWB_BUF_NUM);
    break;
  case ISP_STATS_ACTION_STREAM_BUF_UNCFG:
    rc = isp_stats_unconfig_stats_stream(entry);
    break;
  default:
    break;
  }
  return rc;
}

/** awb_stats_init:
 *    @mod_ctrl: pointer to instance private data
 *    @in_params: input data
 *    @notify_ops: notify operations
 *
 * Initialize private data.
 *
 * This function executes in ISP thread context
 *
 * Return 0 on success.
 **/
static int awb_stats_init(void *ctrl, void *in_params,
  isp_notify_ops_t *notify_ops)
{
  isp_stats_entry_t *entry = ctrl;
  isp_hw_mod_init_params_t *init_params = in_params;
  ISP_StatsAwb_CfgCmdType *cmd = entry->reg_cmd;
  entry->buf_len = ISP_STATS_AWB_BUF_SIZE;
  entry->stats_type = MSM_ISP_STATS_AWB;
  entry->fd = init_params->fd;
  entry->notify_ops = notify_ops;
  entry->dev_idx = init_params->dev_idx;
  entry->buf_mgr = init_params->buf_mgr;

  *cmd = ISP_DefaultStats32Awb_ConfigCmd;

  return 0;
}

/** awb_stats_destroy:
 *    @ctrl: pointer to instance private data
 *
 * Free private resources.
 *
 * This function executes in ISP thread context
 *
 * Return 0 on success.
 **/
static int awb_stats_destroy(void *ctrl)
{
  isp_stats_entry_t *entry = ctrl;
  free(entry->reg_cmd);
  if (entry->parsed_stats_buf)
    free(entry->parsed_stats_buf);
  if (entry->private)
    free(entry->private);
  free(entry);
  return 0;
}

/** awb_stats32_open:
 *    @stats: isp module data
 *    @stats_type: statistic type
 *
 * Allocate instance private data for submodule.
 *
 * This function executes in ISP thread context
 *
 * Return pointer to struct which contain module operations.
 **/
isp_ops_t *awb_stats32_open(isp_stats_mod_t *stats,
  enum msm_isp_stats_type stats_type)
{
  int rc = 0;
  isp_stats_entry_t *entry = NULL;
  ISP_StatsAwb_CfgCmdType *cmd = NULL;
  uint32_t *acked_ymin = NULL;
  entry = malloc(sizeof(isp_stats_entry_t));
  if (!entry) {
    CDBG_ERROR("%s: no mem for aec\n", __func__);
    return NULL;
  }
  cmd = malloc(sizeof(ISP_StatsAwb_CfgCmdType));
  if (!cmd) {
    CDBG_ERROR("%s: no mem\n", __func__);
    free(entry);
    return NULL;
  }
  memset(entry, 0, sizeof(*entry));
  memset(cmd, 0, sizeof(*cmd));

  acked_ymin = malloc(sizeof(uint32_t));
  if (!acked_ymin) {
    CDBG_ERROR("%s: no mem\n", __func__);
    free(cmd);
    free(entry);
    return NULL;
  }
  *acked_ymin = 0;
  entry->parsed_stats_buf = malloc(sizeof(q3a_awb_stats_t));
  if (entry->parsed_stats_buf == NULL) {
    CDBG_ERROR("%s: no mem\n", __func__);
    free(acked_ymin);
    free(cmd);
    free(entry);
    return NULL;
  }
  entry->len_parsed_stats_buf = sizeof(q3a_awb_stats_t);
  entry->private = (void *)acked_ymin;
  entry->reg_cmd = cmd;
  entry->ops.ctrl = (void *)entry;
  entry->ops.init = awb_stats_init;
  entry->ops.destroy = awb_stats_destroy;
  entry->ops.set_params = awb_stats_set_params;
  entry->ops.get_params = awb_stats_get_params;
  entry->ops.action = awb_stats_action;
  return &entry->ops;
}


