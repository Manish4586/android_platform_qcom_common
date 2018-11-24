/*============================================================================

  Copyright (c) 2013 Qualcomm Technologies, Inc. All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.

============================================================================*/
#include <unistd.h>
#include "camera_dbg.h"
#include "bcc44.h"
#include "isp_log.h"


#ifdef ENABLE_BCC_LOGGING
  #undef ISP_DBG
  #define ISP_DBG LOGE
#endif

/** util_bcc_cmd_debug
 *    @cmd: bcc config cmd
 *
 * This function dumps the bcc module configuration set to hw
 *
 * Return: nothing
 **/
static void util_bcc_cmd_debug(ISP_DemosaicDBCC_CmdType *cmd)
{
  ISP_DBG(ISP_MOD_BCC, "%s: cmd->fminThreshold = %d \n", __func__, cmd->fminThreshold);
  ISP_DBG(ISP_MOD_BCC, "%s: cmd->fmaxThreshold = %d \n", __func__, cmd->fmaxThreshold);
  ISP_DBG(ISP_MOD_BCC, "%s: cmd->rOffsetHi = %d \n", __func__, cmd->rOffsetHi);
  ISP_DBG(ISP_MOD_BCC, "%s: cmd->rOffsetLo = %d \n", __func__, cmd->rOffsetLo);
  ISP_DBG(ISP_MOD_BCC, "%s: cmd->bOffsetHi = %d \n", __func__, cmd->bOffsetHi);
  ISP_DBG(ISP_MOD_BCC, "%s: cmd->bOffsetLo = %d \n", __func__, cmd->bOffsetLo);
  ISP_DBG(ISP_MOD_BCC, "%s: cmd->grOffsetHi = %d \n", __func__, cmd->grOffsetHi);
  ISP_DBG(ISP_MOD_BCC, "%s: cmd->grOffsetLo = %d \n", __func__, cmd->grOffsetLo);
  ISP_DBG(ISP_MOD_BCC, "%s: cmd->gbOffsetHi = %d \n", __func__, cmd->gbOffsetHi);
  ISP_DBG(ISP_MOD_BCC, "%s: cmd->gbOffsetLo = %d \n", __func__, cmd->gbOffsetLo);
} /* util_bcc_cmd_debug */

/** bcc_reset
 *    @mod: bcc module struct data
 *
 * bcc module disable,release reg settings and strcuts
 *
 * Return: nothing
 **/
static void bcc_reset(isp_bcc_mod_t *mod)
{
  mod->old_streaming_mode = CAM_STREAMING_MODE_MAX;
  memset(&mod->RegCfgCmd, 0, sizeof(mod->RegCfgCmd));
  memset(&mod->RegCmd, 0, sizeof(mod->RegCmd));
  mod->aec_ratio = 0;
  memset(&mod->p_params, 0, sizeof(mod->p_params));
  memset(&mod->params, 0, sizeof(mod->params));
  mod->hw_update_pending = 0;
  mod->trigger_enable = 0; /* enable trigger update feature flag from PIX */
  mod->skip_trigger = 0;
  mod->enable = 0;         /* enable flag from PIX */
}

/** util_bcc_cmd_config
 *    @mod: bcc module struct data
 *
 * Copy from mod->chromatix params to reg cmd then configure
 *
 * Return: nothing
 **/
static void util_bcc_cmd_config(isp_bcc_mod_t *mod)
{
  mod->RegCfgCmd.enable = mod->enable;
  mod->RegCmd.fminThreshold = mod->p_params.Fmin;
  mod->RegCmd.fmaxThreshold = mod->p_params.Fmax;
  mod->RegCmd.rOffsetHi = mod->p_params.p_input_offset->bpc_4_offset_r_hi;
  mod->RegCmd.rOffsetLo = mod->p_params.p_input_offset->bpc_4_offset_r_lo;
  mod->RegCmd.bOffsetHi = mod->p_params.p_input_offset->bpc_4_offset_b_hi;
  mod->RegCmd.bOffsetLo = mod->p_params.p_input_offset->bpc_4_offset_b_lo;
  mod->RegCmd.grOffsetLo = mod->p_params.p_input_offset->bpc_4_offset_gr_lo;
  mod->RegCmd.grOffsetHi = mod->p_params.p_input_offset->bpc_4_offset_gr_hi;
  mod->RegCmd.gbOffsetLo = mod->p_params.p_input_offset->bpc_4_offset_gb_lo;
  mod->RegCmd.gbOffsetHi = mod->p_params.p_input_offset->bpc_4_offset_gb_hi;
} /* util_bcc_cmd_config */

/** bcc_do_hw_update
 *    @bcc_mod: bcc module struct data
 *
 * update bcc module register to kernel
 *
 * Return: nothing
 **/
static int bcc_do_hw_update(isp_bcc_mod_t *bcc_mod)
{
  int rc = 0;
  struct msm_vfe_cfg_cmd2 cfg_cmd;
  struct msm_vfe_reg_cfg_cmd reg_cfg_cmd[2];
  ISP_DemosaicDBPCCfg_CmdType cfg_mask;

  ISP_DBG(ISP_MOD_BCC, "%s: do hw update %d\n", __func__, bcc_mod->hw_update_pending);
  if (bcc_mod->hw_update_pending) {
    cfg_cmd.cfg_data = (void *) &bcc_mod->RegCmd;
    cfg_cmd.cmd_len = sizeof(bcc_mod->RegCmd);
    cfg_cmd.cfg_cmd = (void *) reg_cfg_cmd;
    cfg_cmd.num_cfg = 2;

    cfg_mask.enable = 1;
    reg_cfg_cmd[0].cmd_type = VFE_CFG_MASK;
    reg_cfg_cmd[0].u.mask_info.reg_offset = ISP_DBCC_DEMOSAIC_MIX_CFG_OFF;
    reg_cfg_cmd[0].u.mask_info.mask = cfg_mask.cfg;
    reg_cfg_cmd[0].u.mask_info.val = bcc_mod->RegCfgCmd.cfg;

    reg_cfg_cmd[1].u.rw_info.cmd_data_offset = 0;
    reg_cfg_cmd[1].cmd_type = VFE_WRITE;
    reg_cfg_cmd[1].u.rw_info.reg_offset = ISP_DBCC40_OFF;
    reg_cfg_cmd[1].u.rw_info.len = ISP_DBCC40_LEN * sizeof(uint32_t);

    rc = ioctl(bcc_mod->fd, VIDIOC_MSM_VFE_REG_CFG, &cfg_cmd);
    if (rc < 0){
      CDBG_ERROR("%s: HW update error, rc = %d", __func__, rc);
      return rc;
    }
    bcc_mod->hw_update_pending = 0;
  }

  /* TODO: update hw reg */
  return rc;
} /* bcc_do_hw_update */

/** bcc_init
 *    @mod_ctrl: bcc module control strcut
 *    @in_params: bcc hw module init params
 *    @notify_ops: fn pointer to notify other modules
 *
 *  bcc module data struct initialization
 *
 * Return: 0 always
 **/
static int bcc_init(void *mod_ctrl, void *in_params,
  isp_notify_ops_t *notify_ops)
{
  isp_bcc_mod_t *mod = mod_ctrl;
  isp_hw_mod_init_params_t *init_params = in_params;

  mod->fd = init_params->fd;
  mod->notify_ops = notify_ops;
  mod->old_streaming_mode = CAM_STREAMING_MODE_MAX;
  bcc_reset(mod);
  return 0;
} /* bcc_init */

/** bcc_config
 *    @mod: bcc module control strcut
 *    @in_params: contains chromatix ptr
 *    @in_param_size: in params struct size
 *
 *  bcc module configuration initial settings
 *
 * Return: 0 - success and negative value - failure
 **/
static int bcc_config(isp_bcc_mod_t *mod,
  isp_hw_pix_setting_params_t *in_params, uint32_t in_param_size)
{
  int  rc = 0;
  chromatix_parms_type *chromatix_ptr =
    (chromatix_parms_type *)in_params->chromatix_ptrs.chromatixPtr;
  chromatix_BPC_type *chromatix_BPC =
    &chromatix_ptr->chromatix_VFE.chromatix_BPC;

  if (in_param_size != sizeof(isp_hw_pix_setting_params_t)) {
    /* size mismatch */
    CDBG_ERROR("%s: size mismatch, expecting = %d, received = %d",
      __func__, sizeof(isp_hw_pix_setting_params_t), in_param_size);
    return -1;
  }

  ISP_DBG(ISP_MOD_BCC, "%s: enter", __func__);

  if (!mod->enable) {
    ISP_DBG(ISP_MOD_BCC, "%s: Mod not Enable.", __func__);
    return rc;
  }

  if ((chromatix_BPC->bcc_Fmin > chromatix_BPC->bcc_Fmax) ||
      (chromatix_BPC->bcc_Fmin_lowlight > chromatix_BPC->bcc_Fmax_lowlight)) {
    CDBG_ERROR("%s: Error min>max: %d/%d; %d/%d\n", __func__,
      chromatix_BPC->bcc_Fmin, chromatix_BPC->bcc_Fmax,
      chromatix_BPC->bcc_Fmin_lowlight, chromatix_BPC->bcc_Fmax_lowlight);
    return -1;
  }

  /* set old cfg to invalid value to trigger the first trigger update */
  mod->old_streaming_mode = CAM_STREAMING_MODE_MAX;

  mod->p_params.p_input_offset =
    &(chromatix_BPC->bcc_4_offset[BPC_NORMAL_LIGHT]);
  mod->p_params.Fmin = chromatix_BPC->bcc_Fmin;
  mod->p_params.Fmax = chromatix_BPC->bcc_Fmax;

  util_bcc_cmd_config(mod);

  mod->enable = TRUE;
  mod->trigger_enable = TRUE;
  mod->aec_ratio = 0;
  mod->skip_trigger = FALSE;
  mod->hw_update_pending = TRUE;

  return rc;
} /* bcc_config */

/** bcc_destroy
 *    @mod_ctrl: bcc module control strcut
 *
 *  Close bcc module
 *
 * Return: 0 always
 **/
static int bcc_destroy (void *mod_ctrl)
{
  isp_bcc_mod_t *mod = mod_ctrl;

  memset(mod,  0,  sizeof(isp_bcc_mod_t));
  free(mod);
  return 0;
} /* bcc_destroy */

/** bcc_enable
 *    @mod: bcc module control struct
 *    @enable: module enable/disable flag
 *
 *  bcc module enable/disable method
 *
 * Return: 0 - success and negative value - failure
 **/
static int bcc_enable(isp_bcc_mod_t *mod, isp_mod_set_enable_t *enable,
 uint32_t in_param_size)
{
  if (in_param_size != sizeof(isp_mod_set_enable_t)) {
    /* size mismatch */
    CDBG_ERROR("%s: size mismatch, expecting = %d, received = %d",
      __func__, sizeof(isp_mod_set_enable_t), in_param_size);
    return -1;
  }

  mod->enable = enable->enable;
  if (!mod->enable)
    mod->hw_update_pending = 0;
  return 0;
} /* bcc_enable */

/** bcc_trigger_enable
 *    @mod: bcc module control struct
 *    @enable: module enable/disable flag
 *    @in_param_size: input params struct size
 *
 *  bcc module enable hw update trigger feature
 *
 * Return: 0 - success and negative value - failure
 **/
static int bcc_trigger_enable(isp_bcc_mod_t *mod, isp_mod_set_enable_t *enable,
  uint32_t in_param_size)
{
  if (in_param_size != sizeof(isp_mod_set_enable_t)) {
    CDBG_ERROR("%s: size mismatch, expecting = %d, received = %d",
      __func__, sizeof(isp_mod_set_enable_t), in_param_size);
    return -1;
  }
  mod->trigger_enable = enable->enable;
  return 0;
} /* bcc_trigger_enable */

/** bcc_trigger_update
 *    @mod: bcc module control struct
 *    @in_params: input config params including chromatix ptr
 *    @in_param_size: input params struct size
 *
 *  bcc module modify reg settings as per new input params and
 *  trigger hw update
 *
 * Return: 0 - success and negative value - failure
 **/
static int bcc_trigger_update(isp_bcc_mod_t *mod,
  isp_pix_trigger_update_input_t *in_params, uint32_t in_param_size)
{
  int rc = 0;
  chromatix_parms_type *chromatix_ptr =
    in_params->cfg.chromatix_ptrs.chromatixPtr;
  chromatix_BPC_type *chromatix_BPC =
    &chromatix_ptr->chromatix_VFE.chromatix_BPC;
  chromatix_CS_MCE_type *chromatix_CS_MCE =
      &chromatix_ptr->chromatix_VFE.chromatix_CS_MCE;
  float aec_ratio;
  uint8_t update_bcc = TRUE;
  uint8_t Fmin, Fmin_lowlight, Fmax, Fmax_lowlight;
  tuning_control_type tunning_control;
  bpc_4_offset_type *bcc_normal_input_offset = NULL;
  bpc_4_offset_type *bcc_lowlight_input_offset = NULL;
  trigger_point_type *trigger_point = NULL;
  int is_snapmode, update_cs;

  if (in_param_size != sizeof(isp_pix_trigger_update_input_t)) {
    /* size mismatch */
    CDBG_ERROR("%s: size mismatch, expecting = %d, received = %d",
         __func__, sizeof(isp_pix_trigger_update_input_t), in_param_size);
    return -1;
  }

  if (!mod->enable || !mod->trigger_enable || mod->skip_trigger) {
    ISP_DBG(ISP_MOD_BCC, "%s: Skip Trigger update. enable %d, trig_enable %d, skip_trigger %d",
      __func__, mod->enable, mod->trigger_enable, mod->skip_trigger);
    return rc;
  }

  is_snapmode = IS_BURST_STREAMING(&in_params->cfg);
  if (!is_snapmode && !isp_util_aec_check_settled(
         &(in_params->trigger_input.stats_update.aec_update))) {
    ISP_DBG(ISP_MOD_BCC, "%s: AEC not settled", __func__);
    return rc;
  }

  ISP_DBG(ISP_MOD_BCC, "%s: Calculate table with AEC", __func__);

  if ((chromatix_BPC->bcc_Fmin > chromatix_BPC->bcc_Fmax) ||
      (chromatix_BPC->bcc_Fmin_lowlight > chromatix_BPC->bcc_Fmax_lowlight)) {
    CDBG_ERROR("%s: Error min>max: %d/%d; %d/%d\n", __func__,
      chromatix_BPC->bcc_Fmin, chromatix_BPC->bcc_Fmax,
      chromatix_BPC->bcc_Fmin_lowlight, chromatix_BPC->bcc_Fmax_lowlight);
    return -1;
  }

  trigger_point = &(chromatix_BPC->bcc_lowlight_trigger);
  Fmin = chromatix_BPC->bcc_Fmin;
  Fmax = chromatix_BPC->bcc_Fmax;
  Fmin_lowlight = chromatix_BPC->bcc_Fmin_lowlight;
  Fmax_lowlight = chromatix_BPC->bcc_Fmax_lowlight;
  bcc_normal_input_offset =
    &(chromatix_BPC->bcc_4_offset[BPC_NORMAL_LIGHT]);
  bcc_lowlight_input_offset =
    &(chromatix_BPC->bcc_4_offset[BPC_LOW_LIGHT]);

  tunning_control = chromatix_BPC->control_bcc;
  aec_ratio = isp_util_get_aec_ratio(mod->notify_ops->parent,
                chromatix_BPC->control_bcc, trigger_point,
                &(in_params->trigger_input.stats_update.aec_update),
                is_snapmode);

  update_cs = ((mod->old_streaming_mode != in_params->cfg.streaming_mode) ||
    !F_EQUAL(mod->aec_ratio, aec_ratio));

  if (!update_cs) {
    ISP_DBG(ISP_MOD_BCC, "%s: update not required", __func__);
    return 0;
  }

  if (F_EQUAL(aec_ratio, 0.0)) {
    ISP_DBG(ISP_MOD_BCC, "%s: Low Light \n", __func__);
    mod->p_params.p_input_offset = bcc_lowlight_input_offset;
    mod->p_params.Fmin = Fmin_lowlight;
    mod->p_params.Fmax = Fmax_lowlight;
    util_bcc_cmd_config(mod);
  } else if (F_EQUAL(aec_ratio, 1.0)) {
    ISP_DBG(ISP_MOD_BCC, "%s: Normal Light \n", __func__);
    mod->p_params.p_input_offset = bcc_normal_input_offset;
    mod->p_params.Fmin = Fmin;
    mod->p_params.Fmax = Fmax;
    util_bcc_cmd_config(mod);
  } else {
    ISP_DBG(ISP_MOD_BCC, "%s: Interpolate between Nomal and Low Light \n", __func__);
  /* Directly configure reg cmd.*/
    Fmin = (uint8_t)LINEAR_INTERPOLATION(Fmin, Fmin_lowlight, aec_ratio);
    mod->RegCmd.fminThreshold = Fmin;

    Fmax = (uint8_t)LINEAR_INTERPOLATION(Fmax, Fmax_lowlight, aec_ratio);
    mod->RegCmd.fmaxThreshold = Fmax;

    mod->RegCmd.rOffsetHi = (uint16_t)LINEAR_INTERPOLATION(
      bcc_normal_input_offset->bpc_4_offset_r_hi,
      bcc_lowlight_input_offset->bpc_4_offset_r_hi, aec_ratio);
    mod->RegCmd.rOffsetLo = (uint16_t)LINEAR_INTERPOLATION(
      bcc_normal_input_offset->bpc_4_offset_r_lo,
      bcc_lowlight_input_offset->bpc_4_offset_r_lo, aec_ratio);

    mod->RegCmd.bOffsetHi = (uint16_t)LINEAR_INTERPOLATION(
      bcc_normal_input_offset->bpc_4_offset_b_hi,
      bcc_lowlight_input_offset->bpc_4_offset_b_hi, aec_ratio);
    mod->RegCmd.bOffsetLo = (uint16_t)LINEAR_INTERPOLATION(
      bcc_normal_input_offset->bpc_4_offset_b_lo,
      bcc_lowlight_input_offset->bpc_4_offset_b_lo, aec_ratio);

    mod->RegCmd.grOffsetHi = (uint16_t)LINEAR_INTERPOLATION(
      bcc_normal_input_offset->bpc_4_offset_gr_hi,
      bcc_lowlight_input_offset->bpc_4_offset_gr_hi, aec_ratio);
    mod->RegCmd.grOffsetLo = (uint16_t)LINEAR_INTERPOLATION(
      bcc_normal_input_offset->bpc_4_offset_gr_lo,
      bcc_lowlight_input_offset->bpc_4_offset_gr_lo, aec_ratio);

    mod->RegCmd.gbOffsetHi = (uint16_t)LINEAR_INTERPOLATION(
      bcc_normal_input_offset->bpc_4_offset_gb_hi,
      bcc_lowlight_input_offset->bpc_4_offset_gb_hi, aec_ratio);
    mod->RegCmd.gbOffsetLo = (uint16_t)LINEAR_INTERPOLATION(
      bcc_normal_input_offset->bpc_4_offset_gb_lo,
      bcc_lowlight_input_offset->bpc_4_offset_gb_lo, aec_ratio);
  }
  mod->aec_ratio = aec_ratio;

  util_bcc_cmd_debug(&(mod->RegCmd));
  mod->hw_update_pending = TRUE;
  return rc;
} /* bcc_trigger_update */

/** bcc_set_params
 *    @mod_ctrl: bcc module control struct
 *    @param_id : param enum index
 *    @in_params: input config params based on param idex
 *    @in_param_size: input params struct size
 *
 *  set config params utility to update bcc module
 *
 * Return: 0 - success and negative value - failure
 **/
static int bcc_set_params (void *mod_ctrl, uint32_t param_id, void *in_params,
  uint32_t in_param_size)
{
  isp_bcc_mod_t *mod = mod_ctrl;
  int rc = 0;

  switch (param_id) {
  case ISP_HW_MOD_SET_MOD_ENABLE: {
    rc = bcc_enable(mod,
      (isp_mod_set_enable_t *) in_params, in_param_size);
  }
    break;

  case ISP_HW_MOD_SET_MOD_CONFIG: {
    rc = bcc_config(mod,
      (isp_hw_pix_setting_params_t *) in_params, in_param_size);
  }
    break;

  case ISP_HW_MOD_SET_TRIGGER_ENABLE: {
    rc = bcc_trigger_enable(mod,
      (isp_mod_set_enable_t *) in_params, in_param_size);
  }
    break;

  case ISP_HW_MOD_SET_TRIGGER_UPDATE: {
    rc = bcc_trigger_update(mod,
      (isp_pix_trigger_update_input_t *)in_params, in_param_size);
  }
    break;

  default: {
    rc = -EAGAIN;
  }
    break;
  }
  return rc;
} /* bcc_set_params */

/** bcc_get_params
 *    @mod_ctrl: bcc module control struct
 *    @param_id : param enum index
 *    @in_params: input config params based on param idex
 *    @in_param_size: input params struct size
 *    @out_params: struct to return out params
 *    @out_param_size: output params struct size
 *
 *  Get config params utility to fetch config of bcc module
 *
 * Return: 0 - success and negative value - failure
 **/
static int bcc_get_params (void *mod_ctrl, uint32_t param_id, void *in_params,
  uint32_t in_param_size, void *out_params, uint32_t out_param_size)
{
  isp_bcc_mod_t *mod = mod_ctrl;
  int rc = 0;

  switch (param_id) {
  case ISP_HW_MOD_GET_MOD_ENABLE: {
    isp_mod_get_enable_t *enable = out_params;

    if (sizeof(isp_mod_get_enable_t) != out_param_size) {
      CDBG_ERROR("%s: error, out_param_size mismatch, param_id = %d",
        __func__, param_id);
      break;
    }
    enable->enable = mod->enable;
  }
    break;

  default: {
    rc = -1;
  }
    break;
  }
  return rc;
} /* bcc_get_params */

/** bcc_action
 *    @mod_ctrl: bcc module control struct
 *    @action_code : action code
 *    @data: not used
 *    @data_size: not used
 *
 *  processing the hw action like update or reset
 *
 * Return: 0 - success and negative value - failure
 **/
static int bcc_action (void *mod_ctrl, uint32_t action_code, void *data,
  uint32_t data_size)
{
  int rc = 0;
  isp_bcc_mod_t *mod = mod_ctrl;

  switch (action_code) {
  case ISP_HW_MOD_ACTION_HW_UPDATE: {
    rc = bcc_do_hw_update(mod);
  }
    break;

  case ISP_HW_MOD_ACTION_RESET: {
    bcc_reset(mod);
  }
    break;

  default: {
    /* no op */
    CDBG_HIGH("%s: action code = %d is not supported. nop",
      __func__, action_code);
    rc = -EAGAIN;
  }
    break;
  }
  return rc;
} /* bcc_action */

/** bcc44_open
 *    @version: hw version
 *
 *  bcc 40 module open and create func table
 *
 * Return: bcc module ops struct pointer
 **/
isp_ops_t *bcc44_open(uint32_t version)
{
  isp_bcc_mod_t *mod = malloc(sizeof(isp_bcc_mod_t));

  ISP_DBG(ISP_MOD_BCC, "%s: E", __func__);

  if (!mod) {
    CDBG_ERROR("%s: fail to allocate memory",  __func__);
    return NULL;
  }
  memset(mod,  0,  sizeof(isp_bcc_mod_t));

  mod->ops.ctrl = (void *)mod;
  mod->ops.init = bcc_init;
  mod->ops.destroy = bcc_destroy;
  mod->ops.set_params = bcc_set_params;
  mod->ops.get_params = bcc_get_params;
  mod->ops.action = bcc_action;

  return &mod->ops;
} /* bcc44_open */

