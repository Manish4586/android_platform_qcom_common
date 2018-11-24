/*============================================================================
   Copyright (c) 2011-2012 Qualcomm Technologies, Inc.  All Rights Reserved.
   Qualcomm Technologies Proprietary and Confidential.
============================================================================*/

#include <stdlib.h>
#include <sys/ioctl.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "vfe.h"
#include "camera_dbg.h"
#include "vfe_config.h"

#ifdef _ANDROID_
#include <cutils/properties.h>
#endif

#ifdef ENABLE_VFE_LOGGING
  #undef CDBG
  #define CDBG LOGE
#endif

static vfe_comp_objs_t vfe_objs;
static vfe_status_t vfe_enable_modules(vfe_ctrl_info_t *vfe_ctrl_obj,
  int hw_write);
static vfe_status_t vfe_dynamic_enable_modules(vfe_ctrl_info_t *vfe_ctrl_obj);

/*===========================================================================
 * FUNCTION    - vfe_stats_stop -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: disables all the stats
 *==========================================================================*/
vfe_status_t vfe_stats_stop(vfe_ctrl_info_t *p_obj)
{
  int fd = p_obj->vfe_params.camfd;
  vfe_status_t status = VFE_SUCCESS;

  status = vfe_util_write_hw_cmd(fd, CMD_GENERAL, 0, 0,
    VFE_CMD_STATS_IHIST_STOP);
  if (VFE_SUCCESS != status)
    CDBG_ERROR("%s: VFE_CMD_STATS_IHIST_STOP failed %d\n",
      __func__, status);

  status = vfe_util_write_hw_cmd(fd, CMD_GENERAL, 0, 0,
    VFE_CMD_STATS_RS_STOP);
  if (VFE_SUCCESS != status)
    CDBG_ERROR("%s: VFE_CMD_STATS_RS_STOP failed %d\n",
      __func__, status);

  status = vfe_util_write_hw_cmd(fd, CMD_GENERAL, 0, 0,
    VFE_CMD_STATS_CS_STOP);
  if (VFE_SUCCESS != status)
    CDBG_ERROR("%s: VFE_CMD_STATS_CS_STOP failed %d\n",
      __func__, status);

  if(!vfe_use_bayer_stats(p_obj)) {

    status = vfe_util_write_hw_cmd(fd, CMD_GENERAL, 0, 0,
      VFE_CMD_STATS_AE_STOP);
    if (VFE_SUCCESS != status)
      CDBG_ERROR("%s: VFE_CMD_STATS_AE_STOP failed %d\n",
        __func__, status);

    status = vfe_util_write_hw_cmd(fd, CMD_GENERAL, 0, 0,
      VFE_CMD_STATS_AWB_STOP);
    if (VFE_SUCCESS != status)
      CDBG_ERROR("%s: VFE_CMD_STATS_AWB_STOP failed %d\n",
        __func__, status);

  } else {
    status = vfe_util_write_hw_cmd(fd, CMD_GENERAL, 0, 0,
      VFE_CMD_STATS_AWB_STOP);
    if (VFE_SUCCESS != status)
      CDBG_ERROR("%s: VFE_CMD_STATS_AWB_STOP failed %d\n",
        __func__, status);

    status = vfe_util_write_hw_cmd(fd, CMD_GENERAL, 0, 0,
      VFE_CMD_STATS_BE_STOP);
    if (VFE_SUCCESS != status)
      CDBG_ERROR("%s: VFE_CMD_STATS_BE_STOP failed %d\n",
        __func__, status);

    status = vfe_util_write_hw_cmd(fd, CMD_GENERAL, 0, 0,
      VFE_CMD_STATS_BG_STOP);
    if (VFE_SUCCESS != status)
      CDBG_ERROR("%s: VFE_CMD_STATS_BG_STOP failed %d\n",
        __func__, status);

     status = vfe_util_write_hw_cmd(fd, CMD_GENERAL, 0, 0,
       VFE_CMD_STATS_BHIST_STOP);
     if (VFE_SUCCESS != status)
       CDBG_ERROR("%s: VFE_CMD_STATS_BG_STOP failed %d\n",
         __func__, status);
  }
  return status;
}
/*===========================================================================
 * FUNCTION    - vfe_stats_init -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function initializes all the vfe modules.
 *==========================================================================*/
vfe_status_t vfe_stats_init(stats_mod_t *stats, vfe_params_t *params)
{
  int mod_id = 0;
  vfe_status_t rc = VFE_SUCCESS;

  if (VFE_SUCCESS != stats->awb_stats.ops.init(mod_id,
    &(stats->awb_stats), params)) {
      CDBG_ERROR("%s: vfe_awb_stats_init error, rc = %d",
        __func__, rc);
      return VFE_ERROR_GENERAL;
  }

  if (VFE_SUCCESS != stats->bg_stats.ops.init(mod_id,
    &(stats->bg_stats), params)) {
      CDBG_ERROR("%s: vfe_bg_stats_init error, rc = %d",
        __func__, rc);
      return VFE_ERROR_GENERAL;
  }

  if (VFE_SUCCESS != stats->be_stats.ops.init(mod_id,
    &(stats->be_stats), params)) {
      CDBG_ERROR("%s: vfe_be_stats_init error, rc = %d",
        __func__, rc);
      return VFE_ERROR_GENERAL;
  }

  if (VFE_SUCCESS != stats->bf_stats.ops.init(mod_id,
    &(stats->bf_stats), params)) {
      CDBG_ERROR("%s: vfe_bf_stats_init error, rc = %d",
        __func__, rc);
      return VFE_ERROR_GENERAL;
  }

  if (VFE_SUCCESS != stats->bhist_stats.ops.init(mod_id,
    &(stats->bhist_stats), params)) {
      CDBG_ERROR("%s: vfe_bhist_stats_init error, rc = %d",
      __func__, rc);
      return VFE_ERROR_GENERAL;
  }

  return VFE_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - vfe_modules_init -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This fucnction initializes all the vfe modules.
 *==========================================================================*/
vfe_status_t vfe_modules_init(vfe_module_t *vfe_module,
  vfe_params_t *params)
{
  vfe_status_t status = VFE_SUCCESS;
  int module_id = 0;
  /*VFE40  default using bayer*/
  vfe_module->stats.use_bayer_stats = 1;

  status = vfe_util_write_hw_cmd(params->camfd, CMD_GENERAL,
    &params->vfe_version, sizeof(params->vfe_version), VFE_CMD_GET_HW_VERSION);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: VFE_CMD_GET_HW_VERSION failed\n", __func__);
    return status;
  }
  CDBG("%s: vfe_version = 0x%x\n", __func__, params->vfe_version);

  if (IS_BAYER_FORMAT(params)) {
    status = vfe_module->linear_mod.ops.init(module_id,
      &(vfe_module->linear_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: linearization init failed\n", __func__);
      return status;
    }

    status = vfe_module->mce_mod.ops.init(module_id,
      &(vfe_module->mce_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: MCE init failed\n", __func__);
      return status;
    }

    status = vfe_module->chroma_enhan_mod.ops.init(module_id,
      &(vfe_module->chroma_enhan_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: chroma enhancment init failed\n", __func__);
      return status;
    }

    status = vfe_module->clamp_mod.ops.init(module_id,
      &(vfe_module->clamp_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: clamp init failed\n", __func__);
      return status;
    }

    status = vfe_module->chroma_supp_mod.ops.init(module_id,
      &(vfe_module->chroma_supp_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: chroma suppression init failed\n", __func__);
      return status;
    }

    status = vfe_module->wb_mod.ops.init(module_id,
      &(vfe_module->wb_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: white balance init failed\n", __func__);
      return status;
    }

    status = vfe_module->demosaic_mod.ops.init(module_id,
      &(vfe_module->demosaic_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: demosaic init failed\n", __func__);
      return status;
    }

    status = vfe_module->bpc_mod.ops.init(module_id,
      &(vfe_module->bpc_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: demosaic bpc init failed\n", __func__);
      return status;
    }

    status = vfe_module->bcc_mod.ops.init(module_id,
      &(vfe_module->bcc_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: demosaic bcc init failed\n", __func__);
      return status;
    }

    status = vfe_module->demux_mod.ops.init(module_id,
      &(vfe_module->demux_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: demux init failed\n", __func__);
      return status;
    }

    status = vfe_module->color_correct_mod.ops.init(module_id,
      &(vfe_module->color_correct_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: color correct init failed\n", __func__);
      return status;
    }

    status = vfe_module->abf_mod.ops.init(module_id,
      &(vfe_module->abf_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: ABF2 init failed\n", __func__);
      return status;
    }

    status = vfe_module->clf_mod.ops.init(module_id,
      &(vfe_module->clf_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: CLF init failed\n", __func__);
      return status;
    }

    status = vfe_module->gamma_mod.ops.init(module_id,
      &(vfe_module->gamma_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: gamma init failed\n", __func__);
      return status;
    }

    status = vfe_module->rolloff_mod.ops.init(module_id,
      &(vfe_module->rolloff_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: rolloff init failed\n", __func__);
      return status;
    }

    status = vfe_module->la_mod.ops.init(module_id,
      &(vfe_module->la_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: Luma Adaptation init failed\n", __func__);
      return status;
    }

    status = vfe_module->sce_mod.ops.init(module_id,
      &(vfe_module->sce_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: Skin Color Enhancement init failed\n", __func__);
      return status;
    }
  } else {
    status = vfe_module->clamp_mod.ops.init(module_id,
      &(vfe_module->clamp_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: clamp init failed\n", __func__);
      return status;
    }

    status = vfe_module->demux_mod.ops.init(module_id,
      &(vfe_module->demux_mod), params);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: demux init failed\n", __func__);
      return status;
    }

  }

  return status;
} /* vfe_modules_init */

/*===========================================================================
 * FUNCTION    - vfe_params_init -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This fucnction initializes all the default vfe params.
 *==========================================================================*/
vfe_status_t vfe_params_init(vfe_ctrl_info_t *vfe_obj, vfe_params_t *params)
{
  params->prev_spl_effect = CAMERA_EFFECT_OFF;
  params->bs_mode = CAMERA_BESTSHOT_OFF;
  params->cam_mode = CAM_MODE_2D;
  params->moduleCfg = &vfe_obj->vfe_op_cfg.op_cmd.moduleCfg;
  params->vfe_op_mode = VFE_OP_MODE_INVALID;
  params->flash_params.flash_mode = VFE_FLASH_NONE;
  params->awb_params.color_temp = 4000;
  params->wb = CAMERA_WB_AUTO;
  params->eztune_status = FALSE;
  params->use_default_config = FALSE;
  params->current_config = (IS_BAYER_FORMAT(params)) ? VIDEO_CONFIG :
    YUV_VIDEO_CONFIG;
  params->stats_config = (params->current_config & ~STATS_ALL_MASK);
  params->color_mod_config = (params->current_config & ~COLOR_MOD_ALL_MASK);
  params->prev_mode = VFE_OP_MODE_INVALID;
  params->flash_params.sensitivity_led_hi = 1;

  params->sharpness_info.asd_soft_focus_dgr = 1.0;
  params->sharpness_info.bst_soft_focus_dgr = 1.0;
  params->sharpness_info.ui_sharp_ctrl_factor = 1.0 / 3.9;
  params->sharpness_info.portrait_severity = 0.0;

  params->color_modules_disable = FALSE;
  params->use_cv_for_hue_sat = TRUE;
  params->crop_factor = 1<<12;
  params->vfe_hfr_mode = CAMERA_HFR_MODE_OFF;

  return VFE_SUCCESS;
}
/*===========================================================================
FUNCTION     - vfe_modules_deinit -

DESCRIPTION
===========================================================================*/
vfe_status_t vfe_modules_deinit(vfe_module_t *vfe_module, vfe_params_t *params)
{
  int mod_id = 0;
  vfe_status_t status = VFE_SUCCESS;

  status = vfe_module->linear_mod.ops.deinit(mod_id,
    &(vfe_module->linear_mod), params);
  if(status != VFE_SUCCESS) {
    CDBG_ERROR("%s: linear_mod de init failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  status = vfe_module->mce_mod.ops.deinit(mod_id,
    &(vfe_module->mce_mod), params);
  if(status != VFE_SUCCESS) {
    CDBG_ERROR("%s: mce_mod de init failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  status = vfe_module->sce_mod.ops.deinit(mod_id,
    &(vfe_module->sce_mod), params);
  if(status != VFE_SUCCESS) {
    CDBG_ERROR("%s: sce_mod de init failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  status = vfe_module->la_mod.ops.deinit(mod_id,
    &(vfe_module->la_mod), params);
  if(status != VFE_SUCCESS) {
    CDBG_ERROR("%s: la_mod de init failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  status = vfe_module->demosaic_mod.ops.deinit(mod_id,
    &(vfe_module->demosaic_mod), params);
  if(status != VFE_SUCCESS) {
    CDBG_ERROR("%s: demosaic_mod de init failed", __func__);
    return VFE_ERROR_GENERAL;
  }
  status = vfe_module->clf_mod.ops.deinit(mod_id,
    &(vfe_module->clf_mod), params);
  if(status != VFE_SUCCESS) {
    CDBG_ERROR("%s: clf_mod de init failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  status = vfe_module->rolloff_mod.ops.deinit(mod_id,
    &(vfe_module->rolloff_mod), params);
  if(status != VFE_SUCCESS) {
    CDBG_ERROR("%s: rolloff_mod de init failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  status = vfe_module->color_correct_mod.ops.deinit(mod_id,
    &(vfe_module->color_correct_mod), params);
  if(status != VFE_SUCCESS) {
    CDBG_ERROR("%s: color_correct_mod de init failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  status = vfe_module->wb_mod.ops.deinit(mod_id,
    &(vfe_module->wb_mod), params);
  if(status != VFE_SUCCESS) {
    CDBG_ERROR("%s: wb_mod de init failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  return VFE_SUCCESS;
} /* vfe_modules_deinit */

/*===========================================================================
 * FUNCTION    - vfe_cmd_hw_reg_update -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_cmd_hw_reg_update(vfe_ctrl_info_t *vfe_ctrl_obj,
  int* p_update)
{
  vfe_status_t status = VFE_SUCCESS;
  vfe_module_t *mod_obj = (vfe_module_t *)&vfe_ctrl_obj->vfe_module;
  vfe_params_t *params = (vfe_params_t *)&vfe_ctrl_obj->vfe_params;
  stats_mod_t *stats = (stats_mod_t *)&mod_obj->stats;
  int module_id = 0;
  params->update = 0;
  *p_update = FALSE;

#ifdef FEATURE_VFE_TEST_VECTOR
  if (vfe_ctrl_obj->test_vector_get_dump) {
    if (vfe_test_vector_enabled(&vfe_ctrl_obj->test_vector)) {
      status = vfe_test_vector_get_output(&vfe_ctrl_obj->test_vector);
      if (status != VFE_SUCCESS) {
        CDBG("%s: vfe_test_vector_get_output failed %d", __func__, status);
      }
      status = vfe_test_vector_validate(&vfe_ctrl_obj->test_vector);
      if (status != VFE_SUCCESS) {
        CDBG("%s: vfe_test_vector_validate failed %d", __func__, status);
      }
    }
  } else {
    vfe_ctrl_obj->test_vector_get_dump = TRUE;
  }
#endif

  status = vfe_dynamic_enable_modules(vfe_ctrl_obj);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: dynamic enable failed", __func__);
    return status;
  }

  status = mod_obj->linear_mod.ops.update(module_id,
             &(mod_obj->linear_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: linearization update failed", __func__);
    return status;
  }
  status = mod_obj->demux_mod.ops.update(module_id,
             &(mod_obj->demux_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: demux update failed", __func__);
    return status;
  }
  status = mod_obj->rolloff_mod.ops.update(module_id,
             &(mod_obj->rolloff_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: rolloff update failed", __func__);
    return status;
  }
  status = mod_obj->wb_mod.ops.update(module_id,
             &(mod_obj->wb_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: wb update failed", __func__);
    return status;
  }
  status = mod_obj->color_correct_mod.ops.update(module_id,
             &(mod_obj->color_correct_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: CC update failed", __func__);
    return status;
  }
  status = mod_obj->chroma_enhan_mod.ops.update(module_id,
             &(mod_obj->chroma_enhan_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: CV update failed", __func__);
    return status;
  }
  status = mod_obj->gamma_mod.ops.update(module_id,
             &(mod_obj->gamma_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Gamma update failed", __func__);
    return status;
  }
  status = mod_obj->clf_mod.ops.update(module_id,
             &(mod_obj->clf_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: CLF update failed", __func__);
    return status;
  }
  status = mod_obj->abf_mod.ops.update(module_id,
             &(mod_obj->abf_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: ABF update failed", __func__);
    return status;
  }
  status = stats->awb_stats.ops.update(module_id,
             &stats->awb_stats, params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: wb trigger update failed", __func__);
    return status;
  }

  mod_obj->scaler_mod.vfe_op_mode = vfe_ctrl_obj->vfe_out.vfe_operation_mode;
  status = mod_obj->scaler_mod.ops.update(module_id,
    &(mod_obj->scaler_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: SCALER update failed", __func__);
    return status;
  }

  status = mod_obj->fov_mod.ops.update(module_id,
    &(mod_obj->fov_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: FOV update failed", __func__);
    return status;
  }

  status = mod_obj->demosaic_mod.ops.update(module_id,
             &(mod_obj->demosaic_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: demosaic update failed", __func__);
    return status;
  }
  status = mod_obj->bpc_mod.ops.update(module_id,
             &(mod_obj->bpc_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: demosaic bpc update failed", __func__);
    return status;
  }
  status = mod_obj->bcc_mod.ops.update(module_id,
             &(mod_obj->bcc_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: demosaic bcc update failed", __func__);
    return status;
  }
  status = mod_obj->mce_mod.ops.update(module_id,
    &(mod_obj->mce_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: MCE update failed", __func__);
    return status;
  }
  status = mod_obj->sce_mod.ops.update(module_id,
             &(mod_obj->sce_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: SCE update failed", __func__);
    return status;
  }
  status = mod_obj->la_mod.ops.update(module_id,
             &(mod_obj->la_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: LA update failed", __func__);
    return status;
  }
  status = mod_obj->chroma_supp_mod.ops.update(module_id,
             &(mod_obj->chroma_supp_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Chroma Suppression update failed", __func__);
    return status;
  }

  if (params->update) {
    if(params->eztune_status)
      ez_vfe_diagnostics_update((void *)vfe_ctrl_obj);
    status = vfe_util_write_hw_cmd (params->camfd, CMD_GENERAL, NULL, 0,
      VFE_CMD_UPDATE);
    if (VFE_SUCCESS == status)
      *p_update = TRUE;
  }

  return VFE_SUCCESS;
} /* vfe_cmd_hw_reg_update */

/*===========================================================================
 * FUNCTION    - vfe_trigger_update_for_aec -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_trigger_update_for_aec(vfe_ctrl_info_t *vfe_ctrl_obj)
{
  /* Note: Since AWB is dependent on AEC, trigger update for AWB always
   *       comes after trigger update of AEC. So move all the vfe module
   *       triggers to AWB.
   */
  return VFE_SUCCESS;
} /* vfe_trigger_update_for_aec */

/*===========================================================================
 * FUNCTION    - vfe_trigger_update_for_awb -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_trigger_update_for_awb(vfe_ctrl_info_t *vfe_ctrl_obj)
{
  vfe_status_t status = VFE_SUCCESS;
  vfe_module_t *mod_obj = (vfe_module_t *)&vfe_ctrl_obj->vfe_module;
  vfe_params_t *params = (vfe_params_t *)&vfe_ctrl_obj->vfe_params;
  stats_mod_t *stats = (stats_mod_t *)&mod_obj->stats;
  int module_id = 0;

#ifdef FEATURE_VFE_TEST_VECTOR
  if (vfe_test_vector_enabled(&vfe_ctrl_obj->test_vector)) {
    status = vfe_test_vector_update_params(&vfe_ctrl_obj->test_vector);
    if (status != VFE_SUCCESS) {
      CDBG("%s: vfe_test_vector_update_params failed", __func__);
    }
  }
#endif
  if (params->awb_params.color_temp == 0)
    return status;

  status = mod_obj->linear_mod.ops.trigger_update(module_id,
             &(mod_obj->linear_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Linearization trigger update failed", __func__);
    return status;
  }
  status = mod_obj->rolloff_mod.ops.trigger_update(module_id,
             &(mod_obj->rolloff_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: rolloff trigger update failed", __func__);
    return status;
  }
  status = mod_obj->wb_mod.ops.trigger_update(module_id,
             &(mod_obj->wb_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: wb trigger update failed", __func__);
    return status;
  }
  /* WB trigger should be called before demosaic trigger */
  status = mod_obj->demosaic_mod.ops.trigger_update(module_id,
             &(mod_obj->demosaic_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Demosaic trigger update failed", __func__);
    return status;
  }
  status = mod_obj->bpc_mod.ops.trigger_update(module_id,
             &(mod_obj->bpc_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Demosaic BPC trigger update failed", __func__);
    return status;
  }
  status = mod_obj->bcc_mod.ops.trigger_update(module_id,
             &(mod_obj->bcc_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Demosaic BCC trigger update failed", __func__);
    return status;
  }
  status = mod_obj->abf_mod.ops.trigger_update(module_id,
             &(mod_obj->abf_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: ABF trigger update failed", __func__);
    return status;
  }
  status = stats->awb_stats.ops.trigger_update(module_id,
             &stats->awb_stats, params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: awb stats trigger update failed", __func__);
    return status;
  }
  status = mod_obj->la_mod.ops.trigger_update(module_id,
             &(mod_obj->la_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Luma Adaptation trigger update failed", __func__);
    return status;
  }
  status = mod_obj->demux_mod.ops.trigger_update(module_id,
             &(mod_obj->demux_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: demux trigger update failed", __func__);
    return status;
  }
  status = mod_obj->color_correct_mod.ops.trigger_update(module_id,
             &(mod_obj->color_correct_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: CC trigger update failed", __func__);
    return status;
  }
  status = mod_obj->chroma_enhan_mod.ops.trigger_update(module_id,
             &(mod_obj->chroma_enhan_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: CV trigger update failed", __func__);
    return status;
  }
  status = mod_obj->gamma_mod.ops.trigger_update(module_id,
             &(mod_obj->gamma_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Gamma trigger update failed", __func__);
    return status;
  }
  status = mod_obj->clf_mod.ops.trigger_update(module_id,
             &(mod_obj->clf_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: CLF trigger update failed", __func__);
    return status;
  }
  status = mod_obj->mce_mod.ops.trigger_update(module_id,
    &mod_obj->mce_mod, params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: MCE trigger update failed", __func__);
    return status;
  }
  status = mod_obj->sce_mod.ops.trigger_update(module_id,
             &(mod_obj->sce_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: SCE trigger update failed", __func__);
    return status;
  }
  status = mod_obj->chroma_supp_mod.ops.trigger_update(module_id,
             &(mod_obj->chroma_supp_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Chroma Suppression trigger update failed", __func__);
    return status;
  }
  return VFE_SUCCESS;
} /* vfe_trigger_update_for_awb */

/*===========================================================================
 * FUNCTION    - vfe_operation_config -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY:
 *==========================================================================*/
static void vfe_operation_config(vfe_ctrl_info_t *vfe_ctrl_obj)
{
  vfe_params_t *params = (vfe_params_t *)&(vfe_ctrl_obj->vfe_params);

  if (!IS_BAYER_FORMAT(params))
    vfe_ctrl_obj->vfe_op_cfg.op_cmd.moduleCfg.chromaUpsampleEnable = 1;
  else
    vfe_ctrl_obj->vfe_op_cfg.op_cmd.moduleCfg.chromaUpsampleEnable = 0;

  vfe_ctrl_obj->vfe_op_cfg.op_cmd.moduleCfg.realignmentBufEnable = 0;
  vfe_ctrl_obj->vfe_op_cfg.op_cmd.statisticsComposite = 1;

  if (IS_BAYER_FORMAT(params)) {
    vfe_ctrl_obj->vfe_op_cfg.op_cmd.vfeStatsCfg.colorConvEnable = 1;
    if(vfe_use_bayer_stats(vfe_ctrl_obj)) {
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.vfeStatsCfg.bayerHistSelect = 1;
    }
  } else
    vfe_ctrl_obj->vfe_op_cfg.op_cmd.vfeStatsCfg.colorConvEnable = 0;

  /* vfeCfg */
  CDBG_HIGH("%s: format %d", __func__,
    vfe_ctrl_obj->vfe_params.sensor_parms.vfe_camif_in_fmt);
  switch (vfe_ctrl_obj->vfe_params.sensor_parms.vfe_camif_in_fmt) {
    case CAMIF_BAYER_G_B:
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.vfeCfg.inputPixelPattern =
        VFE_BAYER_GBGBGB;
      break;
    case CAMIF_BAYER_B_G:
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.vfeCfg.inputPixelPattern =
        VFE_BAYER_BGBGBG;
      break;
    case CAMIF_BAYER_G_R:
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.vfeCfg.inputPixelPattern =
        VFE_BAYER_GRGRGR;
      break;
    case CAMIF_BAYER_R_G:
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.vfeCfg.inputPixelPattern =
        VFE_BAYER_RGRGRG;
      break;
    case CAMIF_YCBCR_Y_CB_Y_CR:
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.vfeCfg.inputPixelPattern =
        VFE_YUV_YCbYCr;
      break;
    case CAMIF_YCBCR_Y_CR_Y_CB:
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.vfeCfg.inputPixelPattern =
        VFE_YUV_YCrYCb;
      break;
    case CAMIF_YCBCR_CB_Y_CR_Y:
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.vfeCfg.inputPixelPattern =
        VFE_YUV_CbYCrY;
      break;
    case CAMIF_YCBCR_CR_Y_CB_Y:
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.vfeCfg.inputPixelPattern =
        VFE_YUV_CrYCbY;
      break;
    default:
      break;
  }

  CDBG_HIGH("%s:vfe_op_mode=%d\n", __func__, vfe_ctrl_obj->vfe_params.vfe_op_mode);
  /* operationMode */
  switch (vfe_ctrl_obj->vfe_params.vfe_op_mode) {
    case VFE_OP_MODE_PREVIEW:
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.operationMode =
        vfe_ctrl_obj->vfe_out.vfe_operation_mode;
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.hfrMode = CAMERA_HFR_MODE_OFF;
      break;
    case VFE_OP_MODE_SNAPSHOT:
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.operationMode =
        vfe_ctrl_obj->vfe_out.vfe_operation_mode;
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.statisticsComposite = 0;
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.hfrMode = CAMERA_HFR_MODE_OFF;
      break;
    case VFE_OP_MODE_VIDEO:
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.operationMode =
        vfe_ctrl_obj->vfe_out.vfe_operation_mode;
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.hfrMode =
        vfe_ctrl_obj->vfe_params.vfe_hfr_mode;
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.statisticsComposite =
        (vfe_ctrl_obj->vfe_op_cfg.op_cmd.hfrMode != CAMERA_HFR_MODE_OFF);
      break;
    case VFE_OP_MODE_RAW_SNAPSHOT:
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.operationMode =
        vfe_ctrl_obj->vfe_out.vfe_operation_mode;
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.statisticsComposite = 0;
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.hfrMode = CAMERA_HFR_MODE_OFF;
      break;
    case VFE_OP_MODE_JPEG_SNAPSHOT:
      CDBG_ERROR("%s: VFE_OP_MODE_JPEG_SNAPSHOT\n", __func__);
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.operationMode =
        vfe_ctrl_obj->vfe_out.vfe_operation_mode;
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.statisticsComposite = 0;
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.realignBufCfg.hsubEnable = 1;
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.realignBufCfg.vsubEnable = 1;
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.realignBufCfg.realignInputSel = 0;
      break;
    case VFE_OP_MODE_ZSL:
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.operationMode =
        vfe_ctrl_obj->vfe_out.vfe_operation_mode;
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.statisticsComposite = 0;
      vfe_ctrl_obj->vfe_op_cfg.op_cmd.hfrMode = CAMERA_HFR_MODE_OFF;
      break;
    default:
      CDBG_ERROR("%s: Invalid vfe_op_mode %d\n", __func__,
        vfe_ctrl_obj->vfe_params.vfe_op_mode);
      break;
  }


  if (VFE_SUCCESS != vfe_util_write_hw_cmd(vfe_ctrl_obj->vfe_params.camfd,
    CMD_GENERAL, (void *)&vfe_ctrl_obj->vfe_op_cfg.op_cmd,
    sizeof(vfe_ctrl_obj->vfe_op_cfg.op_cmd), VFE_CMD_OPERATION_CFG))
    CDBG_HIGH("%s: Operation config failed\n", __func__);
} /* vfe_operation_config */

/*===========================================================================
 * FUNCTION    - vfe_stats_config -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY:
 *==========================================================================*/
static vfe_status_t vfe_stats_config(stats_mod_t *stats, vfe_params_t *params)
{
  int8_t enable;
  int mod_id = 0;
  int rc = VFE_SUCCESS;
  if(IS_BAYER_FORMAT(params) && stats->use_bayer_stats) {
    int8_t hw_write = TRUE;

    enable = ((params->current_config & VFE_MOD_AWB_STATS) != 0);
    if (VFE_SUCCESS != stats->awb_stats.ops.enable(mod_id,
      &(stats->awb_stats), params, enable, FALSE)) {
        CDBG_HIGH("%s: vfe_awb_stats_enable error = %d", __func__, rc);
        return VFE_ERROR_GENERAL;
      }

    enable = ((params->current_config & VFE_MOD_BE_STATS) != 0);
    if (VFE_SUCCESS != stats->be_stats.ops.enable(mod_id,
      &(stats->be_stats), params, enable, hw_write)) {
      CDBG_HIGH("%s: vfe_be_stats_enable error = %d", __func__, rc);
    return VFE_ERROR_GENERAL;
    }

    enable = ((params->current_config & VFE_MOD_AEC_STATS) != 0);
    if (VFE_SUCCESS != stats->bg_stats.ops.enable(mod_id,
      &(stats->bg_stats), params, enable, hw_write)) {
      CDBG_HIGH("%s: vfe_bg_stats_enable error = %d", __func__, rc);
    return VFE_ERROR_GENERAL;
    }

    enable = ((params->current_config & VFE_MOD_AF_STATS) != 0);
    if (VFE_SUCCESS != stats->bf_stats.ops.enable(mod_id,
      &(stats->bf_stats), params, enable, hw_write)) {
      CDBG_HIGH("%s: vfe_bf_stats_enable error = %d", __func__, rc);
    return VFE_ERROR_GENERAL;
    }

    enable = ((params->current_config & VFE_MOD_BHIST_STATS) != 0);
    if (VFE_SUCCESS != stats->bhist_stats.ops.enable(mod_id,
      &(stats->bhist_stats), params, enable, hw_write)) {
      CDBG_HIGH("%s: vfe_bhist_stats_enable error = %d", __func__, rc);
    return VFE_ERROR_GENERAL;
    }

    if (VFE_SUCCESS != stats->awb_stats.ops.config(mod_id,
      &(stats->awb_stats), params)) {
      CDBG_HIGH("%s: vfe_awb_stats_config error = %d", __func__, rc);
      return VFE_ERROR_GENERAL;
    }

    if (VFE_SUCCESS != stats->be_stats.ops.config(mod_id,
      &(stats->be_stats), params)) {
      CDBG_HIGH("%s: vfe_be_stats_config error = %d", __func__, rc);
      return VFE_ERROR_GENERAL;
    }

    if (VFE_SUCCESS != stats->bg_stats.ops.config(mod_id,
      &(stats->bg_stats), params)) {
      CDBG_HIGH("%s: vfe_bg_stats_config error = %d", __func__, rc);
      return VFE_ERROR_GENERAL;
    }
    if (VFE_SUCCESS != stats->bhist_stats.ops.config(mod_id,
      &(stats->bhist_stats), params)) {
      CDBG_HIGH("%s: vfe_bhist_stats_config error = %d", __func__, rc);
      return VFE_ERROR_GENERAL;
    }
  }

  enable = ((params->current_config & VFE_MOD_IHIST_STATS) != 0);
  if (VFE_SUCCESS != stats->ihist_stats.ops.enable(mod_id,
    &(stats->ihist_stats), params, enable, FALSE))
    return VFE_ERROR_GENERAL;
  enable = ((params->current_config & VFE_MOD_RSCS_STATS) != 0);
  if (VFE_SUCCESS != stats->rs_stats.ops.enable(mod_id,
    &(stats->rs_stats), params, enable, FALSE))
    return VFE_ERROR_GENERAL;
  enable = ((params->current_config & VFE_MOD_RSCS_STATS) != 0);
  if (VFE_SUCCESS != stats->cs_stats.ops.enable(mod_id,
    &(stats->cs_stats), params, enable, FALSE))
    return VFE_ERROR_GENERAL;

  if (VFE_SUCCESS != stats->ihist_stats.ops.config(mod_id,
    &(stats->ihist_stats), params))
    return VFE_ERROR_GENERAL;

  if (VFE_SUCCESS != stats->rs_stats.ops.config(mod_id,
    &(stats->rs_stats), params))
    return VFE_ERROR_GENERAL;

  if (VFE_SUCCESS != stats->cs_stats.ops.config(mod_id,
    &(stats->cs_stats), params))
    return VFE_ERROR_GENERAL;

  return rc;
} /* vfe_stats_config */

/*===========================================================================
 * FUNCTION    - vfe_dynamic_enable_modules -
 *
 * DESCRIPTION:
 *==========================================================================*/
static vfe_status_t vfe_dynamic_enable_modules(vfe_ctrl_info_t *vfe_ctrl_obj)
{
  vfe_status_t status = VFE_SUCCESS;
  vfe_module_t *mod_obj = &(vfe_ctrl_obj->vfe_module);
  vfe_params_t* params = &vfe_ctrl_obj->vfe_params;
  uint32_t config = 0;

#ifdef _ANDROID_
  char value[PROPERTY_VALUE_MAX];

  property_get("persist.camera.vfe.config", value, "0");
  config = (uint32_t)strtoul(value, NULL, 16);

  if ((0 == config) || params->eztune_status) {
    return VFE_SUCCESS;
  }

  config |= BASE_CONFIG;
  if (!IS_BAYER_FORMAT(params))
    config &= STATS_ALL_MASK;

  if (params->current_config == config)
    return VFE_SUCCESS;

  CDBG_HIGH("%s: config new 0x%x old 0x%x", __func__, config,
    params->current_config);
  params->current_config = config;
  params->stats_config = (params->current_config & ~STATS_ALL_MASK);
  params->color_mod_config = (params->current_config & ~COLOR_MOD_ALL_MASK);
  status = vfe_enable_modules(vfe_ctrl_obj, TRUE);
#endif
  return status;
}/*vfe_dynamic_enable_modules*/

/*===========================================================================
 * FUNCTION    - vfe_enable_modules -
 *
 * DESCRIPTION:
 *==========================================================================*/
static vfe_status_t vfe_enable_modules(vfe_ctrl_info_t *vfe_ctrl_obj,
  int hw_write)
{
  vfe_status_t status = VFE_SUCCESS;
  vfe_module_t *mod_obj = &(vfe_ctrl_obj->vfe_module);
  stats_mod_t *stats = &mod_obj->stats;
  vfe_params_t* params = &vfe_ctrl_obj->vfe_params;
  current_output_info_t *vfe_out = &vfe_ctrl_obj->vfe_out;
  int module_id = 0;
  int8_t enable = FALSE;

  enable = ((params->current_config & VFE_MOD_LINEARIZATION) != 0);
  status = mod_obj->linear_mod.ops.enable(module_id,
    &(mod_obj->linear_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: linearization enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_FOV) != 0);
  status = mod_obj->fov_mod.ops.enable(module_id,
    &(mod_obj->fov_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: fov enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_FOV_ENC) != 0);
  status = mod_obj->fov_mod.ops.enable(module_id,
    &(mod_obj->fov_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: fov enable failed", __func__);
    return status;
  }
  enable = ((params->current_config & VFE_MOD_SCALER) != 0);
  mod_obj->scaler_mod.vfe_op_mode = vfe_ctrl_obj->vfe_out.vfe_operation_mode;
  status = mod_obj->scaler_mod.ops.enable(module_id,
    &(mod_obj->scaler_mod), params, enable, FALSE);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: scaler enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_SCALER_ENC) != 0);
  mod_obj->scaler_mod.vfe_op_mode = vfe_ctrl_obj->vfe_out.vfe_operation_mode;
  status = mod_obj->scaler_mod.ops.enable(module_id,
    &(mod_obj->scaler_mod), params, enable, FALSE);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: enc scaler enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_ROLLOFF) != 0);
  status = mod_obj->rolloff_mod.ops.enable(module_id,
    &(mod_obj->rolloff_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: rolloff enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_DEMOSAIC) != 0);
  status = mod_obj->demosaic_mod.ops.enable(module_id,
    &(mod_obj->demosaic_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Demosaic enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_BPC) != 0);
  status = mod_obj->bpc_mod.ops.enable(module_id,
    &(mod_obj->bpc_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Demosaic BPC enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_BCC) != 0);
  status = mod_obj->bcc_mod.ops.enable(module_id,
    &(mod_obj->bcc_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Demosaic BCC enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_DEMUX) != 0);
  status = mod_obj->demux_mod.ops.enable(module_id,
    &(mod_obj->demux_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: demux enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_WB) != 0);
  status = mod_obj->wb_mod.ops.enable(module_id,
    &(mod_obj->wb_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: wb enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_COLOR_CONV) != 0);
  status = mod_obj->chroma_enhan_mod.ops.enable(module_id,
    &(mod_obj->chroma_enhan_mod), params, enable, FALSE);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: CV enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_COLOR_CORRECT) != 0);
  status = mod_obj->color_correct_mod.ops.enable(module_id,
    &(mod_obj->color_correct_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: cc enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_GAMMA) != 0);
  status = mod_obj->gamma_mod.ops.enable(module_id,
    &(mod_obj->gamma_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: gamma enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_ABF) != 0);
  status = mod_obj->abf_mod.ops.enable(module_id,
    &(mod_obj->abf_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: abf enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_CLF) != 0);
  vfe_clf_enable_type_t clf_enable = (enable) ?
    VFE_CLF_LUMA_CHROMA_ENABLE : VFE_CLF_LUMA_CHROMA_DISABLE;
  status = mod_obj->clf_mod.ops.enable(module_id,
    &(mod_obj->clf_mod), params, clf_enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: clf enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_LA) != 0);
  status = mod_obj->la_mod.ops.enable(module_id,
    &(mod_obj->la_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: la enable failed", __func__);
    return status;
  }

  enable = ((params->current_config & VFE_MOD_MCE) != 0);
  status = mod_obj->mce_mod.ops.enable(module_id,
    &mod_obj->mce_mod, params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: MCE enable/disable failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  enable = ((params->current_config & VFE_MOD_SCE) != 0);
  status = mod_obj->sce_mod.ops.enable(module_id,
    &(mod_obj->sce_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: SCE enable/disable failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  enable = ((params->current_config & VFE_MOD_CHROMA_SUPPRESS) != 0);
  status = mod_obj->chroma_supp_mod.ops.enable(module_id,
    &(mod_obj->chroma_supp_mod), params, enable, hw_write);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Chroma suppression enable/disable failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  return status;
} /* vfe_enable_modules */

/*===========================================================================
 * FUNCTION    - vfe_reload_chromatix -
 *
 * DESCRIPTION: same as eztune reload chromatix function
 *==========================================================================*/
static vfe_status_t vfe_reload_chromatix(vfe_ctrl_info_t *vfe_ctrl_obj)
{
  vfe_status_t status = VFE_SUCCESS;
  CDBG("%s: \n",__func__);

  vfe_module_t *vfe_module = &(vfe_ctrl_obj->vfe_module);
  vfe_params_t *params = &(vfe_ctrl_obj->vfe_params);
  int mod_id = 0;

  status = vfe_module->rolloff_mod.ops.reload_params(mod_id,
    &(vfe_module->rolloff_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: rolloff_mod reload_params failed", __func__);
    return VFE_ERROR_GENERAL;
  }
  status = vfe_module->bpc_mod.ops.reload_params(mod_id,
    &(vfe_module->bpc_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: bpc_mod reload_params failed", __func__);
    return VFE_ERROR_GENERAL;
  }
  status = vfe_module->bcc_mod.ops.reload_params(mod_id,
    &(vfe_module->bcc_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: bcc_mod reload_params failed", __func__);
    return VFE_ERROR_GENERAL;
  }
  status = vfe_module->color_correct_mod.ops.reload_params(mod_id,
    &(vfe_module->color_correct_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: color_correct_mod reload_params failed", __func__);
    return VFE_ERROR_GENERAL;
  }
  status = vfe_module->chroma_enhan_mod.ops.reload_params(mod_id,
    &(vfe_module->chroma_enhan_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: chroma_enhan_mod reload_params failed", __func__);
    return VFE_ERROR_GENERAL;
  }
  status = vfe_module->gamma_mod.ops.reload_params(mod_id,
    &(vfe_module->gamma_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: gamma_mod reload_params failed", __func__);
    return VFE_ERROR_GENERAL;
  }
  status = vfe_module->abf_mod.ops.reload_params(mod_id,
    &(vfe_module->abf_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: abf_mod reload_params failed", __func__);
    return VFE_ERROR_GENERAL;
  }
  status = vfe_module->demosaic_mod.ops.reload_params(mod_id,
    &(vfe_module->demosaic_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: demosaic_mod reload_params failed", __func__);
    return VFE_ERROR_GENERAL;
  }
  status = vfe_module->demux_mod.ops.reload_params(mod_id,
    &(vfe_module->demux_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: demux_mod reload_params failed", __func__);
    return VFE_ERROR_GENERAL;
  }
  status = vfe_module->sce_mod.ops.reload_params(mod_id,
    &(vfe_module->sce_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: sce_mod reload_params failed", __func__);
    return VFE_ERROR_GENERAL;
  }
  return status;
} /* vfe_reload_chromatix */
/*===========================================================================
 * FUNCTION    - vfe_config_pipeline -
 *
 * DESCRIPTION:
 *==========================================================================*/
static vfe_status_t vfe_config_pipeline(vfe_ctrl_info_t *vfe_ctrl_obj)
{
  vfe_status_t status = VFE_SUCCESS;
  vfe_module_t *mod_obj = &(vfe_ctrl_obj->vfe_module);
  vfe_params_t* params = &vfe_ctrl_obj->vfe_params;
  int module_id = 0;

  /*enable the modules*/
  status = vfe_enable_modules(vfe_ctrl_obj, FALSE);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: enable modules failed", __func__);
    return status;
  }

  if (IS_BAYER_FORMAT(params)) {
    if(params->bs_mode != CAMERA_BESTSHOT_OFF) {
  if (vfe_reload_chromatix(vfe_ctrl_obj) != VFE_SUCCESS)
        CDBG_HIGH("%s: reload Fail",__func__);
    }
  }

  /*trigger update for 3A*/
  if ((params->vfe_op_mode == VFE_OP_MODE_SNAPSHOT ||
       params->vfe_op_mode == VFE_OP_MODE_JPEG_SNAPSHOT) &&
       IS_BAYER_FORMAT(params)) {
    status = vfe_trigger_update_for_aec(vfe_ctrl_obj);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: AWB trigger failed", __func__);
      return status;
    }

    status = vfe_trigger_update_for_awb(vfe_ctrl_obj);
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: AWB trigger failed", __func__);
      return status;
    }
  }

  mod_obj->scaler_mod.vfe_op_mode = vfe_ctrl_obj->vfe_out.vfe_operation_mode;
  status = mod_obj->scaler_mod.ops.config(module_id,
    &(mod_obj->scaler_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: SCALER config failed", __func__);
    return status;
  }

  status = mod_obj->fov_mod.ops.config(module_id,
    &(mod_obj->fov_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: FOV config failed", __func__);
    return status;
  }

  status = vfe_stats_config(&(vfe_ctrl_obj->vfe_module.stats), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: stats config failed", __func__);
    return status;
  }

  status = mod_obj->linear_mod.ops.config(module_id,
    &(mod_obj->linear_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: linearization config failed", __func__);
    return status;
  }

  status = mod_obj->demosaic_mod.ops.config(module_id,
    &(mod_obj->demosaic_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: demosaic config failed", __func__);
    return status;
  }

  status = mod_obj->bpc_mod.ops.config(module_id,
    &(mod_obj->bpc_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: demosaic BPC config failed", __func__);
    return status;
  }

  status = mod_obj->bcc_mod.ops.config(module_id,
    &(mod_obj->bcc_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: demosaic BCC config failed", __func__);
    return status;
  }

  status = mod_obj->chroma_enhan_mod.ops.config(module_id,
    &(mod_obj->chroma_enhan_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: color conversion config failed", __func__);
    return status;
  }

  status = mod_obj->chroma_supp_mod.ops.config(module_id,
    &(mod_obj->chroma_supp_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: chroma suppression config failed", __func__);
    return status;
  }
  status = mod_obj->mce_mod.ops.config(module_id,
    &mod_obj->mce_mod, params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: MCE config failed", __func__);
    return status;
  }

  status = mod_obj->rolloff_mod.ops.config(module_id,
    &(mod_obj->rolloff_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: rolloff config failed", __func__);
    return status;
  }

  status = mod_obj->demux_mod.ops.config(module_id,
    &(mod_obj->demux_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: demux config failed", __func__);
    return status;
  }

  status = mod_obj->wb_mod.ops.config(module_id,
    &(mod_obj->wb_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: wb config failed", __func__);
    return status;
  }

  status = mod_obj->color_correct_mod.ops.config(module_id,
    &(mod_obj->color_correct_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: cc config failed", __func__);
    return status;
  }

  status = mod_obj->gamma_mod.ops.config(module_id,
    &(mod_obj->gamma_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: gamma config failed", __func__);
    return status;
  }

  status = mod_obj->clf_mod.ops.config(module_id,
    &(mod_obj->clf_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: clf config failed", __func__);
    return status;
  }

  status = mod_obj->abf_mod.ops.config(module_id,
    &(mod_obj->abf_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: abf2 config failed", __func__);
    return status;
  }

  status = mod_obj->la_mod.ops.config(module_id,
    &(mod_obj->la_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: LA config failed", __func__);
    return status;
  }

  status = mod_obj->clamp_mod.ops.config(module_id,
    &(mod_obj->clamp_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: clamp config failed", __func__);
    return status;
  }

  status = mod_obj->sce_mod.ops.config(module_id,
    &(mod_obj->sce_mod), params);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: SCE config failed", __func__);
    return status;
  }
  vfe_operation_config(vfe_ctrl_obj);
  if(params->eztune_status)
    ez_vfe_diagnostics((void *)vfe_ctrl_obj);
  return VFE_SUCCESS;
} /* vfe_config_video_pipe */

/*===========================================================================
 * FUNCTION    - vfe_config_raw_snapshot_pipe -
 *
 * DESCRIPTION:
 *==========================================================================*/
static vfe_status_t vfe_config_raw_snapshot_pipe(vfe_ctrl_info_t *vfe_ctrl_obj)
{
  vfe_status_t status = VFE_SUCCESS;
  vfe_module_t *mod_obj = &(vfe_ctrl_obj->vfe_module);
  vfe_params_t* params = &vfe_ctrl_obj->vfe_params;

  vfe_operation_config(vfe_ctrl_obj);
  return VFE_SUCCESS;
} /* vfe_config_raw_snapshot_pipe */

/*===========================================================================
 * FUNCTION    - vfe_config_mode -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_config_mode(vfe_op_mode_t mode, vfe_ctrl_info_t *vfe_ctrl_obj)
{
  uint32_t clip_width, clip_height;
  vfe_status_t status = VFE_SUCCESS;
  vfe_params_t *params = &vfe_ctrl_obj->vfe_params;
  if(!vfe_ctrl_obj)
    return VFE_ERROR_GENERAL;

  vfe_ctrl_obj->vfe_params.prev_mode = vfe_ctrl_obj->vfe_params.vfe_op_mode;
  vfe_ctrl_obj->vfe_params.vfe_op_mode = mode;

  switch (mode) {
    case VFE_OP_MODE_PREVIEW:
      vfe_ctrl_obj->vfe_params.output2w =
        vfe_ctrl_obj->vfe_out.output[PRIMARY].image_width;
      vfe_ctrl_obj->vfe_params.output2h =
        vfe_ctrl_obj->vfe_out.output[PRIMARY].image_height;
      params->current_config = (IS_BAYER_FORMAT(params)) ? VIDEO_CONFIG :
        YUV_VIDEO_CONFIG;
      if (vfe_config_pipeline(vfe_ctrl_obj) != VFE_SUCCESS)
        goto error;
      break;

    case VFE_OP_MODE_VIDEO:
      vfe_ctrl_obj->vfe_params.output1w =
        vfe_ctrl_obj->vfe_out.output[SECONDARY].image_width +
        vfe_ctrl_obj->vfe_out.output[SECONDARY].extra_pad_width;
      vfe_ctrl_obj->vfe_params.output1h =
        vfe_ctrl_obj->vfe_out.output[SECONDARY].image_height +
        vfe_ctrl_obj->vfe_out.output[SECONDARY].extra_pad_height;
      vfe_ctrl_obj->vfe_params.output2w =
        vfe_ctrl_obj->vfe_out.output[PRIMARY].image_width;
      vfe_ctrl_obj->vfe_params.output2h =
        vfe_ctrl_obj->vfe_out.output[PRIMARY].image_height;
      if(params->use_default_config != TRUE)
        params->current_config |= params->stats_config;
      else
        params->current_config = (IS_BAYER_FORMAT(params)) ? VIDEO_CONFIG :
          YUV_VIDEO_CONFIG;
      CDBG_HIGH("%s: VideoCFg config %x", __func__, params->current_config);
      if (vfe_config_pipeline(vfe_ctrl_obj) != VFE_SUCCESS)
        goto error;

      break;

    case VFE_OP_MODE_SNAPSHOT:
    case VFE_OP_MODE_JPEG_SNAPSHOT:
      clip_width = vfe_ctrl_obj->vfe_params.sensor_parms.sensor_width;
      clip_height = vfe_ctrl_obj->vfe_params.sensor_parms.sensor_height;
      if ((vfe_ctrl_obj->vfe_out.output[SECONDARY].image_width +
           vfe_ctrl_obj->vfe_out.output[SECONDARY].extra_pad_width) > clip_width)
        vfe_ctrl_obj->vfe_params.output1w =
          clip_width & 0xFFF0;
      else
        vfe_ctrl_obj->vfe_params.output1w = vfe_ctrl_obj->vfe_out.output[SECONDARY].image_width +
           vfe_ctrl_obj->vfe_out.output[SECONDARY].extra_pad_width;
      if ((vfe_ctrl_obj->vfe_out.output[SECONDARY].image_height +
           vfe_ctrl_obj->vfe_out.output[SECONDARY].extra_pad_height) > clip_height)
        vfe_ctrl_obj->vfe_params.output1h = clip_height;
      else
        vfe_ctrl_obj->vfe_params.output1h = vfe_ctrl_obj->vfe_out.output[SECONDARY].image_height +
           vfe_ctrl_obj->vfe_out.output[SECONDARY].extra_pad_height;

      /* Output2 dimensions */
      if ((vfe_ctrl_obj->vfe_out.output[PRIMARY].image_width +
           vfe_ctrl_obj->vfe_out.output[PRIMARY].extra_pad_width) > clip_width)
        vfe_ctrl_obj->vfe_params.output2w = clip_width & 0xFFF0;
      else
        vfe_ctrl_obj->vfe_params.output2w = vfe_ctrl_obj->vfe_out.output[PRIMARY].image_width +
           vfe_ctrl_obj->vfe_out.output[PRIMARY].extra_pad_width;
      if ((vfe_ctrl_obj->vfe_out.output[PRIMARY].image_height +
           vfe_ctrl_obj->vfe_out.output[PRIMARY].extra_pad_height) > clip_height)
        vfe_ctrl_obj->vfe_params.output2h = clip_height;
      else
        vfe_ctrl_obj->vfe_params.output2h = vfe_ctrl_obj->vfe_out.output[PRIMARY].image_height +
           vfe_ctrl_obj->vfe_out.output[PRIMARY].extra_pad_height;

      if(params->use_default_config != TRUE)
        params->current_config &= STATS_ALL_MASK;
      else
        params->current_config = (IS_BAYER_FORMAT(params)) ? SNAPSHOT_CONFIG :
          YUV_SNAPSHOT_CONFIG;
      CDBG_HIGH("%s: SnapshotCfg config %x", __func__, params->current_config);
      if (vfe_config_pipeline(vfe_ctrl_obj) != VFE_SUCCESS)
        goto error;

#ifdef FEATURE_VFE_TEST_VECTOR
      if (vfe_test_vector_enabled(&vfe_ctrl_obj->test_vector)) {
        status = vfe_test_vector_get_output(&vfe_ctrl_obj->test_vector);
        if (status != VFE_SUCCESS) {
          CDBG("%s: vfe_test_vector_get_output failed %d", __func__, status);
        }
        status = vfe_test_vector_validate(&vfe_ctrl_obj->test_vector);
        if (status != VFE_SUCCESS) {
          CDBG("%s: vfe_test_vector_validate failed %d", __func__, status);
        }
      }
#endif
      break;

    case VFE_OP_MODE_RAW_SNAPSHOT:
      vfe_ctrl_obj->vfe_params.output2w = params->vfe_input_win.width;
      vfe_ctrl_obj->vfe_params.output2h = params->vfe_input_win.height;

      CDBG("raw output w=%d h=%d\n", vfe_ctrl_obj->vfe_params.output2w,
        vfe_ctrl_obj->vfe_params.output2h);
      if (vfe_config_raw_snapshot_pipe(vfe_ctrl_obj) != VFE_SUCCESS)
        goto error;
      break;

    case VFE_OP_MODE_ZSL:
      clip_width = vfe_ctrl_obj->vfe_params.sensor_parms.sensor_width;
      clip_height = vfe_ctrl_obj->vfe_params.sensor_parms.sensor_height;
      /* Output1 dimensions */
      /* Todo: If Snapshot size is smaller than preview */
      vfe_ctrl_obj->vfe_params.output1w = vfe_ctrl_obj->vfe_out.output[SECONDARY].image_width +
           vfe_ctrl_obj->vfe_out.output[SECONDARY].extra_pad_width;
      vfe_ctrl_obj->vfe_params.output1h = vfe_ctrl_obj->vfe_out.output[SECONDARY].image_height +
           vfe_ctrl_obj->vfe_out.output[SECONDARY].extra_pad_height;

      /* Output2 dimensions */
      if ((vfe_ctrl_obj->vfe_out.output[PRIMARY].image_width +
           vfe_ctrl_obj->vfe_out.output[PRIMARY].extra_pad_width) > clip_width)
        vfe_ctrl_obj->vfe_params.output2w = clip_width & 0xFFF0;
      else
        vfe_ctrl_obj->vfe_params.output2w = vfe_ctrl_obj->vfe_out.output[PRIMARY].image_width +
           vfe_ctrl_obj->vfe_out.output[PRIMARY].extra_pad_width;
      if ((vfe_ctrl_obj->vfe_out.output[PRIMARY].image_height +
           vfe_ctrl_obj->vfe_out.output[PRIMARY].extra_pad_height) > clip_height)
        vfe_ctrl_obj->vfe_params.output2h = clip_height;
      else
        vfe_ctrl_obj->vfe_params.output2h = vfe_ctrl_obj->vfe_out.output[PRIMARY].image_height +
           vfe_ctrl_obj->vfe_out.output[PRIMARY].extra_pad_height;

      if(params->use_default_config != TRUE)
        params->current_config |= params->stats_config;
      else
        params->current_config = (IS_BAYER_FORMAT(params)) ? ZSL_CONFIG :
        YUV_ZSL_CONFIG;
      if (vfe_config_pipeline(vfe_ctrl_obj) != VFE_SUCCESS)
        goto error;
      break;

    default:
      CDBG_ERROR("%s: Invalid mode :%d\n", __func__, mode);
      return VFE_ERROR_GENERAL;
  }
  CDBG("%s: Success fully Configured mode = %d \n", __func__, mode);
  return VFE_SUCCESS;

error:
  CDBG_ERROR("%s: Error while configuring mode = %d \n", __func__, mode);
  return VFE_ERROR_GENERAL;
} /* vfe_config_mode */

/*===========================================================================
 * FUNCTION    - vfe_query_stats_buf_ptr -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after init.
 *==========================================================================*/
vfe_status_t vfe_query_stats_buf_ptr(vfe_ctrl_info_t *p_obj,
  vfe_get_parm_t type, void *parm)
{
  vfe_status_t status = VFE_SUCCESS;
  stats_buffers_type_t *output = (stats_buffers_type_t *)parm;
  status = vfe_stats_parser_get_buf_ptr(
    &(p_obj->vfe_module.stats.parser_mod), &p_obj->vfe_params,
    &output);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: failed", __func__);
    return VFE_ERROR_GENERAL;
  }
  return status;
}
/*===========================================================================
 * FUNCTION    - vfe_query_shift_bits -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_query_shift_bits(vfe_ctrl_info_t *p_obj, vfe_get_parm_t type,
  void *parm)
{
  vfe_status_t status = VFE_SUCCESS;
  vfe_stats_output_t *output = (vfe_stats_output_t *)parm;

  switch (type) {
  case VFE_GET_IHIST_SHIFT_BITS:
      output->ihist_shift_bits =
        p_obj->vfe_module.stats.ihist_stats.ihist_stats_cmd.shiftBits;
      break;

  case VFE_GET_AWB_SHIFT_BITS:
      output->awb_shift_bits =
        p_obj->vfe_module.stats.awb_stats.VFE_StatsAwb_ConfigCmd.shiftBits;
      break;

    default:
      CDBG_ERROR("%s: Invalid mode \n", __func__);
      return VFE_ERROR_GENERAL;
      break;
  }
  return status;
}

/*===========================================================================
 * FUNCTION    - vfe_query_fov_crop -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_query_fov_crop(vfe_ctrl_info_t *p_obj, vfe_get_parm_t type,
  void *parm)
{

  switch (type) {
    case VFE_GET_FOV_CROP_PARM: {
      vfe_fov_crop_params_t *fov_crop = (vfe_fov_crop_params_t *)parm;
      fov_crop->first_pixel =
          p_obj->vfe_module.fov_mod.fov_enc_cmd.y_crop_cfg.firstPixel;
      fov_crop->last_pixel =
          p_obj->vfe_module.fov_mod.fov_enc_cmd.y_crop_cfg.lastPixel;
      fov_crop->first_line =
          p_obj->vfe_module.fov_mod.fov_enc_cmd.y_crop_cfg.firstLine;
      fov_crop->last_line =
          p_obj->vfe_module.fov_mod.fov_enc_cmd.y_crop_cfg.lastLine;
      break;
    }
    case VFE_GET_ZOOM_CROP_INFO: {
      vfe_zoom_crop_info_t *zoom_crop = (vfe_zoom_crop_info_t *)parm;
      zoom_crop->output1h = p_obj->vfe_params.output1h;
      zoom_crop->output2h = p_obj->vfe_params.output2h;
      zoom_crop->output1w = p_obj->vfe_params.output1w;
      zoom_crop->output2w = p_obj->vfe_params.output2w;
      zoom_crop->crop_win.x = p_obj->vfe_params.crop_info.crop_first_pixel;
      zoom_crop->crop_win.y = p_obj->vfe_params.crop_info.crop_first_line;
      zoom_crop->crop_win.crop_out_x = p_obj->vfe_params.crop_info.crop_out_x;
      zoom_crop->crop_win.crop_out_y = p_obj->vfe_params.crop_info.crop_out_y;
      break;
    }
    default:
      CDBG_ERROR("%s: Invalid mode \n", __func__);
      return VFE_ERROR_GENERAL;
      break;
  }
  return VFE_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - vfe_apply_hue_saturation -
 *
 * DESCRIPTION:
 *==========================================================================*/
int8_t vfe_apply_hue_saturation(vfe_effects_params_t *params)
{
  switch (params->spl_effect) {
    case CAMERA_EFFECT_SEPIA:
    case CAMERA_EFFECT_MONO:
    case CAMERA_EFFECT_NEGATIVE:
    case CAMERA_EFFECT_AQUA:
    case CAMERA_EFFECT_EMBOSS:
    case CAMERA_EFFECT_SKETCH:
      return FALSE;
    default:;
  }
  return TRUE;
}

/*===========================================================================
 * FUNCTION    - vfe_apply_contrast -
 *
 * DESCRIPTION:
 *==========================================================================*/
int8_t vfe_apply_contrast(vfe_effects_params_t *params)
{
  switch (params->spl_effect) {
    case CAMERA_EFFECT_POSTERIZE:
    case CAMERA_EFFECT_SOLARIZE:
      return FALSE;
    default:;
  }
  return TRUE;
}

/*===========================================================================
 * FUNCTION    - vfe_set_spl_effect -
 *
 * DESCRIPTION:
 *==========================================================================*/
vfe_status_t vfe_set_spl_effect(vfe_ctrl_info_t *vfe_ctrl_obj,
  vfe_params_t *params)
{
  vfe_module_t *mod_obj = (vfe_module_t *)&(vfe_ctrl_obj->vfe_module);
  int mod_id = 0;
  vfe_status_t status = VFE_SUCCESS;
  /* reset the old values */
  switch (params->prev_spl_effect) {
    case CAMERA_EFFECT_OFF: {
      break;
    }

    case CAMERA_EFFECT_EMBOSS:
    case CAMERA_EFFECT_SKETCH: {
      status = mod_obj->chroma_enhan_mod.ops.set_spl_effect(mod_id,
        &(mod_obj->chroma_enhan_mod), params, CAMERA_EFFECT_OFF);
      if (VFE_SUCCESS != status) {
        CDBG_HIGH("%s: CV set %d failed", __func__,
          params->effects_params.spl_effect);
        return VFE_ERROR_GENERAL;
      }
      /*TODO: no ASF in VFE*/
      /*status = mod_obj->asf_mod.ops.set_spl_effect(mod_id,
        &(mod_obj->asf_mod), params, CAMERA_EFFECT_OFF);
      if (VFE_SUCCESS != status) {
        CDBG_HIGH("%s: ASF set %d failed", __func__,
          params->effects_params.spl_effect);
        return VFE_ERROR_GENERAL;
      }*/
      break;
    }

    case CAMERA_EFFECT_NEON: {
      /*TODO: no asf in VFE*/
      /*status = mod_obj->asf_mod.ops.set_spl_effect(mod_id,
        &(mod_obj->asf_mod), params, CAMERA_EFFECT_OFF);
      if (VFE_SUCCESS != status) {
        CDBG_HIGH("%s: ASF reset %d failed", __func__,
          params->prev_spl_effect);
        return VFE_ERROR_GENERAL;
      }*/
      break;
    }

    case CAMERA_EFFECT_SEPIA:
    case CAMERA_EFFECT_MONO:
    case CAMERA_EFFECT_NEGATIVE:
    case CAMERA_EFFECT_AQUA: {
      status = mod_obj->chroma_enhan_mod.ops.set_spl_effect(mod_id,
        &(mod_obj->chroma_enhan_mod), params, CAMERA_EFFECT_OFF);
      if (VFE_SUCCESS != status) {
        CDBG_HIGH("%s: CV reset %d failed", __func__, params->prev_spl_effect);
        return VFE_ERROR_GENERAL;
      }
      break;
    }

    case CAMERA_EFFECT_POSTERIZE:
    case CAMERA_EFFECT_SOLARIZE: {
      status = mod_obj->la_mod.ops.set_spl_effect(mod_id,
        &(mod_obj->la_mod), params, CAMERA_EFFECT_OFF);
      if (VFE_SUCCESS != status) {
        CDBG_ERROR("%s: LA reset %d failed", __func__,
          params->prev_spl_effect);
        return VFE_ERROR_GENERAL;
      }
      break;
    }

    case CAMERA_EFFECT_BLACKBOARD:
    case CAMERA_EFFECT_WHITEBOARD: {
    /* TODO: Black Board & White Board effect will be done by LA*/
      status = mod_obj->gamma_mod.ops.set_effect(mod_id,
        &(mod_obj->gamma_mod), params, CAMERA_EFFECT_OFF);
      if (VFE_SUCCESS != status) {
        CDBG_HIGH("%s: Gamma reset %d failed", __func__,
          params->prev_spl_effect);
        return VFE_ERROR_GENERAL;
      }
      break;
    }
    default:;
  }

  /* apply the new effects */
  switch (params->effects_params.spl_effect) {
    case CAMERA_EFFECT_OFF: {
      status = vfe_color_modules_enable(vfe_ctrl_obj, TRUE);
      if (VFE_SUCCESS != status) {
        CDBG_HIGH("%s: Color modules disable %d failed", __func__,
          params->effects_params.spl_effect);
        return VFE_ERROR_GENERAL;
      }
      break;
    }

    case CAMERA_EFFECT_EMBOSS:
    case CAMERA_EFFECT_SKETCH: {
      status = mod_obj->chroma_enhan_mod.ops.set_spl_effect(mod_id,
        &(mod_obj->chroma_enhan_mod), params, CAMERA_EFFECT_MONO);
      if (VFE_SUCCESS != status) {
        CDBG_HIGH("%s: CV set %d failed", __func__,
          params->effects_params.spl_effect);
        return VFE_ERROR_GENERAL;
        }
      params->effects_params.hue = 0;
      params->effects_params.saturation = .5;
      /*TODO: no asf in VFE*/
      /*status = mod_obj->asf_mod.ops.set_spl_effect(mod_id,
        &(mod_obj->asf_mod), params, params->effects_params.spl_effect);
      if (VFE_SUCCESS != status) {
        CDBG_HIGH("%s: ASF set %d failed", __func__,
          params->effects_params.spl_effect);
        return VFE_ERROR_GENERAL;
      }*/
      break;
    }

    case CAMERA_EFFECT_NEON: {
      /*TODO: no asf in VFE*/
      /*status = mod_obj->asf_mod.ops.set_spl_effect(mod_id,
        &(mod_obj->asf_mod), params, params->effects_params.spl_effect);
      if (VFE_SUCCESS != status) {
        CDBG_HIGH("%s: ASF set %d failed", __func__,
          params->effects_params.spl_effect);
        return VFE_ERROR_GENERAL;
      }*/
      break;
    }

    case CAMERA_EFFECT_SEPIA:
    case CAMERA_EFFECT_MONO:
    case CAMERA_EFFECT_NEGATIVE:
    case CAMERA_EFFECT_AQUA: {
      status = mod_obj->chroma_enhan_mod.ops.set_spl_effect(mod_id,
        &(mod_obj->chroma_enhan_mod), params, params->effects_params.spl_effect);
      if (VFE_SUCCESS != status) {
        CDBG_HIGH("%s: CV set %d failed", __func__,
          params->effects_params.spl_effect);
        return VFE_ERROR_GENERAL;
      }
      params->effects_params.hue = 0;
      params->effects_params.saturation = .5;
      status = vfe_color_modules_enable(vfe_ctrl_obj, FALSE);
      if (VFE_SUCCESS != status) {
        CDBG_HIGH("%s: Color modules disable %d failed", __func__,
          params->effects_params.spl_effect);
        return VFE_ERROR_GENERAL;
      }
      break;
    }

    case CAMERA_EFFECT_POSTERIZE:
    case CAMERA_EFFECT_SOLARIZE: {
      status = mod_obj->la_mod.ops.set_spl_effect(mod_id,
        &(mod_obj->la_mod), params, params->effects_params.spl_effect);
      if (VFE_SUCCESS != status) {
        CDBG_ERROR("%s: LA set %d failed", __func__,
          params->prev_spl_effect);
        return VFE_ERROR_GENERAL;
      }
      break;
    }
    case CAMERA_EFFECT_BLACKBOARD:
    case CAMERA_EFFECT_WHITEBOARD: {
    /* TODO: Black Board & White Board effect will be done by LA */
      status = mod_obj->gamma_mod.ops.set_effect(mod_id,
        &(mod_obj->gamma_mod), params, params->effects_params.spl_effect);
      if (VFE_SUCCESS != status) {
        CDBG_HIGH("%s: Gamma set %d failed", __func__,
          params->effects_params.spl_effect);
        return VFE_ERROR_GENERAL;
      }
      status = vfe_color_modules_enable(vfe_ctrl_obj, FALSE);
      if (VFE_SUCCESS != status) {
        CDBG_HIGH("%s: Color modules disable %d failed", __func__,
          params->effects_params.spl_effect);
        return VFE_ERROR_GENERAL;
      }
      break;
    }
    default:;
  }
  params->prev_spl_effect = params->effects_params.spl_effect;

  CDBG("%s: return status =%d", __func__, status);
  return status;
}

/*===========================================================================
 * FUNCTION    - vfe_set_effect -
 *
 * DESCRIPTION:
 *==========================================================================*/
vfe_status_t vfe_set_bestshot(vfe_ctrl_info_t *vfe_ctrl_obj,
  camera_bestshot_mode_type mode)
{
  int mod_id = 0;
  vfe_status_t status = VFE_SUCCESS;
  vfe_params_t *params = &vfe_ctrl_obj->vfe_params;
  vfe_module_t *module = &vfe_ctrl_obj->vfe_module;
  if (mode == params->bs_mode) {
    CDBG_HIGH("%s: Bestshot mode not changed", __func__);
    return status;
  }
  params->bs_mode = mode;
  status = module->chroma_enhan_mod.ops.set_bestshot(mod_id,
    &(module->chroma_enhan_mod), params, mode);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: CV set bestshot failed", __func__);
    return status;
  }
  status = module->color_correct_mod.ops.set_bestshot(mod_id,
    &(module->color_correct_mod), params, mode);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: CC set bestshot failed", __func__);
    return status;
  }
  status = module->gamma_mod.ops.set_bestshot(mod_id, &module->gamma_mod,
    params, mode);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Gamma set bestshot failed", __func__);
    return status;
  }
  status = module->wb_mod.ops.set_bestshot(mod_id, &module->wb_mod,
    params, mode);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: CV set bestshot failed", __func__);
    return status;
  }


  status = module->la_mod.ops.set_bestshot(mod_id,
    &(module->la_mod), params, mode);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: La set bestshot failed", __func__);
    return status;
  }
  /* set correct SCE for certain modes*/
  status = module->sce_mod.ops.set_bestshot(mod_id,
    &(module->sce_mod), params, mode);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: SCE set bestshot failed", __func__);
    return status;
  }

  /* set correct MCE for landscape modes*/
  if (CAMERA_BESTSHOT_LANDSCAPE == mode)
    vfe_set_mce(vfe_ctrl_obj, TRUE);
  else
    vfe_set_mce(vfe_ctrl_obj, FALSE);

  return status;
} /* vfe_set_bestshot */

/*===========================================================================
 * FUNCTION    - vfe_set_effect -
 *
 * DESCRIPTION:
 *==========================================================================*/
vfe_status_t vfe_set_effect(vfe_ctrl_info_t *vfe_ctrl_obj,
  vfe_effects_type_t type,
  vfe_effects_params_t *params)
{
  vfe_status_t status = VFE_SUCCESS;
  vfe_params_t *vfe_params = &vfe_ctrl_obj->vfe_params;
  vfe_module_t *mod_obj = (vfe_module_t *)&vfe_ctrl_obj->vfe_module;
  int mod_id =0;

  switch (type) {
    case VFE_CONTRAST: {
      if (!vfe_apply_contrast(params)) {
        status = VFE_ERROR_INVALID_OPERATION;
        break;
      }
      status = mod_obj->gamma_mod.ops.set_contrast(mod_id,
        &(mod_obj->gamma_mod), vfe_params, (params->contrast * 10));
      if (VFE_SUCCESS != status)
        CDBG_HIGH("%s: Invalid operation", __func__);
      break;
    }
    case VFE_HUE: {
      if (!vfe_apply_hue_saturation(params)) {
        status = VFE_ERROR_INVALID_OPERATION;
        break;
      }
      status = mod_obj->chroma_enhan_mod.ops.set_effect(mod_id,
        &(mod_obj->chroma_enhan_mod), vfe_params, type);
      if (status != VFE_SUCCESS) {
        CDBG_ERROR("%s: cannot apply hue", __func__);
        return status;
      }

      if (!vfe_params->use_cv_for_hue_sat) {
        status = mod_obj->color_correct_mod.ops.set_effect(mod_id,
          &(mod_obj->color_correct_mod), vfe_params, type);
      }
      break;
    }
    case VFE_SATURATION: {
      if (!vfe_apply_hue_saturation(params)) {
        status = VFE_ERROR_INVALID_OPERATION;
        break;
      }
      status = mod_obj->chroma_enhan_mod.ops.set_effect(mod_id,
        &(mod_obj->chroma_enhan_mod), vfe_params, type);
      if (status != VFE_SUCCESS) {
        CDBG_ERROR("%s: cannot apply saturation", __func__);
        return status;
      }
      if (!vfe_params->use_cv_for_hue_sat) {
        status = mod_obj->color_correct_mod.ops.set_effect(mod_id,
          &(mod_obj->color_correct_mod), vfe_params, type);
        if (status != VFE_SUCCESS) {
          CDBG_ERROR("%s: cannot apply saturation", __func__);
          return status;
        }
      }
      break;
    }
    case VFE_SPL_EFFECT: {
      status = vfe_set_spl_effect(vfe_ctrl_obj, &vfe_ctrl_obj->vfe_params);
      break;
    }
    default:
      break;
  }
  CDBG("%s X",__func__);
  return status;
} /* vfe_set_effect */

/*===========================================================================
 * FUNCTION    - vfe_config_autofocus -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_config_autofocus(vfe_ctrl_info_t *vfe_ctrl_obj,
  vfe_stats_af_params_t *p_obj)
{
  vfe_status_t status = VFE_SUCCESS;
  bf_stats_t *bf_stats = &(vfe_ctrl_obj->vfe_module.stats.bf_stats);
  vfe_params_t *params = &(vfe_ctrl_obj->vfe_params);
  int mod_id = 0;

  status = vfe_af_calc_roi(vfe_ctrl_obj, p_obj);
  if(status != VFE_SUCCESS) {
    CDBG_ERROR("%s: failed af_calc_roi \n", __func__);
    return status;
  }
  if(IS_BAYER_FORMAT(params) &&
    vfe_ctrl_obj->vfe_module.stats.use_bayer_stats) {
    if (VFE_SUCCESS != bf_stats->ops.config(mod_id,
      bf_stats, params)) {
      CDBG("%s: vfe_bf_stats_config error \n", __func__);
      return VFE_ERROR_GENERAL;
    }
  }
  return VFE_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - vfe_set_manual_wb -
 *
 * DESCRIPTION:
 *==========================================================================*/
vfe_status_t vfe_set_manual_wb(vfe_ctrl_info_t *vfe_ctrl_obj)
{
  vfe_status_t status = VFE_SUCCESS;
  vfe_params_t *params = &vfe_ctrl_obj->vfe_params;
  vfe_module_t *module = &vfe_ctrl_obj->vfe_module;
  int mod_id = 0;

  status = module->wb_mod.ops.set_manual_wb(mod_id ,&module->wb_mod, params);
  if (VFE_SUCCESS != status) {
    CDBG("%s: Set manual WB failed %d", __func__, status);
    return status;
  }

  status = module->chroma_enhan_mod.ops.set_manual_wb(mod_id,
    &module->chroma_enhan_mod, params);
  return status;
}/*vfe_set_manual_wb*/

/*===========================================================================
 * FUNCTION    - vfe_update_scaler -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_update_scaler(vfe_ctrl_info_t *vfe_ctrl_obj,
  crop_window_info_t *fov_params)
{
  /* need more cleanup later to fully remove set_zoom_fov */
  /* in vfe40, update scaler for zoom instead of crop*/
  if(vfe_ctrl_obj->vfe_params.output2w > 0) {
    if (VFE_SUCCESS != vfe_set_scaler(&(vfe_ctrl_obj->vfe_params)))
      return VFE_ERROR_GENERAL;

    vfe_ctrl_obj->vfe_module.fov_mod.fov_update = TRUE;
    vfe_ctrl_obj->vfe_module.scaler_mod.scaler_update = TRUE;
    CDBG("%s: fov_update = %d, scaler_update = %d", __func__,
      vfe_ctrl_obj->vfe_module.fov_mod.fov_update,
      vfe_ctrl_obj->vfe_module.scaler_mod.scaler_update);
  }
  return VFE_SUCCESS;
}
/*===========================================================================
 * FUNCTION    - vfe_calc_pixel_crop_info -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_calc_pixel_crop_info(vfe_ctrl_info_t *p_obj, void *parm)
{
  pixel_crop_info_t *info = (pixel_crop_info_t *)parm;

  *info = p_obj->vfe_params.demosaic_op_params;

  return VFE_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - vfe_calc_pixel_crop_info -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_set_pixel_crop_info(vfe_ctrl_info_t *p_obj, void *parm)
{
  pixel_crop_info_t *info = (pixel_crop_info_t *)parm;
  vfe_sensor_params_t *sparams = &(p_obj->vfe_params.sensor_parms);
  uint32_t delta;
  uint32_t max_vfe_width = MAX_VFE_WIDTH;
  vfe_params_t *params = (vfe_params_t *)&(p_obj->vfe_params);

  sparams->firstPixel =  info->first_pixel;
  sparams->lastPixel =  info->last_pixel;
  sparams->firstLine =  info->first_line;
  sparams->lastLine =  info->last_line;

  if ((sparams->lastLine - sparams->firstLine + 1) > MAX_VFE_HEIGHT) {
    delta = ((sparams->lastLine - sparams->firstLine + 1) - MAX_VFE_HEIGHT) / 2;
    p_obj->vfe_params.demosaic_op_params.first_line =
      sparams->firstLine + delta - 1;
    p_obj->vfe_params.demosaic_op_params.last_line = sparams->lastLine - delta;
  } else {
    p_obj->vfe_params.demosaic_op_params.first_line = sparams->firstLine;
    p_obj->vfe_params.demosaic_op_params.last_line = sparams->lastLine;
  }

  if (!(IS_BAYER_FORMAT(params)))
    max_vfe_width *= 2;
  if ((sparams->lastPixel - sparams->firstPixel + 1) > max_vfe_width) {
    delta = ((sparams->lastPixel - sparams->firstPixel + 1) - max_vfe_width) / 2;
    p_obj->vfe_params.demosaic_op_params.first_pixel =
      sparams->firstPixel + delta - 1;
    p_obj->vfe_params.demosaic_op_params.last_pixel = sparams->lastPixel - delta;
  } else {
    p_obj->vfe_params.demosaic_op_params.first_pixel = sparams->firstPixel;
    p_obj->vfe_params.demosaic_op_params.last_pixel = sparams->lastPixel;
  }

  return VFE_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - vfe_calc_pixel_skip_info -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_calc_pixel_skip_info(vfe_ctrl_info_t *p_obj, void *parm)
{
  pixel_skip_info_t *info = (pixel_skip_info_t *)parm;
  info->pixel_skip = 1;
  info->line_skip = 1;
  return VFE_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - vfe_pp_info -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY:
 *==========================================================================*/
vfe_status_t vfe_pp_info(vfe_ctrl_info_t *p_obj, vfe_pp_params_t *pp_info)
{
  vfe_module_t *mod_obj = &(p_obj->vfe_module);
  vfe_params_t* params = &p_obj->vfe_params;
  vfe_status_t status = VFE_SUCCESS;

  status = vfe_gamma_get_table(&mod_obj->gamma_mod, params,
    &pp_info->gamma_num_entries, &pp_info->gamma_table);
  if(status != VFE_SUCCESS) {
    CDBG_HIGH("%s:GAMMA table update for WD failed \n",__func__);
    return VFE_ERROR_GENERAL;
  }

  status = vfe_la_get_table(&mod_obj->la_mod, params, pp_info);
  if(status != VFE_SUCCESS) {
    CDBG_HIGH("%s:LUMA table update for WD failed \n",__func__);
    return VFE_ERROR_GENERAL;
  }
  return status;
}

/*===========================================================================
 * FUNCTION    - vfe_query_rs_cs_parm -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY:
 *==========================================================================*/
vfe_status_t vfe_query_rs_cs_parm(vfe_ctrl_info_t *p_obj, vfe_get_parm_t type,
  void *parm)
{
  vfe_stats_rs_cs_params *out = (vfe_stats_rs_cs_params *)parm;
  vfe_stats_rs_cs_params *in = &(p_obj->vfe_params.rs_cs_params);

  memcpy(out, in, sizeof(vfe_stats_rs_cs_params));

  CDBG("%s: RS Stats Configurations\n", __func__);
  CDBG("%s: rs max v rgns = %d \n", __func__, out->rs_max_rgns);
  CDBG("%s: rs num rgns = %d \n", __func__, out->rs_num_rgns);
  CDBG("%s: rs shiftBits = %d \n", __func__, out->rs_shift_bits);
  CDBG("%s: CS Stats Configurations\n", __func__);
  CDBG("%s: cs max h rgns = %d \n", __func__, out->cs_max_rgns);
  CDBG("%s: cs num rgns = %d \n", __func__, out->cs_num_rgns);
  CDBG("%s: cs shiftBits = %d \n", __func__, out->cs_shift_bits);

  return VFE_SUCCESS;
} /* vfe_query_rs_cs_parm */

/*===========================================================================
 * FUNCTION    - vfe_set_mce -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_set_mce(vfe_ctrl_info_t *vfe_ctrl_obj, uint32_t enable)
{
  vfe_status_t status = VFE_SUCCESS;
  int module_id = 0;
  uint32_t hw_enable;

  if(vfe_ctrl_obj->vfe_params.effects_params.spl_effect !=
    CAMERA_EFFECT_OFF) {
    CDBG_HIGH("%s: MCE disabled when Effects enabled\n",__func__);
    return VFE_SUCCESS;
  }

  hw_enable = ((vfe_ctrl_obj->vfe_params.current_config & VFE_MOD_MCE) != 0);

  CDBG("%s:HAL enable: %d, Module_cfg hw_enable = %d\n", __func__, enable, hw_enable);

  enable = ((enable & hw_enable) != 0);

  CDBG("%s: enable = %d\n", __func__, enable);
  status = vfe_ctrl_obj->vfe_module.mce_mod.ops.enable(module_id,
    &(vfe_ctrl_obj->vfe_module.mce_mod), &(vfe_ctrl_obj->vfe_params),
    enable, FALSE);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: mce enable from UI failed", __func__);
    return status;
  }
  return VFE_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - vfe_set_sce -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_set_sce(vfe_ctrl_info_t *vfe_ctrl_obj, int32_t sce_factor)
{
  vfe_status_t status = VFE_SUCCESS;

  CDBG("%s: sce_factor from UI: %d\n", __func__, sce_factor);

  status = vfe_sce_setup(&(vfe_ctrl_obj->vfe_module.sce_mod),
    &(vfe_ctrl_obj->vfe_params), sce_factor);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: sce factor adjustment from UI failed", __func__);
    return status;
  }
  return VFE_SUCCESS;
}
/*===========================================================================
 * FUNCTION    - vfe_set_output_info -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_set_output_info(vfe_ctrl_info_t *vfe_ctrl_obj,
                                 void *parm)
{
  vfe_status_t status = VFE_SUCCESS;
  current_output_info_t *vfe_out = (current_output_info_t *)parm;
  CDBG("%s: vfe output mode %d \n", __func__, vfe_out->vfe_operation_mode);

  if (vfe_out != NULL){
    vfe_ctrl_obj->vfe_out = *vfe_out;
    vfe_ctrl_obj->vfe_params.enc_format =
      vfe_ctrl_obj->vfe_out.output[SECONDARY].format;
  } else
    status = VFE_ERROR_GENERAL;
  return status;
}

/*===========================================================================
 * FUNCTION    - vfe_color_modules_enable -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_color_modules_enable(vfe_ctrl_info_t *vfe_ctrl_obj,
  uint8_t mod_enable)
{
  vfe_status_t status = VFE_SUCCESS;
  int enable = FALSE;
  int module_id = 0;
  vfe_params_t *params = &(vfe_ctrl_obj->vfe_params);
  CDBG("%s: Enable : %d color 0x%x cfg 0x%x\n",__func__, mod_enable,
    params->color_mod_config, params->current_config);

  if (!mod_enable) {
    params->color_mod_config = params->current_config &
      ~COLOR_MOD_ALL_MASK;
    params->current_config &= COLOR_MOD_ALL_MASK;
  } else {
    params->current_config |= params->color_mod_config;
  }

  CDBG("%s: Enable : %d color 0x%x cfg 0x%x\n",__func__, mod_enable,
    params->color_mod_config, params->current_config);

  enable = ((params->current_config & VFE_MOD_MCE) != 0);
  status = vfe_ctrl_obj->vfe_module.mce_mod.ops.enable(module_id,
    &(vfe_ctrl_obj->vfe_module.mce_mod), params, enable, TRUE);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: MCE enable/disable failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  enable = ((params->current_config & VFE_MOD_SCE) != 0);
  status = vfe_ctrl_obj->vfe_module.sce_mod.ops.enable(module_id,
    &(vfe_ctrl_obj->vfe_module.sce_mod), params, enable, TRUE);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: SCE enable/disable failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  enable = ((params->current_config & VFE_MOD_CHROMA_SUPPRESS) != 0);
  status = vfe_ctrl_obj->vfe_module.chroma_supp_mod.ops.enable(module_id,
    &(vfe_ctrl_obj->vfe_module.chroma_supp_mod), params, enable, TRUE);
  if (VFE_SUCCESS != status) {
    CDBG_HIGH("%s: Chroma suppression enable/disable failed", __func__);
    return VFE_ERROR_GENERAL;
  }

  return VFE_SUCCESS;
}
/*===========================================================================
 * FUNCTION    - vfe_blk_inc_comp -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_blk_inc_comp(vfe_ctrl_info_t *p_obj, void *parm)
{
  vfe_status_t status = VFE_SUCCESS;
  vfe_stats_output_t *output = (vfe_stats_output_t *)parm;

  output->blk_inc_comp = p_obj->vfe_module.linear_mod.blk_inc_comp;
  return VFE_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - vfe_is_stats_conf_enabled -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function returns the stats module that are enabled
 *==========================================================================*/
vfe_status_t vfe_is_stats_conf_enabled(vfe_ctrl_info_t *p_obj, void *parm)
{
  vfe_status_t status = VFE_SUCCESS;
  vfe_params_t *params = &(p_obj->vfe_params);
  vfe_stats_conf_enable_t *p_stats_conf = (vfe_stats_conf_enable_t *)parm;
  p_stats_conf->aec_enabled = (params->current_config & VFE_MOD_AEC_STATS)
    != 0;
  p_stats_conf->awb_enabled = (params->current_config & VFE_MOD_AWB_STATS)
    != 0;
  p_stats_conf->af_enabled = (params->current_config & VFE_MOD_AF_STATS) != 0;
  p_stats_conf->rs_cs_enabled = (params->current_config & VFE_MOD_RSCS_STATS)
    != 0;
  p_stats_conf->hist_enabled = (params->current_config & VFE_MOD_IHIST_STATS)
    != 0;
  return status;
}/*vfe_is_stats_conf_enabled*/

/*===========================================================================
 * FUNCTION    - vfe_set_hfr_mode -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 * Also sets the hfr mode and frame skip pattern
 *==========================================================================*/
vfe_status_t vfe_set_hfr_mode(vfe_ctrl_info_t *p_obj,
  vfe_hfr_info_t *hfr_info)
{
  vfe_status_t status = VFE_SUCCESS;
  vfe_frame_skip frm_skip;
  int mod_id = 0;

  p_obj->vfe_params.vfe_hfr_mode = hfr_info->hfr_mode;

  // no frame skip for the video encoder path
  frm_skip.output2period = 31;
  frm_skip.output2pattern = 0xffffffff;

  if ((hfr_info->hfr_mode != CAMERA_HFR_MODE_OFF) &&
    (FALSE == hfr_info->preview_hfr)) {
    //skip frames for the view finderpath in hfr mode
    frm_skip.output1period = hfr_info->hfr_mode - 1;
    frm_skip.output1pattern = 1 << (hfr_info->hfr_mode - 1);
  } else {
    //no frame skip for the view finderpath in non hfr mode
    frm_skip.output1period = 31;
    frm_skip.output1pattern = 0xffffffff;
  }


  return VFE_SUCCESS;
}
/*===========================================================================
 * FUNCTION    - isp_send_hw_cmd -
 *
 * DESCRIPTION:
 *==========================================================================*/
int isp_send_hw_cmd(int fd, int type,
  void *pCmdData, unsigned int messageSize, int cmd_id)
{
  int rc = 0;
  vfe_status_t status;
  struct msm_vfe_cfg_cmd cfgCmd;
  struct msm_isp_cmd ispcmd;

  ispcmd.id = cmd_id;
  ispcmd.length = messageSize;
  ispcmd.value = pCmdData;

  cfgCmd.cmd_type = type;
  cfgCmd.length = sizeof(struct msm_isp_cmd);
  cfgCmd.value = &ispcmd;

  rc = ioctl(fd, MSM_CAM_IOCTL_CONFIG_VFE, &cfgCmd);
  if (rc < 0)
    CDBG_ERROR("%s: MSM_CAM_IOCTL_CONFIG_VFE failed %d\n",
      __func__, rc);
  return rc;
}

/*===========================================================================
 * FUNCTION    - vfe_command_ops -
 *
 * DESCRIPTION:
 *
 *==========================================================================*/
vfe_status_t vfe_command_ops(vfe_ctrl_info_t *p_obj, void *parm)
{
  int rc = 0;
  mod_cmd_t cmd = *(mod_cmd_t *)parm;
  CDBG(" %s cmd_type = %d", __func__, cmd.mod_cmd_ops);
  switch(cmd.mod_cmd_ops) {
    case MOD_CMD_START:
      rc = isp_send_hw_cmd(p_obj->vfe_params.camfd, CMD_GENERAL,
        cmd.cmd_data, cmd.length, VFE_CMD_START);
      break;
    case MOD_CMD_STOP:
      rc = vfe_stats_stop(p_obj);
      if (VFE_SUCCESS != rc) {
        CDBG_ERROR("%s: vfe_stats_stop failed",__func__);
      }
      rc = isp_send_hw_cmd(p_obj->vfe_params.camfd, CMD_GENERAL,
        cmd.cmd_data, cmd.length, VFE_CMD_STOP);
      break;
    case MOD_CMD_RESET:
      rc = isp_send_hw_cmd(p_obj->vfe_params.camfd, CMD_GENERAL,
        cmd.cmd_data, cmd.length, VFE_CMD_RESET);
      break;
    default:
      CDBG_ERROR("cmd_type = %d not supported", cmd.mod_cmd_ops);
      break;
  }
  if (VFE_SUCCESS != rc) {
    CDBG_ERROR("Failed configuring cmd_type = %d", cmd.mod_cmd_ops);
    return VFE_ERROR_GENERAL;
  }
  return VFE_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - vfe_module_ops_init -
 * DESCRIPTION:
 * DEPENDENCY:
 *==========================================================================*/
vfe_status_t vfe_module_ops_init(vfe_ctrl_info_t *p_obj)
{
  vfe_linearization_ops_init(&(p_obj->vfe_module.linear_mod));
  vfe_abf_ops_init(&(p_obj->vfe_module.abf_mod));
  vfe_bcc_ops_init(&(p_obj->vfe_module.bcc_mod));
  vfe_bpc_v3_ops_init(&(p_obj->vfe_module.bpc_mod));
  vfe_chroma_enhan_ops_init(&(p_obj->vfe_module.chroma_enhan_mod));
  vfe_chroma_suppression_ops_init(&(p_obj->vfe_module.chroma_supp_mod));
  vfe_clamp_ops_init(&(p_obj->vfe_module.clamp_mod));
  vfe_clf_ops_init(&(p_obj->vfe_module.clf_mod));
  vfe_color_correct_ops_init(&(p_obj->vfe_module.color_correct_mod));
  vfe_demosaic_ops_init(&(p_obj->vfe_module.demosaic_mod));
  vfe_demux_ops_init(&(p_obj->vfe_module.demux_mod));
  vfe_fov_ops_init(&(p_obj->vfe_module.fov_mod));
  vfe_gamma_ops_init(&(p_obj->vfe_module.gamma_mod));
  vfe_la_ops_init(&(p_obj->vfe_module.la_mod));
  vfe_mce_ops_init(&(p_obj->vfe_module.mce_mod));
  vfe_rolloff_ops_init(&(p_obj->vfe_module.rolloff_mod));
  vfe_scaler_ops_init(&(p_obj->vfe_module.scaler_mod));
  vfe_sce_ops_init(&(p_obj->vfe_module.sce_mod));
  vfe_wb_ops_init(&(p_obj->vfe_module.wb_mod));
  vfe_awb_stats_ops_init(&(p_obj->vfe_module.stats.awb_stats));
  vfe_ihist_stats_ops_init(&(p_obj->vfe_module.stats.ihist_stats));
  vfe_rs_stats_ops_init(&(p_obj->vfe_module.stats.rs_stats));
  vfe_cs_stats_ops_init(&(p_obj->vfe_module.stats.cs_stats));
  vfe_bg_stats_ops_init(&(p_obj->vfe_module.stats.bg_stats));
  vfe_be_stats_ops_init(&(p_obj->vfe_module.stats.be_stats));
  vfe_bf_stats_ops_init(&(p_obj->vfe_module.stats.bf_stats));
  vfe_bhist_stats_ops_init(&(p_obj->vfe_module.stats.bhist_stats));
  return VFE_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - vfe_module_ops_deinit -
 * DESCRIPTION:
 * DEPENDENCY: This function needs to be called after vfe_module_ops_init.
 *==========================================================================*/
vfe_status_t vfe_module_ops_deinit(vfe_ctrl_info_t *p_obj)
{
  vfe_linearization_ops_deinit(&(p_obj->vfe_module.linear_mod));
  vfe_abf_ops_deinit(&(p_obj->vfe_module.abf_mod));
  vfe_bcc_ops_deinit(&(p_obj->vfe_module.bcc_mod));
  vfe_bpc_v3_ops_deinit(&(p_obj->vfe_module.bpc_mod));
  vfe_chroma_enhan_ops_deinit(&(p_obj->vfe_module.chroma_enhan_mod));
  vfe_color_correct_ops_deinit(&(p_obj->vfe_module.color_correct_mod));
  vfe_chroma_suppression_ops_deinit(&(p_obj->vfe_module.chroma_supp_mod));
  vfe_clamp_ops_deinit(&(p_obj->vfe_module.clamp_mod));
  vfe_clf_ops_deinit(&(p_obj->vfe_module.clf_mod));
  vfe_demosaic_ops_deinit(&(p_obj->vfe_module.demosaic_mod));
  vfe_demux_ops_deinit(&(p_obj->vfe_module.demux_mod));
  vfe_fov_ops_deinit(&(p_obj->vfe_module.fov_mod));
  vfe_gamma_ops_deinit(&(p_obj->vfe_module.gamma_mod));
  vfe_la_ops_deinit(&(p_obj->vfe_module.la_mod));
  vfe_mce_ops_deinit(&(p_obj->vfe_module.mce_mod));
  vfe_rolloff_ops_deinit(&(p_obj->vfe_module.rolloff_mod));
  vfe_scaler_ops_deinit(&(p_obj->vfe_module.scaler_mod));
  vfe_sce_ops_deinit(&(p_obj->vfe_module.sce_mod));
  vfe_wb_ops_deinit(&(p_obj->vfe_module.wb_mod));
  vfe_awb_stats_ops_deinit(&(p_obj->vfe_module.stats.awb_stats));
  vfe_ihist_stats_ops_deinit(&(p_obj->vfe_module.stats.ihist_stats));
  vfe_rs_stats_ops_deinit(&(p_obj->vfe_module.stats.rs_stats));
  vfe_cs_stats_ops_deinit(&(p_obj->vfe_module.stats.cs_stats));
  vfe_bg_stats_ops_deinit(&(p_obj->vfe_module.stats.bg_stats));
  vfe_be_stats_ops_deinit(&(p_obj->vfe_module.stats.be_stats));
  vfe_bf_stats_ops_deinit(&(p_obj->vfe_module.stats.bf_stats));
  vfe_bhist_stats_ops_deinit(&(p_obj->vfe_module.stats.bhist_stats));

  return VFE_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - vfe_client_module_init -
 * DESCRIPTION:
 * DEPENDENCY: This function needs to be called in vfe_process
 *==========================================================================*/
vfe_ctrl_info_t *vfe_get_obj(vfe_client_t *vfe_client)
{
  vfe_ctrl_info_t *vfe_obj = NULL;
  /* now we have not implemented the use case of using 2 VFE obj */
  if(vfe_client->obj_idx_mask == 1)
    vfe_obj= &vfe_objs.vfe_obj[0];
  else if(vfe_client->obj_idx_mask == 2)
    vfe_obj= &vfe_objs.vfe_obj[1];
  return vfe_obj;
}

/*===========================================================================
 * FUNCTION    - vfe_init -
 * DESCRIPTION:
 * DEPENDENCY: This function needs to be called in vfe_process
 *==========================================================================*/
static int vfe_init(vfe_client_t *vfe_client, int event,
  void *parm)
{
  vfe_ctrl_info_t *vfe_ctrl_obj;
  sensor_output_format_t *snsr_fmt = parm;
  if(!vfe_client) {
    CDBG_ERROR("%s: null vfe client",  __func__);
    return -1;
  }

  vfe_ctrl_obj = vfe_get_obj(vfe_client);

  CDBG("%s: parm event =%d", __func__, event);
  if(!vfe_ctrl_obj) {
    CDBG_ERROR("%s: no VFE OBJ associated with clientr",  __func__);
    return -1;
  }
  vfe_ctrl_obj->vfe_params.sensor_parms.vfe_snsr_fmt = *snsr_fmt;
  if(VFE_SUCCESS != vfe_params_init(vfe_ctrl_obj, &vfe_ctrl_obj->vfe_params))
    return VFE_ERROR_GENERAL;

  if(VFE_SUCCESS != vfe_modules_init(&(vfe_ctrl_obj->vfe_module),
    &vfe_ctrl_obj->vfe_params))
    return VFE_ERROR_GENERAL;

  if (vfe_ctrl_obj->vfe_params.sensor_parms.vfe_snsr_fmt != SENSOR_YCBCR) {
    if (VFE_SUCCESS != vfe_stats_init(&(vfe_ctrl_obj->vfe_module.stats),
      &vfe_ctrl_obj->vfe_params))
      return VFE_ERROR_GENERAL;
    if(VFE_SUCCESS != vfe_stats_buffer_init(vfe_ctrl_obj)) {
       CDBG_ERROR("%s: vfe_stats_buffer_init error", __func__);
       return VFE_ERROR_GENERAL;
     }
  }

#ifdef FEATURE_VFE_TEST_VECTOR
  if (VFE_SUCCESS != vfe_test_vector_init(&vfe_ctrl_obj->test_vector,
    (void *)vfe_ctrl_obj))
    return VFE_ERROR_GENERAL;
#endif
  return VFE_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - vfe_add_instance -
 * DESCRIPTION:
 * DEPENDENCY: This function needs to be called in vfe_process
 *==========================================================================*/
static int vfe_add_instance(vfe_client_t *vfe_client, int type,
  void *parm_in, void *parm_out)
{
  vfe_ctrl_info_t *vfe_ctrl_obj;
  int vfe_obj_id = *((int *)parm_in);

  if((vfe_obj_id < 0) || (vfe_obj_id > VFE_MAX_OBJS)) {
    CDBG_ERROR("%s: Wrong ID, cannot support :%d",  __func__, vfe_obj_id);
    return -1;
  }

  if((vfe_client->obj_idx_mask & (1 << vfe_obj_id))) {
    CDBG_ERROR("%s: the object already exists : %d",  __func__, vfe_obj_id);
    return VFE_ERROR_INVALID_OPERATION;
  }

  vfe_client->obj_idx_mask |= (1 << vfe_obj_id);
  vfe_ctrl_obj = &vfe_objs.vfe_obj[vfe_obj_id];
  vfe_ctrl_obj->vfe_params.camfd = vfe_client->ops->fd;
  vfe_ctrl_obj->ops = vfe_client->ops;

  vfe_module_ops_init(vfe_ctrl_obj);
  return VFE_SUCCESS;
} /* vfe_add_instance */

/*===========================================================================
 * FUNCTION    - vfe_process -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
static int vfe_process(void *client, int event, void *parm)
{
  vfe_client_t *vfe_client = (vfe_client_t *)client;
  vfe_ctrl_info_t *vfe_ctrl_obj;
  vfe_op_mode_t op_mode;
  int rc = VFE_SUCCESS;

  if(!vfe_client) {
    CDBG_ERROR("%s: vfe client is NULL",  __func__);
    return -1;
  }
  vfe_ctrl_obj = vfe_get_obj(vfe_client);

  CDBG("%s: parm event =%d", __func__, event);
  if(!vfe_ctrl_obj) {
    CDBG_ERROR("%s: no VFE OBJ associated with clientr",  __func__);
    return -1;
  }

  switch(event) {
  case VFE_MODULE_INIT:
    rc = vfe_init(vfe_client, event, parm);
    break;
  case VFE_CONFIG_MODE:
    op_mode = *(vfe_op_mode_t *)parm;
    rc = vfe_config_mode(op_mode, vfe_ctrl_obj);
    break;
  case VFE_TRIGGER_UPDATE_FOR_3A: {
    vfe_3a_parms_udpate_t *vfe_3a_update = (vfe_3a_parms_udpate_t *)parm;
    switch(vfe_3a_update->mask & VFE_3A_MASK) {
    case VFE_TRIGGER_UPDATE_AEC:
      rc = vfe_trigger_update_for_aec(vfe_ctrl_obj);
      break;
    case VFE_TRIGGER_UPDATE_AWB:
      rc = vfe_trigger_update_for_awb(vfe_ctrl_obj);
      break;
    default:
      CDBG_ERROR("Invalid 3A trigger 3a_mask = 0x%x\n", vfe_3a_update->mask);
      rc = VFE_ERROR_GENERAL;
      break;
    }
    break;
  }
  case VFE_SOF_NOTIFY: {
    int *p_update = (int *)parm;
    rc = vfe_cmd_hw_reg_update(vfe_ctrl_obj, p_update);
    break;
  }
  case VFE_CONFIG_AF: {
   vfe_stats_af_params_t *af_params = (vfe_stats_af_params_t *)parm;
   rc = vfe_config_autofocus(vfe_ctrl_obj, af_params);
   break;
  }
  case VFE_STOP_AF:

    rc = vfe_stop_autofocus(vfe_ctrl_obj);
    if(rc != VFE_SUCCESS) {
      CDBG_ERROR("%s: VFE_STOP_AF failed\n", __func__);
    }
   break;
  case VFE_CMD_OPS:
    rc = vfe_command_ops(vfe_ctrl_obj, parm);
  break;
  case VFE_RELEASE_STATS:
    rc = vfe_release_stats(vfe_ctrl_obj);
    if(rc != VFE_SUCCESS)
      CDBG_ERROR("%s: VFE_RELEASE_STATS failed\n", __func__);
   break;
  default:
    CDBG_ERROR("%s: Invalid mode  %d\n", __func__, event);
    rc = VFE_ERROR_GENERAL;
    break;
  }
  if(rc > 0) {
    rc = -rc;
  }
  return rc;
} /* vfe_process */

/*===========================================================================
 * FUNCTION    - vfe_set_params -
 *
 * DESCRIPTION:
 *==========================================================================*/
static int vfe_set_params(void *client, int type, void *parm_in,
  void *parm_out)
{
  vfe_client_t *vfe_client = (vfe_client_t *)client;
  vfe_ctrl_info_t *vfe_ctrl_obj;
  cam_ctrl_dimension_t *In_dim, *Op_Dim;
  vfe_params_t *params = NULL;
  int rc = VFE_SUCCESS;

  if(!vfe_client) {
    CDBG_ERROR("%s: null vfe client",  __func__);
    return -1;
  }
  if(type == VFE_PARM_HW_VERSION) {
    vfe_client->vfe_version = *((uint32_t *)parm_in);
    return 0;
  } else if(type == VFE_PARM_ADD_OBJ_ID) {
    rc = vfe_add_instance(vfe_client, type, parm_in, parm_out);
    return rc;
  }
  vfe_ctrl_obj = vfe_get_obj(vfe_client);
  CDBG("%s: parm type =%d", __func__, type);
  if(!vfe_ctrl_obj) {
    CDBG_ERROR("%s: no VFE OBJ associated with clientr",  __func__);
    return -1;
  }
  params = &vfe_ctrl_obj->vfe_params;
  switch(type) {
  case VFE_SET_SENSOR_PARM: {
    vfe_sensor_params_t vfe_sensor_params;
    sensor_get_t sensor_get;

    sensor_get.data.sensor_dim.op_mode = SENSOR_MODE_PREVIEW;
    rc = vfe_client->ops->fetch_params(
                         vfe_client->my_comp_id, vfe_client->ops->parent,
                         ((MCTL_COMPID_SENSOR << 24)|SENSOR_GET_DIM_INFO),
                         &sensor_get, sizeof(sensor_get));
    if (rc < 0) {
      CDBG_HIGH("%s: sensor_get_params failed %d\n", __func__, rc);
      return rc;
    }
    vfe_sensor_params.qtr_size_width  = sensor_get.data.sensor_dim.width;
    vfe_sensor_params.qtr_size_height = sensor_get.data.sensor_dim.height;

    sensor_get.data.sensor_dim.op_mode = SENSOR_MODE_SNAPSHOT;
    rc = vfe_client->ops->fetch_params(
                         vfe_client->my_comp_id, vfe_client->ops->parent,
                         ((MCTL_COMPID_SENSOR << 24)|SENSOR_GET_DIM_INFO),
                         &sensor_get, sizeof(sensor_get));
    if (rc < 0) {
      CDBG_HIGH("%s: sensor_get_params failed %d\n", __func__, rc);
      return rc;
    }
    vfe_sensor_params.full_size_width  = sensor_get.data.sensor_dim.width;
    vfe_sensor_params.full_size_height = sensor_get.data.sensor_dim.height;

    rc = vfe_client->ops->fetch_params(
                         vfe_client->my_comp_id, vfe_client->ops->parent,
                         ((MCTL_COMPID_SENSOR << 24)|SENSOR_GET_OUTPUT_CFG),
                         &sensor_get, sizeof(sensor_get));
    if (rc < 0) {
      CDBG("%s: sensor_get_params failed %d\n", __func__, rc);
      return rc;
    }
    vfe_sensor_params.vfe_snsr_fmt  =
      sensor_get.data.sensor_output.output_format;
    vfe_sensor_params.vfe_raw_depth =
      sensor_get.data.sensor_output.raw_output;
    vfe_sensor_params.vfe_snsr_conn_mode =
      sensor_get.data.sensor_output.connection_mode;

    rc = vfe_client->ops->fetch_params(
                         vfe_client->my_comp_id, vfe_client->ops->parent,
                         ((MCTL_COMPID_SENSOR << 24)|SENSOR_GET_CAMIF_CFG),
                         &sensor_get, sizeof(sensor_get));
    if (rc < 0) {
      CDBG("%s: sensor_get_params failed %d\n", __func__, rc);
      return rc;
    }
    vfe_sensor_params.sensor_width     = sensor_get.data.camif_setting.width;
    vfe_sensor_params.sensor_height    = sensor_get.data.camif_setting.height;
    vfe_sensor_params.lastLine         = sensor_get.data.camif_setting.last_line;
    vfe_sensor_params.firstLine        = sensor_get.data.camif_setting.first_line;
    vfe_sensor_params.lastPixel        = sensor_get.data.camif_setting.last_pixel;
    vfe_sensor_params.firstPixel       = sensor_get.data.camif_setting.first_pixel;
    vfe_sensor_params.vfe_camif_in_fmt = sensor_get.data.camif_setting.format;

    vfe_ctrl_obj->vfe_params.sensor_parms = vfe_sensor_params;
    break;
  }
  case VFE_SET_CHROMATIX_PARM:
    vfe_ctrl_obj->vfe_params.chroma3a = (chromatix_parms_type *)parm_in;
    break;
  case VFE_SET_CAMERA_MODE:
    vfe_ctrl_obj->vfe_params.cam_mode = *(cam_mode_t *)parm_in;
    break;

  case VFE_SET_EFFECTS: {
    vfe_effects_type_t *effect_type = (vfe_effects_type_t *)parm_in;
    vfe_effects_params_t *effects_param = (vfe_effects_params_t *)parm_out;
    vfe_ctrl_obj->vfe_params.effects_params = *effects_param;
    rc = vfe_set_effect(vfe_ctrl_obj, *effect_type, effects_param);
    break;
  }

  case VFE_SET_BESTSHOT: {
    camera_bestshot_mode_type *bs_type = (camera_bestshot_mode_type *)parm_in;
    rc = vfe_set_bestshot(vfe_ctrl_obj, *bs_type);
    if(!rc)
      vfe_ctrl_obj->vfe_params.bs_mode = *bs_type;
    break;
  }

  case VFE_SET_AEC_PARAMS: {
#ifdef FEATURE_VFE_TEST_VECTOR
  if (vfe_test_vector_enabled(&vfe_ctrl_obj->test_vector))
    return VFE_SUCCESS;
#endif
    stats_proc_get_t stats_proc_get;
    stats_proc_get.d.get_aec.type = AEC_PARMS;
    rc = vfe_client->ops->fetch_params(
                         vfe_client->my_comp_id, vfe_client->ops->parent,
                         ((MCTL_COMPID_STATSPROC << 24)|STATS_PROC_AEC_TYPE),
                         &stats_proc_get, sizeof(stats_proc_get));
    if (rc < 0) {
      CDBG_HIGH("%s: stats_proc fetch parms failed %d\n", __func__, rc);
      return rc;
    }
    vfe_ctrl_obj->vfe_params.aec_params.cur_luma =
      stats_proc_get.d.get_aec.d.aec_parms.cur_luma;
    vfe_ctrl_obj->vfe_params.aec_params.cur_real_gain =
      stats_proc_get.d.get_aec.d.aec_parms.cur_real_gain;
    vfe_ctrl_obj->vfe_params.aec_params.exp_index =
      stats_proc_get.d.get_aec.d.aec_parms.exp_index;
    vfe_ctrl_obj->vfe_params.aec_params.exp_tbl_val =
      stats_proc_get.d.get_aec.d.aec_parms.exp_tbl_val;
    vfe_ctrl_obj->vfe_params.aec_params.lux_idx =
      stats_proc_get.d.get_aec.d.aec_parms.lux_idx;
    vfe_ctrl_obj->vfe_params.aec_params.snapshot_real_gain =
      stats_proc_get.d.get_aec.d.aec_parms.snapshot_real_gain;
    vfe_ctrl_obj->vfe_params.aec_params.target_luma =
      stats_proc_get.d.get_aec.d.aec_parms.target_luma;
    break;
  }
  case VFE_SET_AWB_PARMS: {
#ifdef FEATURE_VFE_TEST_VECTOR
  if (vfe_test_vector_enabled(&vfe_ctrl_obj->test_vector))
    break; //return VFE_SUCCESS;
#endif
    stats_proc_get_t stats_proc_get;
    uint32_t color_temp = vfe_ctrl_obj->vfe_params.awb_params.color_temp;
    stats_proc_get.d.get_awb.type = AWB_PARMS;
    rc = vfe_client->ops->fetch_params(
                         vfe_client->my_comp_id, vfe_client->ops->parent,
                         ((MCTL_COMPID_STATSPROC << 24)|STATS_PROC_AWB_TYPE),
                         &stats_proc_get, sizeof(stats_proc_get));
    if (rc < 0) {
      CDBG_HIGH("%s: stats_proc fetch parms failed %d\n", __func__, rc);
      return rc;
    }
    vfe_ctrl_obj->vfe_params.awb_params.bounding_box =
       stats_proc_get.d.get_awb.d.awb_params.bounding_box;
    vfe_ctrl_obj->vfe_params.awb_params.color_temp =
       stats_proc_get.d.get_awb.d.awb_params.color_temp;
    vfe_ctrl_obj->vfe_params.awb_params.exterme_col_param.t1 =
       stats_proc_get.d.get_awb.d.awb_params.exterme_col_param.t1;
    vfe_ctrl_obj->vfe_params.awb_params.exterme_col_param.t2 =
       stats_proc_get.d.get_awb.d.awb_params.exterme_col_param.t2;
    vfe_ctrl_obj->vfe_params.awb_params.exterme_col_param.t3 =
       stats_proc_get.d.get_awb.d.awb_params.exterme_col_param.t3;
    vfe_ctrl_obj->vfe_params.awb_params.exterme_col_param.t4 =
       stats_proc_get.d.get_awb.d.awb_params.exterme_col_param.t4;
    vfe_ctrl_obj->vfe_params.awb_params.exterme_col_param.t5 =
       stats_proc_get.d.get_awb.d.awb_params.exterme_col_param.t5;
    vfe_ctrl_obj->vfe_params.awb_params.exterme_col_param.t6 =
       stats_proc_get.d.get_awb.d.awb_params.exterme_col_param.t6;
    vfe_ctrl_obj->vfe_params.awb_params.gain =
       stats_proc_get.d.get_awb.d.awb_params.gain;

    if(vfe_ctrl_obj->vfe_params.awb_params.color_temp == 0) {
      vfe_ctrl_obj->vfe_params.awb_params.color_temp = color_temp;
    }
    break;
  }
  case VFE_SET_ASD_PARMS: {
    stats_proc_get_t stats_proc_get;
    stats_proc_get.d.get_asd.type = ASD_PARMS;
    rc = vfe_client->ops->fetch_params(
                         vfe_client->my_comp_id, vfe_client->ops->parent,
                         ((MCTL_COMPID_STATSPROC << 24)|STATS_PROC_ASD_TYPE),
                         &stats_proc_get, sizeof(stats_proc_get));
    if (rc < 0) {
      CDBG_HIGH("%s: stats_proc fetch parms failed %d\n", __func__, rc);
      return rc;
    }
    vfe_ctrl_obj->vfe_params.asd_params.asd_soft_focus_dgr =
      stats_proc_get.d.get_asd.d.asd_params.asd_soft_focus_dgr;
    vfe_ctrl_obj->vfe_params.asd_params.backlight_detected =
      stats_proc_get.d.get_asd.d.asd_params.backlight_detected;
    vfe_ctrl_obj->vfe_params.asd_params.backlight_scene_severity =
      stats_proc_get.d.get_asd.d.asd_params.backlight_scene_severity;
    vfe_ctrl_obj->vfe_params.asd_params.landscape_severity =
      stats_proc_get.d.get_asd.d.asd_params.landscape_severity;
    vfe_ctrl_obj->vfe_params.asd_params.portrait_severity =
      stats_proc_get.d.get_asd.d.asd_params.portrait_severity;

    vfe_ctrl_obj->vfe_params.sharpness_info.asd_soft_focus_dgr =
      stats_proc_get.d.get_asd.d.asd_params.asd_soft_focus_dgr;
    vfe_ctrl_obj->vfe_params.sharpness_info.portrait_severity =
      (float)stats_proc_get.d.get_asd.d.asd_params.portrait_severity;
    break;
  }
  case VFE_SET_SENSOR_DIG_GAIN: {
#ifdef FEATURE_VFE_TEST_VECTOR
  if (vfe_test_vector_enabled(&vfe_ctrl_obj->test_vector))
    break; //return VFE_SUCCESS;
#endif
    float *p_dig_gain = (float *)parm_in;
    vfe_ctrl_obj->vfe_params.digital_gain = MIN(1.0, *p_dig_gain);
    break;
  }
  case VFE_SET_FLASH_PARMS: {
#ifdef FEATURE_VFE_TEST_VECTOR
  if (vfe_test_vector_enabled(&vfe_ctrl_obj->test_vector))
    break; //return VFE_SUCCESS;
#endif
    stats_proc_get_t stats_proc_get;
    stats_proc_get.d.get_aec.type = AEC_FLASH_DATA;
    rc = vfe_client->ops->fetch_params(
                         vfe_client->my_comp_id, vfe_client->ops->parent,
                         ((MCTL_COMPID_STATSPROC << 24)|STATS_PROC_AEC_TYPE),
                         &stats_proc_get, sizeof(stats_proc_get));
    if (rc < 0) {
      CDBG_HIGH("%s: stats_proc fetch parms failed %d\n", __func__, rc);
      return rc;
    }
    vfe_ctrl_obj->vfe_params.flash_params.flash_mode =
      stats_proc_get.d.get_aec.d.flash_parms.flash_mode;
    vfe_ctrl_obj->vfe_params.flash_params.sensitivity_led_hi =
      stats_proc_get.d.get_aec.d.flash_parms.sensitivity_led_hi;
    vfe_ctrl_obj->vfe_params.flash_params.sensitivity_led_low =
      stats_proc_get.d.get_aec.d.flash_parms.sensitivity_led_low;
    vfe_ctrl_obj->vfe_params.flash_params.sensitivity_led_off =
      stats_proc_get.d.get_aec.d.flash_parms.sensitivity_led_off;
    vfe_ctrl_obj->vfe_params.flash_params.strobe_duration =
      stats_proc_get.d.get_aec.d.flash_parms.strobe_duration;
    vfe_ctrl_obj->vfe_params.flash_params.sensitivity_led_hi =
      MAX(1, vfe_ctrl_obj->vfe_params.flash_params.sensitivity_led_hi);
    CDBG("%s: sensitivity high %d", __func__,
      vfe_ctrl_obj->vfe_params.flash_params.sensitivity_led_hi);
    break;
  }
  case VFE_SET_EZTUNE: {
    ez_vfe_set(vfe_ctrl_obj, parm_in, parm_out);
    break;
  }
  case VFE_SET_SHARPNESS_PARMS: {
    float *sharp_parm = (float *)parm_in;
    vfe_ctrl_obj->vfe_params.sharpness_info.ui_sharp_ctrl_factor = *sharp_parm;
    break;
  }
  case VFE_SET_WB: {
    vfe_ctrl_obj->vfe_params.wb = *((config3a_wb_t *)parm_in);
    if(vfe_ctrl_obj->vfe_params.wb != CAMERA_WB_AUTO) {
      stats_proc_get_t stats_proc_get;
      stats_proc_get.d.get_awb.type = AWB_GAINS;
      rc = vfe_client->ops->fetch_params(
                       vfe_client->my_comp_id, vfe_client->ops->parent,
                       ((MCTL_COMPID_STATSPROC << 24)|STATS_PROC_AWB_TYPE),
                       &stats_proc_get, sizeof(stats_proc_get));
      if (rc < 0) {
        CDBG_HIGH("%s: stats_proc fetch AWB Gains failed %d\n", __func__, rc);
        return rc;
      }
      vfe_ctrl_obj->vfe_params.awb_params.gain =
        stats_proc_get.d.get_awb.d.awb_gains.curr_gains;
      CDBG_HIGH("%s: Whitebalance gains for %d R = %f G = %f B = %f", __func__,
            vfe_ctrl_obj->vfe_params.wb,
            vfe_ctrl_obj->vfe_params.awb_params.gain.r_gain,
            vfe_ctrl_obj->vfe_params.awb_params.gain.g_gain,
            vfe_ctrl_obj->vfe_params.awb_params.gain.b_gain);
    }
    rc = vfe_set_manual_wb(vfe_ctrl_obj);
    break;
  }
  case VFE_SET_FOV_CROP_FACTOR: {
    uint32_t *p_crop_factor = (uint32_t *)parm_in;
    rc = 0;
    vfe_ctrl_obj->vfe_params.crop_factor = *p_crop_factor;
    rc = vfe_update_scaler(vfe_ctrl_obj, NULL);
    CDBG("%s: crop_factor %d, rc = %d", __func__, params->crop_factor, rc);
    break;
  }
  case VFE_SET_CAMIF_DIM: {
    pixel_crop_info_t demosaic_crop_info;
    camif_output_t camif_output;
    rc = vfe_client->ops->fetch_params(
                        vfe_client->my_comp_id, vfe_client->ops->parent,
                        ((MCTL_COMPID_CAMIF << 24)|CAMIF_PARAMS_CAMIF_DIMENSION),
                        (void *)&camif_output, sizeof(camif_output));
    if (rc < 0) {
      CDBG_HIGH("%s Fetch params error ", __func__);
      return rc;
    }
    vfe_ctrl_obj->vfe_params.vfe_input_win.first_pixel =
                                camif_output.d.camif_window.first_pixel;
    vfe_ctrl_obj->vfe_params.vfe_input_win.last_pixel  =
                                camif_output.d.camif_window.last_pixel;
    vfe_ctrl_obj->vfe_params.vfe_input_win.first_line  =
                                camif_output.d.camif_window.first_line;
    vfe_ctrl_obj->vfe_params.vfe_input_win.last_line   =
                                camif_output.d.camif_window.last_line;
    vfe_ctrl_obj->vfe_params.vfe_input_win.width =
                   vfe_ctrl_obj->vfe_params.vfe_input_win.last_pixel -
                   vfe_ctrl_obj->vfe_params.vfe_input_win.first_pixel + 1;
    vfe_ctrl_obj->vfe_params.vfe_input_win.height =
                   vfe_ctrl_obj->vfe_params.vfe_input_win.last_line -
                   vfe_ctrl_obj->vfe_params.vfe_input_win.first_line + 1;

    params->scaler_op_params[OUT_ENCODER].first_pixel = camif_output.d.camif_window.first_pixel;
    params->scaler_op_params[OUT_ENCODER].last_pixel = camif_output.d.camif_window.last_pixel;
    params->scaler_op_params[OUT_ENCODER].first_line = camif_output.d.camif_window.first_line;
    params->scaler_op_params[OUT_ENCODER].last_line = camif_output.d.camif_window.last_line;

    params->scaler_op_params[OUT_PREVIEW].first_pixel = camif_output.d.camif_window.first_pixel;
    params->scaler_op_params[OUT_PREVIEW].last_pixel = camif_output.d.camif_window.last_pixel;
    params->scaler_op_params[OUT_PREVIEW].first_line = camif_output.d.camif_window.first_line;
    params->scaler_op_params[OUT_PREVIEW].last_line = camif_output.d.camif_window.last_line;

    demosaic_crop_info.first_pixel = vfe_ctrl_obj->vfe_params.vfe_input_win.first_pixel;
    demosaic_crop_info.last_pixel  = vfe_ctrl_obj->vfe_params.vfe_input_win.last_pixel;
    demosaic_crop_info.first_line  = vfe_ctrl_obj->vfe_params.vfe_input_win.first_line;
    demosaic_crop_info.last_line   = vfe_ctrl_obj->vfe_params.vfe_input_win.last_line;
    vfe_set_pixel_crop_info(vfe_ctrl_obj, &demosaic_crop_info);
    break;
  }
  case VFE_SET_MCE: {
    uint32_t *mce_enable = (uint32_t *)parm_in;
    rc = vfe_set_mce(vfe_ctrl_obj, *mce_enable);
    break;
  }
  case VFE_SET_SCE: {
    int32_t *sce_factor = (int32_t *)parm_in;
    rc = vfe_set_sce(vfe_ctrl_obj, *sce_factor);
    break;
  }
  case VFE_SET_OUTPUT_INFO: {
    rc = vfe_set_output_info(vfe_ctrl_obj, parm_in);
    break;
  }
  /* move frame skip to axi
    case VFE_SET_FRAME_SKIP: {
    vfe_frame_skip *frm_skip = (vfe_frame_skip *)parm_in;
    rc = vfe_set_frame_skip_pattern(vfe_ctrl_obj, frm_skip);
    break;
  }*/
  case VFE_SET_HFR_MODE: {
    vfe_hfr_info_t  *hfr_info = (vfe_hfr_info_t *)parm_in;
    rc = vfe_set_hfr_mode(vfe_ctrl_obj, hfr_info);
    break;
  }
  case VFE_SET_STATS_VERSION: {
    struct msm_ver_num_info ver = *((struct msm_ver_num_info *)parm_in);
    rc = vfe_set_stats_version(vfe_ctrl_obj, ver);
    break;
  }
  case VFE_SETS_STATS_STREAMS: {
    vfe_set_hal_stream *tmp = (vfe_set_hal_stream *)parm_in;
    rc = vfe_set_stats_streams(vfe_ctrl_obj, tmp);
    break;
  }
  default:
    CDBG_ERROR("Invalid parm");
    rc = -VFE_ERROR_GENERAL;
    break;
  }
  if(rc > 0) {
    rc = -rc;
    CDBG_ERROR("%s: param_type = %d, error = %d",  __func__, type, rc);
  }
  return rc;
} /* vfe_set_params */

/*===========================================================================
 * FUNCTION    - vfe_get_params -
 *
 * DESCRIPTION:
 *==========================================================================*/
static int vfe_get_params(void *client, int parm_type,
  void *parm, int parm_len)
{
  vfe_client_t *vfe_client = (vfe_client_t *)client;
  vfe_ctrl_info_t *vfe_ctrl_obj;
  int rc = VFE_SUCCESS;

  if(!vfe_client) {
    CDBG_ERROR("%s: null vfe client",  __func__);
    return -1;
  }
  vfe_ctrl_obj = vfe_get_obj(vfe_client);

  CDBG("%s: parm type =%d", __func__, parm_type);
  if(!vfe_ctrl_obj) {
    CDBG_ERROR("%s: no VFE OBJ associated with clientr",  __func__);
    return -1;
  }

  switch(parm_type) {
  case VFE_GET_STATS_BUF_PTR:
    rc = vfe_query_stats_buf_ptr(vfe_ctrl_obj, parm_type, parm);
  break;
  case VFE_GET_ZOOM_CROP_INFO:
  case VFE_GET_FOV_CROP_PARM:
    rc = vfe_query_fov_crop(vfe_ctrl_obj, parm_type, parm);
    break;
  case VFE_GET_CAMIF_PARM:
    break;
  case VFE_GET_OUTPUT_DIMENSION: {
      vfe_axi_output_dim_t *dim = (vfe_axi_output_dim_t *)parm;
      memset(dim, 0, sizeof(vfe_axi_output_dim_t));
      if(vfe_ctrl_obj->vfe_params.vfe_op_mode == VFE_OP_MODE_PREVIEW ||
         vfe_ctrl_obj->vfe_params.vfe_op_mode == VFE_OP_MODE_RAW_SNAPSHOT) {
        dim->output2w = vfe_ctrl_obj->vfe_params.output2w;
        dim->output2h = vfe_ctrl_obj->vfe_params.output2h;
      } else if(vfe_ctrl_obj->vfe_params.vfe_op_mode == VFE_OP_MODE_VIDEO ||
          vfe_ctrl_obj->vfe_params.vfe_op_mode == VFE_OP_MODE_ZSL ||
          vfe_ctrl_obj->vfe_params.vfe_op_mode == VFE_OP_MODE_SNAPSHOT ||
          vfe_ctrl_obj->vfe_params.vfe_op_mode == VFE_OP_MODE_JPEG_SNAPSHOT) {
        dim->output1w = vfe_ctrl_obj->vfe_params.output1w;
        dim->output1h = vfe_ctrl_obj->vfe_params.output1h;
        dim->output2w = vfe_ctrl_obj->vfe_params.output2w;
        dim->output2h = vfe_ctrl_obj->vfe_params.output2h;
      }
    }
    break;
  case VFE_GET_AWB_SHIFT_BITS:
  case VFE_GET_IHIST_SHIFT_BITS:
  case VFE_GET_AEC_SHIFT_BITS:
    rc = vfe_query_shift_bits(vfe_ctrl_obj, parm_type, parm);
    break;
  case VFE_GET_RS_CS_PARM:
    rc = vfe_query_rs_cs_parm(vfe_ctrl_obj, parm_type, parm);
    break;
  case VFE_GET_PIXEL_CROP_INFO:
    rc = vfe_calc_pixel_crop_info(vfe_ctrl_obj, parm);
    break;
  case VFE_GET_PIXEL_SKIP_INFO:
    rc = vfe_calc_pixel_skip_info(vfe_ctrl_obj, parm);
    break;
  case VFE_GET_DIAGNOSTICS_PTR: {
    vfe_eztune_info_t *p_info = (vfe_eztune_info_t *)parm;
    p_info->diag_ptr = (void *)&(vfe_ctrl_obj->vfe_diag);
    break;
    }
  case VFE_GET_PP_INFO:
    rc = vfe_pp_info(vfe_ctrl_obj, parm);
    break;
  case VFE_GET_BLK_INC_COMP:
    rc = vfe_blk_inc_comp(vfe_ctrl_obj, parm);
    break;
  case VFE_GET_STATS_CONF_ENABLE:
    rc = vfe_is_stats_conf_enabled(vfe_ctrl_obj, parm);
    break;
  case VFE_GET_RESIDUE_DIG_GAIN: {
    float *p_dig_gain = (float *)parm;
    *p_dig_gain = vfe_ctrl_obj->vfe_params.aec_gain_adj.cc_gain_adj
      * vfe_ctrl_obj->vfe_params.aec_gain_adj.wb_gain_adj;
    *p_dig_gain = MAX(1.0, *p_dig_gain);
    break;
  }
  case VFE_GET_STATS_BUF_SIZE:
    rc = vfe_get_stats_buf_dimension(vfe_ctrl_obj, parm);
    if(rc != VFE_SUCCESS) {
      CDBG_ERROR("%s: VFE_GET_STATS_BUF_SIZE Failed", __func__);
    }
  break;
  default:
    CDBG_ERROR("%s: Invalid mode \n", __func__);
    rc = VFE_ERROR_GENERAL;
    break;
  }
  if(rc > 0) {
    rc = -rc;
  }
  return rc;
} /* vfe_get_params */

/*===========================================================================
 * FUNCTION    - vfe_destroy -
 *
 * DESCRIPTION:
 *==========================================================================*/
static int vfe_destroy(void *client)
{
  int idx;
  vfe_client_t *vfe_client = (vfe_client_t *)client;
  vfe_ctrl_info_t *vfe_ctrl_obj;
  int rc = VFE_SUCCESS;

  CDBG("%s: E", __func__);
  if(!vfe_client) {
    CDBG_ERROR("%s: null vfe client",  __func__);
    return -1;
  }
  vfe_ctrl_obj = vfe_get_obj(vfe_client);

  if(!vfe_ctrl_obj) {
    CDBG_ERROR("%s: no VFE OBJ associated with clientr",  __func__);
    goto end;
  }

#ifdef FEATURE_VFE_TEST_VECTOR
  if (0 != (rc = vfe_test_vector_deinit(&vfe_ctrl_obj->test_vector)))
    CDBG_ERROR("%s: vfe_test_vector_deinit err = %d", __func__, rc);
#endif

  if (0 != (rc = vfe_modules_deinit(&(vfe_ctrl_obj->vfe_module),
    &vfe_ctrl_obj->vfe_params))) {
    CDBG_ERROR("%s: vfe_modules_deinit err = %d\n", __func__, rc);
  }
  if (vfe_ctrl_obj->vfe_params.sensor_parms.vfe_snsr_fmt != SENSOR_YCBCR)
    if(VFE_SUCCESS != vfe_stats_buffer_free(vfe_ctrl_obj))
      CDBG_ERROR("%s: vfe_stats_buffer_free error", __func__);
end:
  return rc;
} /* vfe_destroy */
/*===========================================================================
 * FUNCTION    - vfe_parse_stats -
 *
 * DESCRIPTION:
 *==========================================================================*/
static int vfe_parse_stats(void *client,
  int isp_started, stats_type_t type, void *stats, void *stats_output)
{
  vfe_client_t *vfe_client = (vfe_client_t *)client;
  vfe_ctrl_info_t *p_obj;
  struct msm_cam_evt_msg *adsp = (struct msm_cam_evt_msg *) stats;
  struct msm_stats_buf *buf = (struct msm_stats_buf *) (adsp->data);
  vfe_stats_output_t *vfe_output = stats_output;

  int rc = VFE_SUCCESS;

  if(!vfe_client) {
    CDBG_ERROR("%s: vfe client is NULL",  __func__);
    return -1;
  }
  p_obj = vfe_get_obj(vfe_client);

  if(!p_obj) {
    CDBG_ERROR("%s: no VFE OBJ associated with clientr",  __func__);
    return -1;
  }

  CDBG("%s: E", __func__);

  switch (adsp->msg_id) {
    case MSG_ID_STATS_AWB:
      rc = vfe_stats_proc_MSG_ID_STATS_AWB(p_obj, isp_started,
             type, adsp, vfe_output);
      break;
    case  MSG_ID_STATS_IHIST:
      rc = vfe_stats_proc_MSG_ID_STATS_IHIST(p_obj, isp_started,
             type, adsp, vfe_output);
      break;
    case  MSG_ID_STATS_RS:
      rc = vfe_stats_proc_MSG_ID_STATS_RS(p_obj, isp_started,
             type, adsp, vfe_output);
      break;
    case  MSG_ID_STATS_CS:
      rc = vfe_stats_proc_MSG_ID_STATS_CS(p_obj, isp_started,
             type, adsp, vfe_output);
      break;
    case MSG_ID_STATS_COMPOSITE:
      rc = vfe_stats_proc_MSG_ID_STATS_COMPOSITE(p_obj, isp_started,
             type, adsp, vfe_output);
      break;
    case MSG_ID_STATS_BG:
      rc = vfe_stats_proc_MSG_ID_STATS_BG(p_obj, isp_started,
             type, adsp, vfe_output);
      break;
    case MSG_ID_STATS_BE:
      rc = vfe_stats_proc_MSG_ID_STATS_BE(p_obj, isp_started,
             type, adsp, vfe_output);
      break;
    case MSG_ID_STATS_BF:
      rc = vfe_stats_proc_MSG_ID_STATS_BF(p_obj, isp_started,
             type, adsp, vfe_output);
      break;
    case MSG_ID_STATS_BHIST:
      rc = vfe_stats_proc_MSG_ID_STATS_BHIST(p_obj, isp_started,
             type, adsp, vfe_output);
      break;
    default:
      rc = -1;
      break;
  } /*switch (adsp->msg_id) */

  CDBG("%s: X", __func__);
  return rc;
}/* vfe_parse_stats */
/*===========================================================================
 * FUNCTION    - vfe_ops_init -
 * DESCRIPTION:
 * DEPENDENCY: This function is called in vfe_client_init.
 *==========================================================================*/
vfe_status_t vfe_ops_init(vfe_client_t* vfe_client)
{
  CDBG_HIGH("%s: E", __func__);
  if(!vfe_client) {
    CDBG_ERROR("%s: vfe client is null", __func__);
    return -1;
  }

  vfe_client->vfe_ops.process = vfe_process;
  vfe_client->vfe_ops.set_param = vfe_set_params;
  vfe_client->vfe_ops.get_param = vfe_get_params;
  vfe_client->vfe_ops.parse_stats = vfe_parse_stats;
  vfe_client->vfe_ops.destroy = vfe_destroy;

  return VFE_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - vfe_ops_deinit -
 * DESCRIPTION:
 * DEPENDENCY: This function is called in vfe_client_init.
 *==========================================================================*/
vfe_status_t vfe_ops_deinit(vfe_client_t* vfe_client)
{
  CDBG_HIGH("%s: E", __func__);
  if(!vfe_client) {
    CDBG_ERROR("%s: vfe client is null", __func__);
    return -1;
  }
  memset(&(vfe_client->vfe_ops), 0 , sizeof(vfe_ops_t));
  return VFE_SUCCESS;
}
/*===========================================================================
 * FUNCTION    - vfe_stop_autofocus -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_stop_autofocus(vfe_ctrl_info_t *vfe_ctrl_obj)
{
  vfe_status_t status = VFE_SUCCESS;

  if(vfe_use_bayer_stats(vfe_ctrl_obj)){
    status = vfe_bf_stats_stop(&(vfe_ctrl_obj->vfe_module.stats.bf_stats),
      &(vfe_ctrl_obj->vfe_params));
    if (VFE_SUCCESS != status) {
      CDBG_HIGH("%s: failed", __func__);
      return VFE_ERROR_GENERAL;
    }
  }
  return status;
}

/*===========================================================================
 * FUNCTION    - vfe_release_stats -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: This function needs to be called after set_parms.
 *==========================================================================*/
vfe_status_t vfe_release_stats(vfe_ctrl_info_t *vfe_ctrl_obj)
{
  vfe_status_t status = VFE_SUCCESS;
  vfe_params_t *params = (vfe_params_t *)&(vfe_ctrl_obj->vfe_params);

  CDBG("%s: E \n", __func__);
  if (IS_BAYER_FORMAT(params))
    vfe_release_all_stats_buff(vfe_ctrl_obj);

  CDBG("%s: X \n", __func__);
  return status;
}

/*===========================================================================
 * FUNCTION    - vfe_set_stats_version -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: checks for the stats type enabled.
 *==========================================================================*/
vfe_status_t vfe_set_stats_version(vfe_ctrl_info_t *p_obj,
  struct msm_ver_num_info ver)
{
  p_obj->ver_num = ver;
  p_obj->vfe_module.stats.use_bayer_stats = vfe_use_bayer_stats(p_obj);


  return VFE_SUCCESS;
}
/*===========================================================================
 * FUNCTION    - vfe_use_bayer_stats -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: checks for the stats type enabled.
 *==========================================================================*/
uint8_t vfe_use_bayer_stats(vfe_ctrl_info_t *p_obj)
{
  return TRUE;
}
/*===========================================================================
 * FUNCTION    - vfe_get_stats_buf_dimension -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: gets the buffer dimension of stats output.
 *==========================================================================*/
vfe_status_t vfe_get_stats_buf_dimension(vfe_ctrl_info_t *p_obj, void* parm)
{
  vfe_status_t rc = VFE_SUCCESS;
  cam_stats_buf_dimension_t *stats_dim = (cam_stats_buf_dimension_t *)parm;
  switch (stats_dim->type) {
  case MSM_V4L2_EXT_CAPTURE_MODE_AEC:
    stats_dim->width = sizeof(vfe_stats_ae_struct_t);
    stats_dim->height = 1;
    break;
  case MSM_V4L2_EXT_CAPTURE_MODE_AF:
    stats_dim->width = sizeof(vfe_stats_af_struct_t);
    stats_dim->height = 1;
    break;
  case MSM_V4L2_EXT_CAPTURE_MODE_AWB:
    stats_dim->width = sizeof(vfe_stats_awb_struct_t);
    stats_dim->height = 1;
    break;
  case MSM_V4L2_EXT_CAPTURE_MODE_IHIST:
    stats_dim->width = sizeof(vfe_stats_ihist_struct_t);
    stats_dim->height = 1;
    break;
  case MSM_V4L2_EXT_CAPTURE_MODE_CS:
    stats_dim->width = sizeof(vfe_stats_cs_struct_t);
    stats_dim->height = 1;
    break;
  case MSM_V4L2_EXT_CAPTURE_MODE_RS:
    stats_dim->width = sizeof(vfe_stats_rs_struct_t);
    stats_dim->height = 1;
    break;
  default:
    CDBG_ERROR("%s: Invalid type : %d", __func__, stats_dim->type);
    break;
  }
  CDBG("%s: type: %d, width : %d, height: %d\n", __func__,
    stats_dim->type, stats_dim->width, stats_dim->height);
  return rc;
}
/*===========================================================================
 * FUNCTION    - vfe_set_stats_version -
 *
 * DESCRIPTION:
 *
 * DEPENDENCY: checks for the stats type enabled.
 *==========================================================================*/
vfe_status_t vfe_set_stats_streams(vfe_ctrl_info_t *p_obj,
  vfe_set_hal_stream *tmp)
{
  switch(tmp->type) {
  case MSM_STATS_TYPE_AWB:
    p_obj->vfe_module.stats.awb_stats.use_hal_buf = tmp->use_hal_buff;
    break;
  case MSM_STATS_TYPE_RS:
    p_obj->vfe_module.stats.rs_stats.use_hal_buf = tmp->use_hal_buff;
    break;
  case MSM_STATS_TYPE_CS:
    p_obj->vfe_module.stats.cs_stats.use_hal_buf = tmp->use_hal_buff;
    break;
  case MSM_STATS_TYPE_IHIST:
    p_obj->vfe_module.stats.ihist_stats.use_hal_buf = tmp->use_hal_buff;
    break;
  default:
    CDBG_ERROR("%s: Invalid stats type : %d\n", __func__, tmp->type);
    break;
  }
  return VFE_SUCCESS;
}
